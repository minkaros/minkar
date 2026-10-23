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

"""Boot a ravynOS disk image in QEMU and check the serial console.

The gate passes when every --expect marker shows up, in order, before the
timeout; it fails on a --fail marker (a kernel panic by default) or when
QEMU exits first. The full console goes to --log.

Exit status: 0 pass, 1 failure marker or early exit, 2 timeout.

Gates from the architecture plan:
  M0  the kernel prints its first message     (default markers)
  M1  the root volume mounts and PID 1 runs    (--gate M1)

For M1 the kernel prints nothing once PID 1 starts, so PID 1 itself
(launchd, or a temporary init) must print USERLAND_MARKER on the console.
"""

import argparse
import os
import re
import selectors
import shutil
import subprocess
import sys
import tempfile
import time

USERLAND_MARKER = 'ravynOS: PID 1 running'
GATES = {
    'M0': ['Darwin Kernel Version'],
    'M1': ['Darwin Kernel Version', 'BSD root:', 'hfs: mounted', USERLAND_MARKER],
}
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
        '-serial', 'stdio', '-monitor', 'none', '-display', 'none',
        '-no-reboot',
    ]


def run(args):
    expect = list(args.expect or GATES[args.gate])
    fail = args.fail if args.fail is not None else DEFAULT_FAIL
    for path in (args.image, args.ovmf_code, args.ovmf_vars):
        if not os.path.exists(path):
            print('boottest: not found: %s' % path, file=sys.stderr)
            return 1

    tmp = tempfile.mkdtemp(prefix='ravynos-boottest-')
    vars_copy = os.path.join(tmp, 'OVMF_VARS.fd')
    shutil.copyfile(args.ovmf_vars, vars_copy)
    cmd = qemu_command(args, vars_copy)
    print('boottest: ' + ' '.join(cmd), file=sys.stderr)

    log = open(args.log, 'wb') if args.log else None
    proc = subprocess.Popen(cmd, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT)
    sel = selectors.DefaultSelector()
    sel.register(proc.stdout, selectors.EVENT_READ)

    start = time.monotonic()
    pending = list(expect)
    line = b''
    status, reason = 2, 'timed out after %ds' % args.timeout
    try:
        while time.monotonic() - start < args.timeout:
            if not sel.select(timeout=1):
                if proc.poll() is not None:
                    status, reason = 1, 'QEMU exited (status %d)' % proc.returncode
                    break
                continue
            chunk = os.read(proc.stdout.fileno(), 4096)
            if not chunk:
                status, reason = 1, 'QEMU exited (status %s)' % proc.wait()
                break
            if log:
                log.write(chunk)
                log.flush()
            if args.verbose:
                sys.stdout.buffer.write(chunk)
                sys.stdout.flush()
            line += ANSI.sub(b'', chunk)
            *lines, line = line.split(b'\n')
            for text, complete in [(l, True) for l in lines] + [(line, False)]:
                text = text.decode('utf-8', 'replace')
                # Failure markers wait for the whole line, to report all of it.
                if complete and any(f in text for f in fail):
                    status, reason = 1, 'failure marker: %s' % text.strip()
                    break
                while pending and pending[0] in text:
                    elapsed = time.monotonic() - start
                    print('boottest: [%6.1fs] %s' % (elapsed, pending.pop(0)),
                          file=sys.stderr)
            if status == 1 or not pending:
                if not pending:
                    status, reason = 0, 'all %d markers seen' % len(expect)
                break
    finally:
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
        print('boottest: not seen: %s' % ', '.join(pending), file=sys.stderr)
    return status


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument('--image', required=True)
    p.add_argument('--gate', choices=sorted(GATES), default='M0')
    p.add_argument('--expect', action='append',
                   help='marker that must appear, in order (repeatable; overrides --gate)')
    p.add_argument('--fail', action='append', help='marker that fails the test (repeatable)')
    p.add_argument('--timeout', type=int, default=900, help='seconds')
    p.add_argument('--log', help='write the whole console here')
    p.add_argument('--verbose', action='store_true', help='echo the console')
    p.add_argument('--qemu', default='qemu-system-x86_64')
    # Not "max": under TCG it reports an AMD family the kernel panics on.
    # Westmere is fully emulated by TCG (SSE4.2, POPCNT, no AVX).
    p.add_argument('--cpu', default='Westmere')
    p.add_argument('--smp', type=int, default=2)
    p.add_argument('--memory', type=int, default=4096, help='MiB')
    p.add_argument('--ovmf-code', default='/usr/share/OVMF/OVMF_CODE_4M.fd')
    p.add_argument('--ovmf-vars', default='/usr/share/OVMF/OVMF_VARS_4M.fd')
    return run(p.parse_args(argv))


if __name__ == '__main__':
    sys.exit(main())
