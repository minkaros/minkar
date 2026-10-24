#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 The ravynOS contributors
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

"""Build a bootable ravynOS disk image on a Linux host.

Layout (GPT):
  1  EFI System Partition (FAT32)
       /EFI/BOOT/BOOTX64.EFI            ravynOS EFI loader
       /EFI/ravynOS/                    (looked up by the loader)
       /ravynOS/kernelcache             prelinked kernel + kexts
       /ravynOS/com.ravynos.boot.plist  boot-args, boot-file, boot-uuid
  2  Apple HFS+ root volume, populated from the sysroot

The root is found by the kernel through boot-uuid: AppleFileSystemDriver
derives a volume's UUID from the 8-byte identifier in the HFS+ volume
header's Finder info, so this script writes that identifier and puts the
matching UUID in the boot plist.

Needs root (loop devices and mounts), sgdisk, mkfs.fat, mkfs.hfsplus and
the Linux hfsplus driver.
"""

import argparse
import hashlib
import os
import plistlib
import shutil
import struct
import subprocess
import sys
import tempfile
import uuid

# Volume UUID namespace for file systems (kFSUUIDNamespaceSHA1 in <uuid/namespace.h>).
FS_UUID_NAMESPACE = uuid.UUID('B3E20F39-F292-11D6-97A4-00306543ECAC')

HFS_HEADER_OFFSET = 1024          # volume header, from the start of the volume
HFS_FINDER_INFO = 0x50            # finderInfo[32] inside the header
HFS_VOLUME_ID = HFS_FINDER_INFO + 24  # finderInfo[24..31]: volume identifier
HFS_SIGNATURES = (b'H+', b'HX')

MOUNT_POINTS = ('dev',)
LAUNCHD = '/sbin/launchd'

GPT_TYPE_EFI = 'EF00'
GPT_TYPE_HFS = 'AF00'             # 48465300-0000-11AA-AA11-00306543ECAC


def hfs_volume_uuid(volume_id):
    """The UUID macOS and AppleFileSystemDriver derive from an HFS+ volume id."""
    digest = bytearray(hashlib.md5(FS_UUID_NAMESPACE.bytes + volume_id).digest())
    digest[6] = 0x30 | (digest[6] & 0x0F)     # version 3 (name based, MD5)
    digest[8] = 0x80 | (digest[8] & 0x3F)     # RFC 4122 variant
    return uuid.UUID(bytes=bytes(digest))


def set_hfs_volume_id(device, volume_id):
    """Write the identifier into the primary and alternate volume headers."""
    with open(device, 'r+b') as dev:
        dev.seek(0, os.SEEK_END)
        size = dev.tell()
        for offset in (HFS_HEADER_OFFSET, size - HFS_HEADER_OFFSET):
            dev.seek(offset)
            header = dev.read(512)
            if header[:2] not in HFS_SIGNATURES:
                raise SystemExit('%s: no HFS+ volume header at %d' % (device, offset))
            dev.seek(offset + HFS_VOLUME_ID)
            dev.write(volume_id)


def case_collisions(tree):
    """Paths a case-insensitive volume cannot hold together with another.

    Returns (paths to leave out, conflicting directories). Of two names
    that differ only in case, a symlink alias gives way to a real file;
    of two files, the one whose name sorts first is kept, which matches
    what installing both on a macOS volume leaves behind. A directory
    clashing with anything is an error.
    """
    seen = {}
    drop, conflicts = [], []
    for dirpath, dirnames, filenames in os.walk(tree):
        dirnames.sort()
        for name in sorted(dirnames + filenames):
            path = os.path.relpath(os.path.join(dirpath, name), tree)
            key = path.casefold()
            if key not in seen:
                seen[key] = path
                continue
            other = seen[key]
            full, other_full = os.path.join(tree, path), os.path.join(tree, other)
            if (os.path.isdir(full) and not os.path.islink(full)) or \
               (os.path.isdir(other_full) and not os.path.islink(other_full)):
                conflicts.append((other, path))
            elif os.path.islink(full) or not os.path.islink(other_full):
                drop.append(path)
            else:
                drop.append(other)
                seen[key] = path
    return drop, conflicts


def copy_tree(src, dst):
    """Copy src into dst keeping owners, modes and symbolic links (needs root)."""
    drop, conflicts = case_collisions(src)
    if conflicts:
        raise SystemExit('names that differ only in case cannot share a volume:\n' +
                         '\n'.join('  %s / %s' % c for c in conflicts))
    for path in drop:
        print('warning: leaving out %s (differs only in case from another name)' % path)
    excludes = []
    for path in drop:
        excludes += ['--exclude', './' + path]
    # Hard links become separate files (about 6 MB more): Linux's hfsplus
    # driver makes link records without the link-chain flag Darwin's HFS
    # expects, so the kernel logged "hfs: set hardlink bit" on the console,
    # synchronously, at every lookup of one.
    pack = subprocess.Popen(['tar', '-C', src, '--hard-dereference', '-cf', '-'] +
                            excludes + ['.'],
                            stdout=subprocess.PIPE)
    unpack = subprocess.run(['tar', '-C', dst, '-xpf', '-'], stdin=pack.stdout)
    pack.stdout.close()
    if pack.wait() != 0 or unpack.returncode != 0:
        raise SystemExit('copying %s failed' % src)


def install_init(root_dir, init):
    """Make /sbin/launchd a link to init: a RELEASE kernel tries no other path."""
    if not os.path.isabs(init):
        raise SystemExit('--init takes an absolute path in the image: %s' % init)
    target = os.path.join(root_dir, init.lstrip('/'))
    if not os.path.isfile(target):
        raise SystemExit('--init: %s is not in the sysroot' % init)
    launchd = os.path.join(root_dir, LAUNCHD.lstrip('/'))
    if os.path.lexists(launchd):
        print('warning: %s replaced by a link to %s' % (LAUNCHD, init))
        os.unlink(launchd)
    os.symlink(init, launchd)


def set_root_password(root_dir, password):
    """Give root a SHA-512-crypt password in /etc/master.passwd (dev images)."""
    path = os.path.join(root_dir, 'private/etc/master.passwd')
    hashed = sha512_crypt(password)
    with open(path) as f:
        lines = f.readlines()
    for i, line in enumerate(lines):
        fields = line.split(':')
        if fields[0] == 'root' and len(fields) == 10:
            fields[1] = hashed
            lines[i] = ':'.join(fields)
            break
    else:
        raise SystemExit('%s: no root entry' % path)
    with open(path, 'w') as f:
        f.writelines(lines)
    os.chmod(path, 0o600)


def sha512_crypt(password):
    hashed = subprocess.run(['openssl', 'passwd', '-6', '-stdin'], input=password,
                            capture_output=True, text=True, check=True).stdout.strip()
    if not hashed.startswith('$6$'):
        raise SystemExit('openssl passwd -6 gave no SHA-512 hash')
    return hashed


def add_test_user(root_dir, name, password, uid=501, gid=20):
    """Add a login account for testing, like macOS's first user: uid 501,
    group staff, member of admin, home in /Users (dev images)."""
    etc = os.path.join(root_dir, 'private/etc')
    gecos, home, shell = name, '/Users/' + name, '/bin/sh'
    for fname, entry, mode in (
            ('master.passwd', '%s:%s:%d:%d::0:0:%s:%s:%s\n' % (
                name, sha512_crypt(password), uid, gid, gecos, home, shell), 0o600),
            ('passwd', '%s:*:%d:%d:%s:%s:%s\n' % (name, uid, gid, gecos, home, shell),
             0o644)):
        path = os.path.join(etc, fname)
        with open(path) as f:
            lines = [l for l in f if not l.startswith(name + ':')]
        lines.append(entry)
        with open(path, 'w') as f:
            f.writelines(lines)
        os.chmod(path, mode)
    path = os.path.join(etc, 'group')
    with open(path) as f:
        lines = f.readlines()
    for i, line in enumerate(lines):
        fields = line.rstrip('\n').split(':')
        if len(fields) == 4 and fields[0] == 'admin':
            members = [m for m in fields[3].split(',') if m]
            if name not in members:
                members.append(name)
            fields[3] = ','.join(members)
            lines[i] = ':'.join(fields) + '\n'
    with open(path, 'w') as f:
        f.writelines(lines)
    home_dir = os.path.join(root_dir, home.lstrip('/'))
    os.makedirs(home_dir, mode=0o755, exist_ok=True)
    os.chown(home_dir, uid, gid)


def permit_root_ssh(root_dir):
    """Let root log in over ssh with a password too (dev images, until 1.0)."""
    path = os.path.join(root_dir, 'private/etc/ssh/sshd_config')
    with open(path) as f:
        lines = [l for l in f if not l.startswith('PermitRootLogin ')]
    lines.append('# Development image: root may log in over ssh (until 1.0).\n'
                 'PermitRootLogin yes\n')
    with open(path, 'w') as f:
        f.writelines(lines)


def set_root_authorized_keys(root_dir, keys_file):
    """Install public keys root can log in with over ssh (dev images)."""
    with open(keys_file) as f:
        keys = f.read()
    ssh_dir = os.path.join(root_dir, 'private/var/root/.ssh')
    os.makedirs(ssh_dir, mode=0o700, exist_ok=True)
    os.chmod(ssh_dir, 0o700)
    path = os.path.join(ssh_dir, 'authorized_keys')
    with open(path, 'w') as f:
        f.write(keys if keys.endswith('\n') else keys + '\n')
    os.chmod(path, 0o600)
    for p in (ssh_dir, path):
        os.chown(p, 0, 0)


def run(*cmd, **kwargs):
    print('+ ' + ' '.join(cmd), flush=True)
    return subprocess.run(cmd, check=True, **kwargs)


def boot_plist(boot_args, volume_uuid):
    return plistlib.dumps({
        'boot-args': boot_args,
        'boot-file': '\\ravynOS\\kernelcache',
        'boot-uuid': str(volume_uuid).upper(),
    }, fmt=plistlib.FMT_XML)


def build(args):
    for path, what in ((args.sysroot, 'sysroot'), (args.kernelcache, 'kernelcache'),
                       (args.loader, 'loader')):
        if not os.path.exists(path):
            raise SystemExit('%s not found: %s' % (what, path))
    if os.geteuid() != 0:
        raise SystemExit('must run as root (loop devices and mounts)')

    volume_id = bytes.fromhex(args.volume_id) if args.volume_id else os.urandom(8)
    if len(volume_id) != 8:
        raise SystemExit('--volume-id takes 16 hex digits')
    volume_uuid = hfs_volume_uuid(volume_id)

    tmp_image = args.output + '.tmp'
    if os.path.exists(tmp_image):
        os.unlink(tmp_image)
    with open(tmp_image, 'wb') as f:
        f.truncate(args.size_mb * 1024 * 1024)

    run('sgdisk', '--zap-all', tmp_image, stdout=subprocess.DEVNULL)
    run('sgdisk',
        '-n', '1:2048:+%dM' % args.esp_mb, '-t', '1:' + GPT_TYPE_EFI, '-c', '1:EFI',
        '-n', '2:0:0', '-t', '2:' + GPT_TYPE_HFS, '-c', '2:' + args.volume_name,
        tmp_image, stdout=subprocess.DEVNULL)

    loop = run('losetup', '--find', '--show', '--partscan', tmp_image,
               capture_output=True, text=True).stdout.strip()
    esp, root = loop + 'p1', loop + 'p2'
    mounts = []
    try:
        run('udevadm', 'settle')
        run('mkfs.fat', '-F', '32', '-n', 'EFI', esp, stdout=subprocess.DEVNULL)
        run('mkfs.hfsplus', '-v', args.volume_name, root, stdout=subprocess.DEVNULL)
        set_hfs_volume_id(root, volume_id)

        workdir = tempfile.mkdtemp(prefix='ravynos-image-')
        esp_dir = os.path.join(workdir, 'esp')
        root_dir = os.path.join(workdir, 'root')
        os.mkdir(esp_dir)
        os.mkdir(root_dir)

        run('mount', '-t', 'vfat', esp, esp_dir)
        mounts.append(esp_dir)
        os.makedirs(os.path.join(esp_dir, 'EFI', 'BOOT'))
        os.makedirs(os.path.join(esp_dir, 'EFI', 'ravynOS'))
        os.makedirs(os.path.join(esp_dir, 'ravynOS'))
        shutil.copyfile(args.loader, os.path.join(esp_dir, 'EFI', 'BOOT', 'BOOTX64.EFI'))
        shutil.copyfile(args.kernelcache, os.path.join(esp_dir, 'ravynOS', 'kernelcache'))
        with open(os.path.join(esp_dir, 'ravynOS', 'com.ravynos.boot.plist'), 'wb') as f:
            f.write(boot_plist(args.boot_args, volume_uuid))

        run('mount', '-t', 'hfsplus', '-o', 'rw', root, root_dir)
        mounts.append(root_dir)
        # HFS+ stores BSD owners and modes; tar as root keeps them.
        copy_tree(args.sysroot, root_dir)
        # Mount points the kernel needs before userland runs (devfs on /dev).
        for mount_point in MOUNT_POINTS:
            path = os.path.join(root_dir, mount_point)
            if not os.path.lexists(path):
                os.makedirs(path, mode=0o555)
        if args.init:
            install_init(root_dir, args.init)
        if args.root_password:
            set_root_password(root_dir, args.root_password)
        if args.root_authorized_keys:
            set_root_authorized_keys(root_dir, args.root_authorized_keys)
        if args.test_user:
            if not args.test_user_password:
                raise SystemExit('--test-user needs --test-user-password')
            add_test_user(root_dir, args.test_user, args.test_user_password)
        if args.ssh_root_login:
            permit_root_ssh(root_dir)
        if args.launchd_debug:
            # launchd logs at debug level to the console when this exists.
            open(os.path.join(root_dir, 'private/var/db/.launchd_log_debug'), 'w').close()
    finally:
        for mount in reversed(mounts):
            subprocess.run(['umount', mount])
        subprocess.run(['losetup', '-d', loop])

    # Linux must not have changed the identifier while mounted.
    os.replace(tmp_image, args.output)
    print('image: %s' % args.output)
    print('root volume UUID: %s' % str(volume_uuid).upper())


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument('--sysroot', required=True, help='tree copied to the root volume')
    p.add_argument('--kernelcache', required=True)
    p.add_argument('--loader', required=True, help='loader.efi')
    p.add_argument('--output', required=True, help='raw disk image to write')
    p.add_argument('--size-mb', type=int, default=2048)
    p.add_argument('--esp-mb', type=int, default=200)
    p.add_argument('--volume-name', default='ravynOS')
    p.add_argument('--volume-id', help='HFS+ volume identifier, 16 hex digits (default random)')
    p.add_argument('--init', help='program to start as PID 1 instead of launchd, '
                   'e.g. /sbin/ravyninit (installed as a %s link)' % LAUNCHD)
    p.add_argument('--root-password', default=os.environ.get('ROOT_PASSWORD'),
                   help='set root\'s password (default: $ROOT_PASSWORD; else root has none '
                   'and cannot log in)')
    p.add_argument('--root-authorized-keys', default=os.environ.get('ROOT_AUTHORIZED_KEYS'),
                   help='file of public keys root can log in with over ssh '
                   '(default: $ROOT_AUTHORIZED_KEYS)')
    p.add_argument('--test-user', default=os.environ.get('TEST_USER'),
                   help='add a login account for testing (uid 501, admin; default: $TEST_USER)')
    p.add_argument('--test-user-password', default=os.environ.get('TEST_USER_PASSWORD'),
                   help='its password (default: $TEST_USER_PASSWORD)')
    p.add_argument('--ssh-root-login', action='store_true',
                   help='PermitRootLogin yes in sshd_config (development images)')
    p.add_argument('--launchd-debug', action='store_true',
                   help='make launchd log at debug level to the console')
    p.add_argument('--boot-args', default='-v debug=0x14e serial=3 keepsyms=1',
                   help='kernel command line')
    args = p.parse_args(argv)
    build(args)


if __name__ == '__main__':
    main()
