#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
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

"""
dcn302-snapshot.py: the display engine registers of Navi 23 (DCN 3.0.2)
that hold a lit display's state, for MinkarDRM to save when the firmware
lights a display and to set again when it must light that display itself
(Docs/GPU-RDNA2.md, R2b): a pipe's blocks (the plane's reader, the pixel
processor, the blending tree's output, the output processor, the timing
generator), a DIG's (its encoders and packet generators) and the
engine's clocks and hub. Left out: status, interrupt, lock, trigger and
CRC registers, index/data pairs (the gamma tables' memories) and the
display clocks the SMU sets. From drm-kmod's dcn_3_0_2_offset.h (MIT) at
the version dist/FILES pins; written to amdgpu/amd_dcn302_snapshot.h.

    dcn302-snapshot.py [--source DIR] [--check]
"""

import argparse
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
DRM = os.path.join(HERE, '..', '..', 'Kernel', 'Extensions', 'MinkarDRM')
OUT = os.path.join(DRM, 'amdgpu', 'amd_dcn302_snapshot.h')

PIPES = 4
DIGS = 6
# A pipe's blocks, by their names' prefixes (the instance follows).
PIPE_BLOCKS = ['HUBP', 'HUBPREQ', 'HUBPRET', 'CURSOR', 'DPP_TOP', 'CNVC_CFG', 'CNVC_CUR',
               'DSCL', 'CM', 'MPCC', 'MPCC_OGAM', 'MPC_OUT', 'ODM', 'FMT', 'OPPBUF', 'OPP_PIPE', 'OTG', 'DPG', 'VTG']
DIG_BLOCKS = ['DIG', 'AFMT', 'VPG']
# The engine's: its clocks' controls and its hub's arbiter.
GLOBAL = re.compile(r'^(DCCG_|DCHUBBUB_|DPPCLK|DP_DTO|PHY[A-E]SYMCLK|SYMCLK|DSCCLK|OTG[0-3]_PIXEL_RATE|'
                    r'OTG[0-3]_PHYPLL|DCCG_AUDIO|DTBCLK|REFCLK|MICROSECOND)')
SKIP = re.compile(r'STATUS|_INT_|INTERRUPT|_ACK|(?<!C)LOCK|TRIGGER|CRC|PERFMON|DEBUG|_TEST|_INDEX$|_DATA$|'
                  r'DENTIST|DISPCLK|EARLIEST_INUSE|SURFACE_INUSE|POSITION|FRAME_COUNT|SNAPSHOT|'
                  r'_CAPTURE|FIFO_ERROR|UNDERFLOW|_RD_|_READ_|_COUNTER|DCCG_CAC|_SPARE|PERFCOUNTER')


def defines(path):
    out = {}
    for m in re.finditer(r'^#define\s+mm(\w+)\s+(0x[0-9A-Fa-f]+|\d+)\b', open(path).read(), re.M):
        out[m.group(1)] = int(m.group(2), 0)
    return out


def run(source):
    inc = os.path.join(source, 'drivers', 'gpu', 'drm', 'amd', 'include')
    offsets = defines(os.path.join(inc, 'asic_reg', 'dcn', 'dcn_3_0_2_offset.h'))
    bases = {}
    for m in re.finditer(r'^#define\s+(DCN_BASE__INST0_SEG\d)\s+(0x[0-9A-Fa-f]+)',
                         open(os.path.join(inc, 'dimgrey_cavefish_ip_offset.h')).read(), re.M):
        bases[m.group(1)] = int(m.group(2), 16)
    notice = open(os.path.join(inc, 'asic_reg', 'dcn', 'dcn_3_0_2_offset.h')).read()
    notice = notice[:notice.index('*/') + 2]

    def index(name):
        return bases['DCN_BASE__INST0_SEG%d' % offsets[name + '_BASE_IDX']] + offsets[name]

    regs = [n for n in offsets if not n.endswith('_BASE_IDX') and not SKIP.search(n)]

    def instances(blocks, count):
        """Names of instance 0's registers that every instance has, and
        each instance's indices of them."""
        out = []
        for n in sorted(regs):
            m = re.match(r'^(%s)0_(\w+)$' % '|'.join(sorted(blocks, key=len, reverse=True)), n)
            if not m or GLOBAL.match(n):
                continue
            names = ['%s%d_%s' % (m.group(1), i, m.group(2)) for i in range(count)]
            if all(x in offsets for x in names):
                out.append((n, [index(x) for x in names]))
        return out

    # The blocks' clock enables first: a block whose clock is off does not
    # take the double-buffered values written to it.
    pipe = sorted(instances(PIPE_BLOCKS, PIPES), key=lambda r: not re.search(r'CLOCK|CLK|OPP_PIPE_CONTROL', r[0]))
    dig = instances(DIG_BLOCKS, DIGS)
    glob = [(n, index(n)) for n in sorted(regs) if GLOBAL.match(n)]
    return pipe, dig, glob, notice


def header(pipe, dig, glob, notice, version):
    out = [notice, '',
           '/*',
           ' * Navi 23 (DCN 3.0.2): the registers of a lit display\'s state, generated',
           ' * by tools/drm-import/dcn302-snapshot.py from drm-kmod %s' % version,
           ' * (dcn_3_0_2_offset.h), under the notice above. Do not edit: run the',
           ' * tool again.',
           ' */', '',
           '#ifndef _MINKAR_AMD_DCN302_SNAPSHOT_H',
           '#define _MINKAR_AMD_DCN302_SNAPSHOT_H', '',
           '#include <stdint.h>', '',
           '#define DCN302_SNAPSHOT_PIPE_REGS\t%d' % len(pipe),
           '#define DCN302_SNAPSHOT_DIG_REGS\t%d' % len(dig),
           '#define DCN302_SNAPSHOT_GLOBAL_REGS\t%d' % len(glob), '',
           '/* A pipe\'s, by pipe (0 to 3). */',
           'static const uint32_t dcn302_snapshot_pipe[4][%d] = {' % len(pipe)]
    for p in range(PIPES):
        out.append('\t{ ' + ', '.join('0x%05x' % v[p] for _, v in pipe) + ' },')
    out += ['};', '', '/* A DIG\'s, by DIG (0 to 5). */',
            'static const uint32_t dcn302_snapshot_dig[6][%d] = {' % len(dig)]
    for d in range(DIGS):
        out.append('\t{ ' + ', '.join('0x%05x' % v[d] for _, v in dig) + ' },')
    out += ['};', '', '/* The engine\'s. */',
            'static const uint32_t dcn302_snapshot_global[%d] = {' % len(glob),
            '\t' + ', '.join('0x%05x' % v for _, v in glob), '};', '',
            '/* Which register a pipe\'s entry is (pipe 0\'s name), for the reports. */',
            'static const char *const dcn302_snapshot_pipe_names[%d] = {' % len(pipe),
            '\t' + ', '.join('"%s"' % n for n, _ in pipe), '};', '', '#endif', '']
    return '\n'.join(out)


def main(argv):
    p = argparse.ArgumentParser(description='Writes amdgpu/amd_dcn302_snapshot.h.')
    p.add_argument('--source', default=os.environ.get('MINKAR_DRM_SOURCE'))
    p.add_argument('--check', action='store_true')
    args = p.parse_args(argv)
    if args.source is None:
        sys.exit('dcn302-snapshot: no source (--source or $MINKAR_DRM_SOURCE)')
    version = None
    for line in open(os.path.join(DRM, 'dist', 'FILES')):
        if line.startswith('version '):
            version = line.split()[1]
    pipe, dig, glob, notice = run(args.source)
    text = header(pipe, dig, glob, notice, version)
    if args.check:
        same = os.path.exists(OUT) and open(OUT).read() == text
        print('dcn302-snapshot: %s' % ('matches the source' if same else 'differs from the source'))
        return 0 if same else 1
    open(OUT, 'w').write(text)
    print('dcn302-snapshot: wrote %s: %d a pipe, %d a DIG, %d the engine\'s'
          % (os.path.relpath(OUT), len(pipe), len(dig), len(glob)))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
