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

"""Boot a Minkar disk image in QEMU and check the serial console.

The gate passes when every --expect marker shows up, in order, before the
timeout; it fails on a --fail marker (a kernel panic by default) or when
QEMU exits first. The full console goes to --log.

Exit status: 0 pass, 1 failure marker or early exit, 2 timeout.

Gates from the architecture plan:
  M0  the kernel prints its first message     (default markers)
  M1  the root volume mounts, PID 1 runs and starts a shell that answers
      a command typed on the console               (--gate M1)
  M2-login  launchd brings up getty; root logs in on the console with a
      password and the shell answers               (--gate M2-login)
  M2  M2-login, then the network: dhcpclient configures en0 (and tells
      configd, whose DNS configuration scutil shows), root logs
      in over ssh from the host with a key, and the guest resolves the
      host's name through DNS                      (--gate M2)

For M1 the kernel prints nothing once PID 1 starts, so PID 1 itself
(launchd, or a temporary init) must print USERLAND_MARKER on the console,
and the shell it starts SHELL_MARKER.
"""

import argparse
import os
import re
import selectors
import shutil
import socket
import subprocess
import sys
import tempfile
import time

# Printed by PID 1 (BSD/sbin/ravyninit until launchd runs) and by the
# shell it starts; keep in sync with ravyninit.c.
USERLAND_MARKER = 'Minkar: PID 1 running'
SHELL_MARKER = 'Minkar: shell running'


class Send(str):
    """A gate step that types a line on the console instead of waiting."""


class SSH(str):
    """A gate step that runs a command in the guest over ssh from the host.

    It is retried until the connection works (sshd may still be starting);
    its output is matched by the markers that follow, like the console's.
    """


# The shell echoes what it is sent, so the answer, not the command, is
# what the next marker matches: "Minkar: 42" only appears in its output.
SHELL_INPUT = [Send('echo "Minkar: $((6 * 7))"'), 'Minkar: 42']

# Console login through launchd, getty, login and PAM (part of gate M2).
# The image needs launchd as PID 1 (INIT=) and a root password
# (ROOT_PASSWORD=), which the test types: --password or $BOOTTEST_PASSWORD.
PASSWORD = Send('%PASSWORD%')

# Network (rest of gate M2): QEMU's DHCP server gives en0 10.0.2.15 and
# forwards the host's --ssh-port to the guest's port 22. The image needs
# the public half of --ssh-key in root's authorized_keys
# (ROOT_AUTHORIZED_KEYS=). configd must have the DNS configuration that
# dhcpclient published (scutil --dns: IPMonitor's dnsinfo, over XPC).
# Name resolution (IPMonitor's resolv.conf, mDNSResponder, getaddrinfo) is
# checked with the host's own name: QEMU forwards the guest's DNS queries
# to the host's resolver, which answers for it without Internet access.
HOSTNAME = '%HOSTNAME%'
SSH_INPUT = [SSH('echo "Minkar ssh: $((6 * 7))"; ipconfig getifaddr en0; '
                 'scutil --dns | grep "nameserver\\[0\\]" | head -1; '
                 'dscacheutil -q host -a name %HOSTNAME%'),
             'Minkar ssh: 42', '10.0.2.15', 'nameserver[0] : 10.0.2.3',
             'name: %HOSTNAME%', 'ip_address: ']

GATES = {
    'M0': ['Darwin Kernel Version'],
    'M1': ['Darwin Kernel Version', 'BSD root:', 'hfs: mounted', USERLAND_MARKER,
           SHELL_MARKER] + SHELL_INPUT,
    'M2-login': ['Darwin Kernel Version', 'BSD root:', 'hfs: mounted',
                 'launchd[1] has started up', 'login:', Send('root'), 'Password:',
                 PASSWORD, 'Welcome to Minkar.'] + SHELL_INPUT,
}
GATES['M2'] = GATES['M2-login'] + SSH_INPUT
GATE_FAIL = {
    'M2-login': ['Login incorrect'],
    'M2': ['Login incorrect'],
}
# How long the shell gets to print its prompt before input is sent.
SEND_DELAY = 3
# The password typed, never echoed in the test's own output.
SECRET = [None]
DEFAULT_FAIL = ['panic(cpu', 'Kernel panic', 'Debugger called:',
                'load_init_program: failed loading', 'Process 1 exec of']
ANSI = re.compile(rb'\x1b\[[0-9;?]*[A-Za-z]|\x1b[()][A-Z0-9]|\r')


def qemu_command(args, vars_copy):
    return [
        args.qemu, '-machine', 'q35', '-cpu', args.cpu,
        '-smp', str(args.smp), '-m', str(args.memory),
        '-drive', 'if=pflash,format=raw,readonly=on,file=%s' % args.ovmf_code,
        '-drive', 'if=pflash,format=raw,file=%s' % vars_copy,
        # The image is never written: tests start from the same state.
        '-drive', 'if=none,id=disk,format=raw,snapshot=on,file=%s' % args.image,
        '-device', 'ahci,id=ahci', '-device', 'ide-hd,drive=disk,bus=ahci.0',
        # The loader needs EFI_RNG_PROTOCOL; without entropy the kernel panics.
        '-device', 'virtio-rng-pci',
        # User-mode network (DHCP from QEMU at 10.0.2.2); the guest's ssh
        # port is forwarded to the host's --ssh-port.
        '-netdev', 'user,id=net0,hostfwd=tcp:127.0.0.1:%d-:22' % args.ssh_port,
        '-device', '%s,netdev=net0' % args.nic,
        '-serial', 'stdio', '-monitor', 'none', '-display', 'none',
        '-no-reboot',
    ]


def send_pending(proc, pending, start):
    """Type the Send steps at the head of pending on the serial console."""
    while pending and isinstance(pending[0], Send):
        time.sleep(SEND_DELAY)
        line = pending.pop(0)
        shown = '(password)' if line == SECRET[0] else line
        print('boottest: [%6.1fs] sent: %s' % (time.monotonic() - start, shown),
              file=sys.stderr)
        # A serial terminal ends lines with a carriage return.
        proc.stdin.write(line.encode() + b'\r')
        proc.stdin.flush()


def ssh_command(args, command):
    return ['ssh', '-i', args.ssh_key, '-p', str(args.ssh_port),
            '-o', 'StrictHostKeyChecking=no', '-o', 'UserKnownHostsFile=/dev/null',
            '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=10', '-o', 'LogLevel=ERROR',
            'root@127.0.0.1', command]


def run(args):
    expect = list(args.expect or GATES[args.gate])
    if PASSWORD in expect:
        if not args.password:
            print('boottest: gate %s needs --password or $BOOTTEST_PASSWORD' % args.gate,
                  file=sys.stderr)
            return 1
        expect = [Send(args.password) if m is PASSWORD else m for m in expect]
    if any(isinstance(m, SSH) for m in expect) and not (
            args.ssh_key and os.path.exists(args.ssh_key)):
        print('boottest: gate %s needs --ssh-key or $BOOTTEST_SSH_KEY' % args.gate,
              file=sys.stderr)
        return 1
    if any(HOSTNAME in m for m in expect):
        host = socket.gethostname().split('.')[0]
        try:
            socket.getaddrinfo(host, None)
        except OSError as e:
            print('boottest: gate %s resolves the host name %r in the guest, but the '
                  'host cannot resolve it either: %s' % (args.gate, host, e), file=sys.stderr)
            return 1
        expect = [type(m)(m.replace(HOSTNAME, host)) for m in expect]
    fail = args.fail if args.fail is not None else DEFAULT_FAIL + GATE_FAIL.get(args.gate, [])
    for path in (args.image, args.ovmf_code, args.ovmf_vars):
        if not os.path.exists(path):
            print('boottest: not found: %s' % path, file=sys.stderr)
            return 1

    tmp = tempfile.mkdtemp(prefix='minkar-boottest-')
    vars_copy = os.path.join(tmp, 'OVMF_VARS.fd')
    shutil.copyfile(args.ovmf_vars, vars_copy)
    cmd = qemu_command(args, vars_copy)
    print('boottest: ' + ' '.join(cmd), file=sys.stderr)

    log = open(args.log, 'wb') if args.log else None
    proc = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT)
    sel = selectors.DefaultSelector()
    sel.register(proc.stdout, selectors.EVENT_READ, 'console')

    start = time.monotonic()
    pending = list(expect)
    partial = {'console': b'', 'ssh': b''}
    ssh = None              # the running ssh, if any
    ssh_retry = 0.0         # when to start ssh again
    status, reason = 2, 'timed out after %ds' % args.timeout

    def start_ssh():
        print('boottest: [%6.1fs] ssh: %s' % (time.monotonic() - start, pending[0]),
              file=sys.stderr)
        p = subprocess.Popen(ssh_command(args, pending[0]), stdin=subprocess.DEVNULL,
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        sel.register(p.stdout, selectors.EVENT_READ, 'ssh')
        return p

    def match(text, complete):
        """Checks a line against the failure and expected markers."""
        nonlocal status, reason
        if complete and any(f in text for f in fail):
            status, reason = 1, 'failure marker: %s' % text.strip()
            return
        while pending and not isinstance(pending[0], SSH) and pending[0] in text:
            elapsed = time.monotonic() - start
            print('boottest: [%6.1fs] %s' % (elapsed, pending.pop(0)), file=sys.stderr)
            send_pending(proc, pending, start)

    try:
        while time.monotonic() - start < args.timeout:
            if pending and isinstance(pending[0], SSH) and ssh is None and \
                    time.monotonic() >= ssh_retry:
                ssh = start_ssh()
            events = sel.select(timeout=1)
            if not events and proc.poll() is not None:
                status, reason = 1, 'QEMU exited (status %d)' % proc.returncode
                break
            for key, _ in events:
                stream = key.data
                chunk = os.read(key.fileobj.fileno(), 4096)
                if not chunk:
                    if stream == 'console':
                        status, reason = 1, 'QEMU exited (status %s)' % proc.wait()
                        break
                    # ssh ended: if it worked, match its output; else retry.
                    sel.unregister(key.fileobj)
                    rc = ssh.wait()
                    ssh = None
                    out, partial['ssh'] = partial['ssh'], b''
                    if log:
                        log.write(b''.join(b'ssh| ' + l + b'\n' for l in out.splitlines()))
                        log.flush()
                    if rc != 0:
                        print('boottest: [%6.1fs] ssh failed (%d): %s' % (
                            time.monotonic() - start, rc,
                            out.decode('utf-8', 'replace').strip()[-200:]),
                            file=sys.stderr)
                        ssh_retry = time.monotonic() + 5
                        continue
                    pending.pop(0)
                    for text in ANSI.sub(b'', out).split(b'\n'):
                        match(text.decode('utf-8', 'replace'), True)
                    continue
                if stream == 'ssh':
                    partial['ssh'] += chunk     # matched when ssh exits
                    continue
                if log:
                    log.write(chunk)
                    log.flush()
                if args.verbose:
                    sys.stdout.buffer.write(chunk)
                    sys.stdout.flush()
                buf = partial[stream] + ANSI.sub(b'', chunk)
                *lines, partial[stream] = buf.split(b'\n')
                for text in lines:
                    match(text.decode('utf-8', 'replace'), True)
                    if status == 1:
                        break
                if status != 1:
                    match(partial[stream].decode('utf-8', 'replace'), False)
                if status == 1 or not pending:
                    break
            if status == 1 or not pending:
                if not pending:
                    status, reason = 0, 'all %d markers seen' % sum(
                        not isinstance(m, (Send, SSH)) for m in expect)
                break
    finally:
        if ssh is not None and ssh.poll() is None:
            ssh.kill()
        if proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                proc.kill()
        if log:
            log.close()
        shutil.rmtree(tmp, ignore_errors=True)

    verdict = {0: 'PASS', 1: 'FAIL', 2: 'TIMEOUT'}[status]
    print('boottest: %s: %s' % (verdict, reason), file=sys.stderr)
    if pending and status != 0:
        print('boottest: not seen: %s' % ', '.join(p for p in pending
                                                  if not isinstance(p, Send)),
              file=sys.stderr)
    return status


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument('--image', required=True)
    p.add_argument('--gate', choices=sorted(GATES), default='M0')
    p.add_argument('--expect', action='append',
                   help='marker that must appear, in order (repeatable; overrides --gate)')
    p.add_argument('--fail', action='append', help='marker that fails the test (repeatable)')
    p.add_argument('--password', default=os.environ.get('BOOTTEST_PASSWORD'),
                   help='root password for login gates (default: $BOOTTEST_PASSWORD)')
    p.add_argument('--timeout', type=int, default=900, help='seconds')
    p.add_argument('--log', help='write the whole console here')
    p.add_argument('--verbose', action='store_true', help='echo the console')
    p.add_argument('--qemu', default='qemu-system-x86_64')
    # Not "max": under TCG it reports an AMD family the kernel panics on.
    # Westmere is fully emulated by TCG (SSE4.2, POPCNT, no AVX).
    p.add_argument('--cpu', default='Westmere')
    p.add_argument('--smp', type=int, default=2)
    p.add_argument('--memory', type=int, default=4096, help='MiB')
    p.add_argument('--ssh-key', default=os.environ.get('BOOTTEST_SSH_KEY'),
                   help='private key ssh steps log in with (default: $BOOTTEST_SSH_KEY)')
    p.add_argument('--nic', default='e1000e',
                   help='QEMU network device: e1000e (82574L, MSI) or e1000 (82540EM, INTx)')
    p.add_argument('--ssh-port', type=int,
                   default=int(os.environ.get('BOOTTEST_SSH_PORT', '2222')),
                   help='host port forwarded to the guest\'s port 22')
    p.add_argument('--ovmf-code', default='/usr/share/OVMF/OVMF_CODE_4M.fd')
    p.add_argument('--ovmf-vars', default='/usr/share/OVMF/OVMF_VARS_4M.fd')
    args = p.parse_args(argv)
    SECRET[0] = args.password
    return run(args)


if __name__ == '__main__':
    sys.exit(main())
