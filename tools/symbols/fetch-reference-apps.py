#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
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

"""Downloads the reference apps and unpacks them for symbol-inventory.py.

  fetch-reference-apps.py --dest DIR [--manifest reference-apps.tsv]

Each app in the manifest (a zip, a tar or a disk image: disk images need
7z for their HFS+ file system) is downloaded to DIR/downloads (kept, so a second
run downloads nothing), checked against its sha256 and unpacked into
DIR/apps/<name>/. A download whose checksum differs is deleted and the
run fails. Archives are unpacked without trusting them: no absolute
paths, no "..", no links that point outside the app's directory. The
apps are data: nothing here runs them.

Exit status: 0 when every app is in place, 1 otherwise.
"""

import argparse
import bz2
import hashlib
import lzma
import os
import plistlib
import shutil
import stat
import struct
import subprocess
import sys
import tarfile
import tempfile
import time
import urllib.error
import urllib.request
import zipfile
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))


def read_manifest(path):
    apps = []
    with open(path) as f:
        for n, line in enumerate(f, 1):
            line = line.rstrip('\n')
            if not line.strip() or line.startswith('#'):
                continue
            fields = line.split('\t')
            if len(fields) != 5:
                raise SystemExit('%s:%d: expected 5 tab-separated fields' % (path, n))
            name, version, minos, url, sha = fields
            apps.append({'name': name, 'version': version, 'minos': minos,
                         'url': url, 'sha256': sha.lower()})
    return apps


def sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def download(url, dest, attempts=4):
    """Fetch url to dest. GitHub's release downloads answer 502, 503 and
    504 now and then: those, and a dropped connection, are tried again
    after 5, 10 and 20 seconds."""
    tmp = dest + '.part'
    for attempt in range(attempts):
        try:
            with urllib.request.urlopen(url, timeout=120) as r, open(tmp, 'wb') as f:
                shutil.copyfileobj(r, f)
            os.replace(tmp, dest)
            return
        except urllib.error.HTTPError as e:
            if e.code not in (429, 500, 502, 503, 504) or attempt == attempts - 1:
                raise
        except (urllib.error.URLError, TimeoutError, ConnectionError):
            if attempt == attempts - 1:
                raise
        time.sleep(5 * 2 ** attempt)


def inside(root, path):
    root = os.path.realpath(root)
    return os.path.commonpath([root, os.path.realpath(path)]) == root


def safe_member(name):
    return not (name.startswith('/') or name.startswith('\\') or
                '..' in name.replace('\\', '/').split('/'))


def unzip(archive, dest):
    with zipfile.ZipFile(archive) as z:
        for info in z.infolist():
            if not safe_member(info.filename):
                raise ValueError('unsafe path in archive: %s' % info.filename)
            target = os.path.join(dest, info.filename)
            mode = info.external_attr >> 16
            if info.is_dir():
                os.makedirs(target, exist_ok=True)
                continue
            os.makedirs(os.path.dirname(target), exist_ok=True)
            if stat.S_ISLNK(mode):
                link = z.read(info).decode()
                resolved = os.path.join(os.path.dirname(target), link)
                if link.startswith('/') or not inside(dest, resolved):
                    raise ValueError('link out of the archive: %s -> %s'
                                     % (info.filename, link))
                os.symlink(link, target)
            else:
                with z.open(info) as src, open(target, 'wb') as out:
                    shutil.copyfileobj(src, out)
                os.chmod(target, (mode & 0o777) or 0o644)


def untar(archive, dest):
    with tarfile.open(archive) as t:
        for m in t.getmembers():
            if not safe_member(m.name):
                raise ValueError('unsafe path in archive: %s' % m.name)
        # The "data" filter refuses links out of dest, devices and the like.
        t.extractall(dest, filter='data')


def is_dmg(archive):
    with open(archive, 'rb') as f:
        f.seek(0, 2)
        if f.tell() < 512:
            return False
        f.seek(-512, 2)
        return f.read(4) == b'koly'


def dmg_partitions(archive, workdir):
    """A UDIF disk image's partitions, decompressed to files in workdir
    (raw, zero, zlib, bzip2 and LZMA chunks), the largest first."""
    images = []
    with open(archive, 'rb') as f:
        f.seek(-512, 2)
        koly = f.read(512)
        data_offset = struct.unpack('>Q', koly[0x18:0x20])[0]
        xml_offset, xml_length = struct.unpack('>QQ', koly[0xd8:0xe8])
        f.seek(xml_offset)
        blocks = plistlib.loads(f.read(xml_length))['resource-fork']['blkx']
        for n, block in enumerate(blocks):
            mish = block['Data']
            count = struct.unpack('>I', mish[0xc8:0xcc])[0]
            path = os.path.join(workdir, '%d.img' % n)
            with open(path, 'wb') as out:
                for i in range(count):
                    kind, _, _, sectors, offset, length = struct.unpack(
                        '>IIQQQQ', mish[0xcc + 40 * i:0xcc + 40 * (i + 1)])
                    if kind == 0xffffffff:
                        break
                    if kind == 0x7ffffffe:          # a comment
                        continue
                    size = sectors * 512
                    if kind in (0, 2):              # zeros
                        out.write(b'\0' * size)
                        continue
                    f.seek(data_offset + offset)
                    raw = f.read(length)
                    if kind == 1:
                        data = raw
                    elif kind == 0x80000005:
                        data = zlib.decompress(raw)
                    elif kind == 0x80000006:
                        data = bz2.decompress(raw)
                    elif kind == 0x80000008:
                        data = lzma.decompress(raw)
                    else:
                        raise ValueError('disk image chunk type %#x not supported' % kind)
                    out.write(data[:size].ljust(size, b'\0'))
            images.append(path)
    return sorted(images, key=os.path.getsize, reverse=True)


def undmg(archive, dest):
    """A disk image's file system, through 7z (HFS+), into dest; links that
    leave dest (the usual one to /Applications) are dropped."""
    with tempfile.TemporaryDirectory() as work:
        for image in dmg_partitions(archive, work):
            tree = os.path.join(work, 'tree')
            # 7z warns (and exits 2) for parts of an HFS+ volume it skips,
            # the volume's own metadata: what it unpacked is what counts.
            subprocess.run(['7z', 'x', '-y', '-o' + tree, image],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            if os.path.isdir(tree) and any(n.endswith('.app') for _, d, _ in os.walk(tree) for n in d):
                break
            shutil.rmtree(tree, ignore_errors=True)
        else:
            raise ValueError('no file system 7z can read in the disk image')
        for root, dirs, files in os.walk(tree):
            for name in dirs + files:
                path = os.path.join(root, name)
                # 7z writes extended attributes as files beside theirs.
                if ':com.apple.' in name and not os.path.isdir(path):
                    os.unlink(path)
                    continue
                if os.path.islink(path):
                    link = os.readlink(path)
                    if link.startswith('/') or not inside(tree, os.path.join(root, link)):
                        os.unlink(path)
        for name in os.listdir(tree):
            shutil.move(os.path.join(tree, name), os.path.join(dest, name))


def unpack(archive, dest):
    if zipfile.is_zipfile(archive):
        unzip(archive, dest)
    elif tarfile.is_tarfile(archive):
        untar(archive, dest)
    elif is_dmg(archive):
        undmg(archive, dest)
    else:
        raise ValueError('not a zip or tar archive or a disk image')


def fetch(app, dest, cache=None):
    downloads = os.path.join(dest, 'downloads')
    os.makedirs(downloads, exist_ok=True)
    archive = os.path.join(downloads, '%s-%s%s' % (
        app['name'], app['version'],
        '.zip' if app['url'].endswith('.zip') else
        '.dmg' if app['url'].endswith('.dmg') else '.tar'))
    cached = os.path.join(cache, os.path.basename(archive)) if cache else None
    if not os.path.exists(archive) or sha256(archive) != app['sha256']:
        if cached and os.path.exists(cached) and sha256(cached) == app['sha256']:
            shutil.copyfile(cached, archive)
        else:
            download(app['url'], archive)
    got = sha256(archive)
    if got != app['sha256']:
        os.unlink(archive)
        raise ValueError('sha256 %s, expected %s' % (got, app['sha256']))
    if cached and not os.path.exists(cached):
        try:
            os.makedirs(cache, exist_ok=True)
            shutil.copyfile(archive, cached + '.part')
            os.replace(cached + '.part', cached)
        except OSError:
            pass                    # a cache that cannot be written is no cache
    target = os.path.join(dest, 'apps', app['name'])
    if os.path.isdir(target):
        shutil.rmtree(target)
    os.makedirs(target)
    unpack(archive, target)
    return target


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument('--dest', required=True, help='download and unpack here')
    p.add_argument('--manifest', default=os.path.join(HERE, 'reference-apps.tsv'))
    p.add_argument('--cache', default=os.environ.get('MINKAR_DOWNLOAD_CACHE') or os.path.join(
                       os.environ.get('XDG_CACHE_HOME') or os.path.expanduser('~/.cache'), 'minkar', 'downloads'),
                   help='keep the archives here (checked by SHA-256), so a failed download does not'
                        ' fail the report; "" for none (default: $MINKAR_DOWNLOAD_CACHE or'
                        ' ~/.cache/minkar/downloads)')
    args = p.parse_args(argv)

    ok = True
    for app in read_manifest(args.manifest):
        try:
            target = fetch(app, args.dest, args.cache or None)
            print('fetch-reference-apps: %s %s -> %s' % (app['name'], app['version'], target))
        except Exception as e:      # report every app, then fail
            ok = False
            print('fetch-reference-apps: %s %s: %s' % (app['name'], app['version'], e),
                  file=sys.stderr)
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
