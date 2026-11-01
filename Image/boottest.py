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
      in over ssh from the host with a key, the guest resolves the
      host's name through DNS, and the code integrity policy marks a
      binary of the trust cache as a platform binary and refuses a copy
      re-signed outside it                         (--gate M2)
  M4-draw  M2, then WindowServer (its launchd job enabled over ssh) and
      its demo client: the screen, taken through QMP, matches a
      reference image within a tolerance, and a click (USB tablet) and a
      key (PS/2 keyboard) injected through QMP reach the demo's window as
      NSEvents                                     (--gate M4-draw)
  M4-compose  M2, then the new window server (SkyLight) and the demo's two
      overlapping AppKit windows: the screen matches a reference image,
      again after a window is raised and after one is moved; a click and
      a key reach a window as NSEvents, a window is dragged by its title
      bar (and AppKit is told its new frame); wsctl shows no frame while
      nothing changes                              (--gate M4-compose)

For M1 the kernel prints nothing once PID 1 starts, so PID 1 itself
(launchd, or a temporary init) must print USERLAND_MARKER on the console,
and the shell it starts SHELL_MARKER.
"""

import argparse
import gzip
import json
import os
import re
import selectors
import shutil
import socket
import subprocess
import sys
import tempfile
import time

# Printed by PID 1 (BSD/sbin/minkarinit until launchd runs) and by the
# shell it starts; keep in sync with minkarinit.c.
USERLAND_MARKER = 'Minkar: PID 1 running'
SHELL_MARKER = 'Minkar: shell running'


class Send(str):
    """A gate step that types a line on the console instead of waiting."""


class Act(str):
    """A gate step done on the host through QEMU's QMP: "compare [NAME]"
    (the screen against the reference image: --reference, or
    reference/NAME.ppm.gz), "click X Y" (screen pixels) or "key NAME" (a
    QEMU key code). Its result lines are matched by the markers that
    follow."""


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
# Code integrity (Docs/ADR/0001-Kernel-Entitlements.md): cstest in the
# trust cache is a platform binary; a copy re-signed outside it with the
# same restricted entitlement is killed at exec (137: SIGKILL), and an
# ad-hoc copy without it is refused by IOHIDSystem as its server.
CS_OUTSIDE = '/usr/local/tests/codesign/outside'
CODESIGN_INPUT = [SSH('dmesg | grep "MinkarCodeIntegrity: trust cache loaded"; '
                      '/usr/local/bin/cstest status; '
                      '%s/cstest-resigned status; echo "boottest: resigned rc=$?"; '
                      '%s/cstest-adhoc hid' % (CS_OUTSIDE, CS_OUTSIDE)),
                  'MinkarCodeIntegrity: trust cache loaded',
                  'valid=1 adhoc=0 signed=1 platform=1',
                  'boottest: resigned rc=137', 'refused']
GATES['M2'] = GATES['M2-login'] + SSH_INPUT + CODESIGN_INPUT

# Gate M4-draw (WindowServer plan, WS0): WindowServer's job is installed
# disabled; enabled over ssh, it takes the screen. Its demo client opens
# one window at a fixed place, so the screen can be compared with
# REFERENCE (--update-reference writes it from a good run).
WS_JOB = '/System/Library/LaunchDaemons/org.minkar.WindowServer.plist'
WS_DEMO = '/System/Library/CoreServices/WindowServerDemo.app/Contents/MacOS/WindowServerDemo'
REFERENCE = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'reference',
                         'M4-draw.ppm.gz')
# A pixel differs when a channel differs by more than PIXEL_TOLERANCE; the
# screen matches when at most SCREEN_TOLERANCE of its pixels differ.
PIXEL_TOLERANCE = 32
SCREEN_TOLERANCE = 0.005
GATES['M4-draw'] = GATES['M2'] + [
    SSH('launchctl load -w %s; for i in $(seq 60); do grep -q "IOFramebuffer: display" '
        '/var/log/WindowServer.log 2>/dev/null && break; sleep 1; done; '
        'grep "IOFramebuffer: display" /var/log/WindowServer.log' % WS_JOB),
    'IOFramebuffer: display',
    SSH('cd /; (nohup %s > /tmp/demo.out 2>&1 < /dev/null &); for i in $(seq 60); do '
        'grep -q "^window frame=" /tmp/demo.out && break; sleep 1; done; sleep 3; '
        'grep "^window frame=" /tmp/demo.out' % WS_DEMO),
    'window frame=198,197,402,332',
    Act('compare'), 'boottest-draw: the screen matches the reference',
    # The middle of the demo's content (its frame is 198,197,402,332 from
    # the bottom left, on a 1024x768 screen).
    Act('click 399 405'), Act('key a'),
    SSH('sleep 2; cat /tmp/demo.out'),
    'event mouseDown x=', 'event mouseUp', 'event keyDown keyCode=0 chars=a',
]
# Gate M4-compose (WindowServer plan, WS2): the new server, started by
# hand (its job comes in WS4), composites the demo's two AppKit windows
# (WindowServerDemo -two); the demo takes commands from a file, as there
# is no input through the new server before WS3.
WS_NEXT = ('/System/Library/PrivateFrameworks/SkyLight.framework/Versions/A/Resources/'
           'WindowServer')
# Written whole, then renamed: the demo must not read a half-written file.
DEMO_COMMAND = ('echo "%s" > /tmp/ws-demo.cmd.new; mv /tmp/ws-demo.cmd.new /tmp/ws-demo.cmd; '
                'for i in $(seq 100); do '
                '[ -e /tmp/ws-demo.cmd ] || break; sleep 0.1; done; sleep 1; echo "%s"')
GATES['M4-compose'] = GATES['M2'] + [
    SSH('cd /; (nohup %s > /tmp/ws2.log 2>&1 < /dev/null &); for i in $(seq 60); do '
        'grep -q "^WindowServer: serving" /tmp/ws2.log && break; sleep 1; done; '
        'cat /tmp/ws2.log' % WS_NEXT),
    'WindowServer: serving org.minkar.WindowServer.next',
    SSH('cd /; (nohup %s -two > /tmp/demo.out 2>&1 < /dev/null &); for i in $(seq 60); do '
        'grep -q "^second frame=" /tmp/demo.out && break; sleep 1; done; sleep 3; '
        'grep "frame=" /tmp/demo.out' % WS_DEMO),
    'window frame=198,197,402,332', 'second frame=448,97,302,282',
    Act('compare M4-compose-1'), 'boottest-draw: the screen matches the reference',
    SSH(DEMO_COMMAND % ('raise-blue', 'boottest: raised')), 'boottest: raised',
    Act('compare M4-compose-2'), 'boottest-draw: the screen matches the reference',
    SSH(DEMO_COMMAND % ('move-red 600 400', 'boottest: moved')), 'boottest: moved',
    Act('compare M4-compose-3'), 'boottest-draw: the screen matches the reference',
    # Input (WS3): a click and a key reach the blue window (the middle of
    # its content), and the red one is dragged by its title bar.
    Act('click 399 405'), Act('key a'),
    SSH('sleep 2; grep event /tmp/demo.out'),
    'event mouseDown x=', 'event mouseUp', 'event keyDown keyCode=0 chars=a',
    Act('drag 700 100 600 300'),
    SSH('sleep 2; grep "second moved" /tmp/demo.out | tail -1'),
    'second moved frame=500,200,302,282',
    Act('compare M4-compose-4'), 'boottest-draw: the screen matches the reference',
    SSH('a=$(/usr/local/bin/wsctl stats | awk \'/^frames/ { print $2 }\'); sleep 2; '
        'b=$(/usr/local/bin/wsctl stats | awk \'/^frames/ { print $2 }\'); '
        '[ "$a" = "$b" ] && echo "boottest: no frame while idle ($a)"'),
    'boottest: no frame while idle',
]
GATE_FAIL = {
    'M2-login': ['Login incorrect'],
    'M2': ['Login incorrect'],
    'M4-draw': ['Login incorrect', 'boottest-draw: the screen differs'],
    'M4-compose': ['Login incorrect', 'boottest-draw: the screen differs'],
}
# How long the shell gets to print its prompt before input is sent.
SEND_DELAY = 3
# The password typed, never echoed in the test's own output.
SECRET = [None]
PANIC_MARKERS = ['panic(cpu', 'Kernel panic', 'Debugger called:']
DEFAULT_FAIL = PANIC_MARKERS + ['load_init_program: failed loading', 'Process 1 exec of']
# After a panic the console is read on for the rest of the report (its
# backtrace comes after the marker), up to this many seconds.
PANIC_DRAIN = 30
PANIC_REPORT_END = (b'panic.apple.com', b'Attempting to commit panic log')
PANIC_REPORT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'tools',
                            'panic-report.py')
ANSI = re.compile(rb'\x1b\[[0-9;?]*[A-Za-z]|\x1b[()][A-Z0-9]|\r')


def accel(args):
    # KVM when the host has it and we may open it, else TCG (as in CI).
    if args.accel != 'auto':
        return args.accel
    return 'kvm' if os.access('/dev/kvm', os.R_OK | os.W_OK) else 'tcg'


def qemu_command(args, vars_copy):
    # Disks before the image (--extra-disk) take the first AHCI ports, as
    # other operating systems' disks do on a PC: the firmware finds their
    # EFI partitions first.
    disks = []
    for i, extra in enumerate(args.extra_disk or []):
        disks += ['-drive', 'if=none,id=extra%d,format=raw,snapshot=on,file=%s' % (i, extra),
                  '-device', 'ide-hd,drive=extra%d,bus=ahci.%d' % (i, i)]
    port = len(args.extra_disk or [])
    return [
        args.qemu, '-machine', 'q35', '-accel', accel(args), '-cpu', args.cpu,
        '-smp', str(args.smp), '-m', str(args.memory),
        '-drive', 'if=pflash,format=raw,readonly=on,file=%s' % args.ovmf_code,
        '-drive', 'if=pflash,format=raw,file=%s' % vars_copy,
        '-device', 'ahci,id=ahci',
    ] + disks + [
        # The image is never written: tests start from the same state.
        '-drive', 'if=none,id=disk,format=raw,snapshot=on,file=%s' % args.image,
    ] + (['-device', 'nvme,serial=MINKAR0001,drive=disk'] if args.disk == 'nvme' else
         ['-device', 'ide-hd,drive=disk,bus=ahci.%d' % port]) + ([] if args.no_rng else [
        # Firmware RNG for the loader; --no-rng leaves it out, as on PCs
        # whose firmware has none (the loader then uses RDRAND).
        '-device', 'virtio-rng-pci',
    ]) + [
        # User-mode network (DHCP from QEMU at 10.0.2.2); the guest's ssh
        # port is forwarded to the host's --ssh-port.
        '-netdev', 'user,id=net0,hostfwd=tcp:127.0.0.1:%d-:22' % args.ssh_port,
        '-device', '%s,netdev=net0' % args.nic,
        '-serial', 'stdio', '-monitor', 'none', '-display', 'none',
        '-no-reboot',
    ] + ([] if args.qmp is None else [
        # Gate M4-draw: a USB tablet for absolute pointer positions, and QMP
        # to take the screen and inject input.
        '-device', 'qemu-xhci,id=xhci', '-device', 'usb-tablet,bus=xhci.0',
        '-qmp', 'unix:%s,server,nowait' % args.qmp,
    ])


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


def drain_panic(proc, sel, console, log, verbose):
    """Reads the console on until the panic report ends (or PANIC_DRAIN)."""
    end = time.monotonic() + PANIC_DRAIN
    while time.monotonic() < end and proc.poll() is None:
        for key, _ in sel.select(timeout=1):
            if key.data != 'console':
                continue
            chunk = os.read(key.fileobj.fileno(), 4096)
            if not chunk:
                return
            console += chunk
            if log:
                log.write(chunk)
                log.flush()
            if verbose:
                sys.stdout.buffer.write(chunk)
                sys.stdout.flush()
            if any(m in console[-8192:] for m in PANIC_REPORT_END):
                # The rest is a line or two: take them too.
                end = min(end, time.monotonic() + 2)


def print_panic_report(args, console):
    """The panic's backtrace with function names (tools/panic-report.py)."""
    if not args.kernel:
        print('boottest: no --kernel: the panic backtrace is not symbolicated', file=sys.stderr)
        return
    with tempfile.NamedTemporaryFile(prefix='minkar-panic-', suffix='.log') as f:
        f.write(bytes(console))
        f.flush()
        cmd = [sys.executable, PANIC_REPORT, '--kernel', args.kernel]
        if args.kexts:
            cmd += ['--kexts', args.kexts]
        r = subprocess.run(cmd + [f.name], capture_output=True, text=True)
    print('boottest: panic report:', file=sys.stderr)
    for line in (r.stdout + r.stderr).splitlines():
        print('boottest: | ' + line, file=sys.stderr)


def qmp(args, *commands):
    """Runs QMP commands (name, arguments) and returns their results."""
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(args.qmp)
    f = s.makefile('rw')
    f.readline()
    results = []
    for name, arguments in (('qmp_capabilities', None),) + commands:
        msg = {'execute': name}
        if arguments is not None:
            msg['arguments'] = arguments
        f.write(json.dumps(msg) + '\n')
        f.flush()
        while True:
            r = json.loads(f.readline())
            if 'return' in r or 'error' in r:
                if 'error' in r:
                    raise RuntimeError('QMP %s: %s' % (name, r['error']))
                results.append(r['return'])
                break
    s.close()
    return results[1:]


def read_ppm(data):
    """Width, height and RGB bytes of a binary PPM (P6, 8 bits)."""
    parts = data.split(b'\n', 3)
    w, h = map(int, parts[1].split())
    return w, h, parts[3]


def act(args, step, tmp):
    """Does an Act step; returns the lines its markers are matched in."""
    words = step.split()
    if words[0] == 'compare':
        name = words[1] if len(words) > 1 else None
        reference = (os.path.join(os.path.dirname(REFERENCE), name + '.ppm.gz') if name
                     else args.reference)
        shot = os.path.join(tmp, 'screen.ppm')
        qmp(args, ('screendump', {'filename': shot}))
        time.sleep(1)
        with open(shot, 'rb') as f:
            data = f.read()
        if args.log:
            shutil.copyfile(shot, args.log + ('.%s' % name if name else '') + '.screen.ppm')
        if args.update_reference:
            os.makedirs(os.path.dirname(reference), exist_ok=True)
            with gzip.open(reference, 'wb') as f:
                f.write(data)
            return ['boottest-draw: the screen matches the reference (written to %s)'
                    % reference]
        with gzip.open(reference, 'rb') as f:
            rw, rh, ref = read_ppm(f.read())
        w, h, px = read_ppm(data)
        if (w, h) != (rw, rh):
            return ['boottest-draw: the screen differs: %dx%d, the reference %dx%d'
                    % (w, h, rw, rh)]
        differ = sum(1 for i in range(0, len(px), 3)
                     if max(abs(px[i + c] - ref[i + c]) for c in range(3)) > PIXEL_TOLERANCE)
        verdict = 'matches' if differ <= SCREEN_TOLERANCE * w * h else 'differs'
        return ['boottest-draw: the screen %s the reference (%d of %d pixels differ)'
                % (verdict, differ, w * h)]
    if words[0] in ('click', 'drag'):
        # The tablet spans 0..32767 on each axis of the screen.
        shot = os.path.join(tmp, 'size.ppm')
        qmp(args, ('screendump', {'filename': shot}))
        time.sleep(1)
        with open(shot, 'rb') as f:
            w, h, _ = read_ppm(f.read())
        points = [(int(words[i]) * 32767 // w, int(words[i + 1]) * 32767 // h)
                  for i in range(1, len(words) - 1, 2)]

        def send(x, y, down):
            events = [{'type': 'abs', 'data': {'axis': 'x', 'value': x}},
                      {'type': 'abs', 'data': {'axis': 'y', 'value': y}}]
            if down is not None:
                events.append({'type': 'btn', 'data': {'down': down, 'button': 'left'}})
            qmp(args, ('input-send-event', {'events': events}))
            time.sleep(0.5)

        (x, y), (x2, y2) = points[0], points[-1]
        send(x, y, None)
        send(x, y, True)
        if words[0] == 'drag':
            # In steps, as a hand moves: the window follows each one.
            for k in range(1, 5):
                send(x + (x2 - x) * k // 4, y + (y2 - y) * k // 4, None)
        send(x2, y2, False)
        return ['boottest-draw: %s at %s' % ('dragged' if words[0] == 'drag' else 'clicked',
                                            ' '.join(words[1:]))]
    if words[0] == 'key':
        qmp(args, ('send-key', {'keys': [{'type': 'qcode', 'data': words[1]}]}))
        time.sleep(0.5)
        return ['boottest-draw: typed %s' % words[1]]
    raise ValueError('unknown step: %s' % step)


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
    args.qmp = os.path.join(tmp, 'qmp.sock') if any(
        isinstance(m, Act) for m in expect) else None
    vars_copy = os.path.join(tmp, 'OVMF_VARS.fd')
    shutil.copyfile(args.ovmf_vars, vars_copy)
    cmd = qemu_command(args, vars_copy)
    print('boottest: ' + ' '.join(cmd), file=sys.stderr)

    log = open(args.log, 'wb') if args.log else None
    console = bytearray()   # the whole console, for a panic report
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
            while pending and isinstance(pending[0], Act) and status != 1:
                step = pending.pop(0)
                print('boottest: [%6.1fs] qmp: %s' % (time.monotonic() - start, step),
                      file=sys.stderr)
                for text in act(args, step, tmp):
                    print('boottest: [%6.1fs] %s' % (time.monotonic() - start, text),
                          file=sys.stderr)
                    if log:
                        log.write(b'qmp| ' + text.encode() + b'\n')
                        log.flush()
                    match(text, True)
            if status == 1 or (not pending and expect):
                if not pending:
                    status, reason = 0, 'all %d markers seen' % sum(
                        not isinstance(m, (Send, SSH, Act)) for m in expect)
                break
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
                console += chunk
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
                        not isinstance(m, (Send, SSH, Act)) for m in expect)
                break
        if status == 1 and any(m in reason for m in PANIC_MARKERS):
            drain_panic(proc, sel, console, log, args.verbose)
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
    if status == 1 and any(m in reason for m in PANIC_MARKERS):
        print_panic_report(args, console)
    if pending and status != 0:
        print('boottest: not seen: %s' % ', '.join(p for p in pending
                                                  if not isinstance(p, (Send, Act))),
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
    p.add_argument('--kernel', default=os.environ.get('BOOTTEST_KERNEL'),
                   help='the unstripped kernel of the image: a panic is then printed with '
                        'function names (or $BOOTTEST_KERNEL)')
    p.add_argument('--kexts', default=os.environ.get('BOOTTEST_KEXTS'),
                   help="a directory with the image's kexts, for frames in kexts")
    p.add_argument('--verbose', action='store_true', help='echo the console')
    p.add_argument('--qemu', default='qemu-system-x86_64')
    # Not "max": under TCG it reports an AMD family the kernel panics on.
    # Westmere is fully emulated by TCG (SSE4.2, POPCNT, no AVX).
    p.add_argument('--cpu', default='Westmere')
    # TCG by default: on CI runners /dev/kvm opens too, and the kernel
    # does not boot under their KVM yet (it does under KVM nested in
    # VMware); ask for kvm or auto where it is known to work.
    p.add_argument('--accel', default=os.environ.get('BOOTTEST_ACCEL', 'tcg'),
                   choices=['auto', 'kvm', 'tcg'],
                   help='kvm, tcg, or auto: kvm when /dev/kvm can be opened '
                        '(default: $BOOTTEST_ACCEL or tcg)')
    p.add_argument('--extra-disk', action='append',
                   help='raw disk attached before the image (repeatable)')
    p.add_argument('--disk', choices=['ahci', 'nvme'],
                   default=os.environ.get('BOOTTEST_DISK', 'ahci'),
                   help='how the image is attached: a SATA disk on AHCI, or an '
                        'NVMe SSD (default: $BOOTTEST_DISK or ahci)')
    p.add_argument('--no-rng', action='store_true',
                   help='no virtio-rng: firmware without EFI_RNG_PROTOCOL')
    # A count, or a QEMU topology (e.g. 24,sockets=1,cores=12,threads=2).
    p.add_argument('--smp', default='2')
    p.add_argument('--memory', type=int, default=4096, help='MiB')
    p.add_argument('--ssh-key', default=os.environ.get('BOOTTEST_SSH_KEY'),
                   help='private key ssh steps log in with (default: $BOOTTEST_SSH_KEY)')
    p.add_argument('--nic', default='e1000e',
                   help='QEMU network device: e1000e (82574L, MSI) or e1000 (82540EM, INTx)')
    p.add_argument('--ssh-port', type=int,
                   default=int(os.environ.get('BOOTTEST_SSH_PORT', '2222')),
                   help='host port forwarded to the guest\'s port 22')
    p.add_argument('--reference', default=REFERENCE,
                   help='reference screen for gate M4-draw (gzip-compressed PPM)')
    p.add_argument('--update-reference', action='store_true',
                   help='gates M4-draw and M4-compose: write the screens to their '
                        'references instead of comparing them (from a run checked by hand)')
    p.add_argument('--ovmf-code', default='/usr/share/OVMF/OVMF_CODE_4M.fd')
    p.add_argument('--ovmf-vars', default='/usr/share/OVMF/OVMF_VARS_4M.fd')
    args = p.parse_args(argv)
    SECRET[0] = args.password
    return run(args)


if __name__ == '__main__':
    sys.exit(main())
