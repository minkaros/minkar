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
dcn302-regs.py: the display engine registers MinkarDRM uses on Navi 23
(DCN 3.0.2, the RX 6600's), as numbers, from drm-kmod's amdgpu (MIT) at
the version dist/FILES pins: each register's dword index in the register
window (its block's segment base, from dimgrey_cavefish_ip_offset.h, plus
its offset, from dcn_3_0_2_offset.h), for each of the four pipes where it
is a pipe's, and the fields read, from dcn_3_0_2_sh_mask.h. Written, with
the source's MIT notice, to amdgpu/amd_dcn302_regs.h.

    dcn302-regs.py [--source DIR] [--check]

--check compares the header with what the source gives (exit status 1 if
it differs).
"""

import argparse
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
DRM = os.path.join(HERE, '..', '..', 'Kernel', 'Extensions', 'MinkarDRM')
OUT = os.path.join(DRM, 'amdgpu', 'amd_dcn302_regs.h')
PIPES = 4

# Registers of each pipe: the name with N for its number.
PIPE_REGS = [
    'OTGN_OTG_CONTROL', 'OTGN_OTG_H_TOTAL', 'OTGN_OTG_V_TOTAL',
    'OTGN_OTG_H_BLANK_START_END', 'OTGN_OTG_V_BLANK_START_END',
    'HUBPN_DCHUBP_CNTL', 'HUBPN_DCSURF_SURFACE_CONFIG',
    'HUBPN_DCSURF_PRI_VIEWPORT_DIMENSION',
    'HUBPREQN_DCSURF_SURFACE_PITCH', 'HUBPREQN_DCSURF_PRIMARY_SURFACE_ADDRESS',
    'HUBPREQN_DCSURF_PRIMARY_SURFACE_ADDRESS_HIGH',
]
# Registers of the engine as a whole.
REGS = ['DCN_VM_FB_LOCATION_BASE', 'DCN_VM_FB_LOCATION_TOP', 'DCN_VM_FB_OFFSET']
# Fields, by pipe 0's register (the same in every pipe).
FIELDS = [
    'OTG0_OTG_CONTROL__OTG_MASTER_EN',
    'OTG0_OTG_H_TOTAL__OTG_H_TOTAL',
    'OTG0_OTG_V_TOTAL__OTG_V_TOTAL',
    'OTG0_OTG_H_BLANK_START_END__OTG_H_BLANK_START',
    'OTG0_OTG_H_BLANK_START_END__OTG_H_BLANK_END',
    'OTG0_OTG_V_BLANK_START_END__OTG_V_BLANK_START',
    'OTG0_OTG_V_BLANK_START_END__OTG_V_BLANK_END',
    'HUBP0_DCHUBP_CNTL__HUBP_BLANK_EN',
    'HUBP0_DCSURF_SURFACE_CONFIG__SURFACE_PIXEL_FORMAT',
    'HUBP0_DCSURF_PRI_VIEWPORT_DIMENSION__PRI_VIEWPORT_WIDTH',
    'HUBP0_DCSURF_PRI_VIEWPORT_DIMENSION__PRI_VIEWPORT_HEIGHT',
    'HUBPREQ0_DCSURF_SURFACE_PITCH__PITCH',
    'HUBPREQ0_DCSURF_PRIMARY_SURFACE_ADDRESS_HIGH__PRIMARY_SURFACE_ADDRESS_HIGH',
    'DCN_VM_FB_LOCATION_BASE__FB_BASE',
    'DCN_VM_FB_LOCATION_TOP__FB_TOP',
    'DCN_VM_FB_OFFSET__FB_OFFSET',
]


def defines(path):
    out = {}
    for m in re.finditer(r'^#define\s+(\w+)\s+(0x[0-9A-Fa-f]+|\d+)L?\b', open(path).read(), re.M):
        out[m.group(1)] = int(m.group(2), 0)
    return out


def run(source):
    inc = os.path.join(source, 'drivers', 'gpu', 'drm', 'amd', 'include')
    offsets = defines(os.path.join(inc, 'asic_reg', 'dcn', 'dcn_3_0_2_offset.h'))
    masks = defines(os.path.join(inc, 'asic_reg', 'dcn', 'dcn_3_0_2_sh_mask.h'))
    bases = defines(os.path.join(inc, 'dimgrey_cavefish_ip_offset.h'))
    notice = open(os.path.join(inc, 'asic_reg', 'dcn', 'dcn_3_0_2_offset.h')).read()
    notice = notice[:notice.index('*/') + 2]

    def index(name):
        if 'mm' + name not in offsets:
            sys.exit('dcn302-regs: no mm%s' % name)
        seg = offsets['mm' + name + '_BASE_IDX']
        return bases['DCN_BASE__INST0_SEG%d' % seg] + offsets['mm' + name]

    values = []
    for r in PIPE_REGS:
        values.append(('pipe', r.replace('N_', '_'), [index(r.replace('N_', '%d_' % p))
                                                      for p in range(PIPES)]))
    for r in REGS:
        values.append(('reg', r, index(r)))
    for f in FIELDS:
        if f + '_MASK' not in masks:
            sys.exit('dcn302-regs: no %s_MASK' % f)
        values.append(('field', f, (masks[f + '__SHIFT'], masks[f + '_MASK'])))
    return values, notice


def header(values, notice, version):
    out = [notice, '',
           '/*',
           ' * Navi 23 (DCN 3.0.2) display engine registers, as numbers: generated',
           ' * by tools/drm-import/dcn302-regs.py from drm-kmod %s' % version,
           ' * (dimgrey_cavefish_ip_offset.h and dcn_3_0_2_offset.h: each one\'s',
           ' * dword index in the register window; dcn_3_0_2_sh_mask.h: the fields),',
           ' * under the notice above. Do not edit: run the tool again.',
           ' */', '',
           '#ifndef _MINKAR_AMD_DCN302_REGS_H',
           '#define _MINKAR_AMD_DCN302_REGS_H', '',
           '#include <stdint.h>', '',
           '#define DCN302_PIPES\t%d' % PIPES, '',
           '/* A pipe\'s registers, by pipe. */']
    for kind, name, v in values:
        if kind == 'pipe':
            out.append('static const uint32_t dcn302_%s[DCN302_PIPES] = { %s };'
                       % (name, ', '.join('0x%05x' % x for x in v)))
    out += ['', '/* The engine\'s. */']
    for kind, name, v in values:
        if kind == 'reg':
            out.append('#define dcn302_%s\t0x%05x' % (name, v))
    out += ['', '/* Fields: shift and mask. */']
    for kind, name, v in values:
        if kind == 'field':
            short = name.replace('0_', '_', 1) if re.match(r'^(OTG|HUBP|HUBPREQ)0_', name) else name
            out.append('#define DCN302_%s__SHIFT\t%d' % (short, v[0]))
            out.append('#define DCN302_%s_MASK\t0x%08x' % (short, v[1]))
    out += ['', '#endif', '']
    return '\n'.join(out)


def main(argv):
    p = argparse.ArgumentParser(description='Writes amdgpu/amd_dcn302_regs.h.')
    p.add_argument('--source', default=os.environ.get('MINKAR_DRM_SOURCE'))
    p.add_argument('--check', action='store_true')
    args = p.parse_args(argv)
    if args.source is None:
        sys.exit('dcn302-regs: no source (--source or $MINKAR_DRM_SOURCE)')
    version = None
    for line in open(os.path.join(DRM, 'dist', 'FILES')):
        if line.startswith('version '):
            version = line.split()[1]
    values, notice = run(args.source)
    text = header(values, notice, version)
    if args.check:
        same = os.path.exists(OUT) and open(OUT).read() == text
        print('dcn302-regs: %s' % ('matches the source' if same else 'differs from the source'))
        return 0 if same else 1
    open(OUT, 'w').write(text)
    print('dcn302-regs: wrote %s' % os.path.relpath(OUT))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
