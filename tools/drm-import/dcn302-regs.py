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
DDCS = 5
DIGS = 6

# Registers of each pipe: the name with N for its number.
PIPE_REGS = [
    'OTGN_OTG_CONTROL', 'OTGN_OTG_H_TOTAL', 'OTGN_OTG_V_TOTAL',
    'OTGN_OTG_H_BLANK_START_END', 'OTGN_OTG_V_BLANK_START_END',
    'HUBPN_DCHUBP_CNTL', 'HUBPN_DCSURF_SURFACE_CONFIG',
    'HUBPN_DCSURF_PRI_VIEWPORT_DIMENSION',
    'HUBPREQN_DCSURF_SURFACE_PITCH', 'HUBPREQN_DCSURF_PRIMARY_SURFACE_ADDRESS',
    'HUBPREQN_DCSURF_PRIMARY_SURFACE_ADDRESS_HIGH',
    'HUBPN_DCSURF_TILING_CONFIG', 'HUBPREQN_DCSURF_FLIP_CONTROL',
    'HUBPREQN_DCSURF_SURFACE_EARLIEST_INUSE',
    'HUBPREQN_DCSURF_SURFACE_EARLIEST_INUSE_HIGH',
    'OTGN_OTG_STATUS', 'OTGN_OTG_STATUS_POSITION', 'OTGN_OTG_STATUS_FRAME_COUNT',
]
# Registers of each DDC line (1 to 5): the name with DDCN for its number.
DDC_REGS = ['DC_I2C_DDCN_SETUP', 'DC_I2C_DDCN_SPEED', 'DC_I2C_DDCN_HW_STATUS', 'DC_GPIO_DDCN_MASK']
# Registers of each DIG (0 to 5), the stream (front end) and link (back
# end) encoders: the name with DIGN for its number.
DIG_REGS = ['DIGN_DIG_FE_CNTL', 'DIGN_DIG_BE_CNTL']
# Registers of the engine as a whole.
REGS = ['DCN_VM_FB_LOCATION_BASE', 'DCN_VM_FB_LOCATION_TOP', 'DCN_VM_FB_OFFSET',
        'DC_I2C_CONTROL', 'DC_I2C_ARBITRATION', 'DC_I2C_SW_STATUS', 'DC_I2C_TRANSACTION0',
        'DC_I2C_TRANSACTION1', 'DC_I2C_TRANSACTION2', 'DC_I2C_TRANSACTION3', 'DC_I2C_DATA',
        'MICROSECOND_TIME_BASE_DIV',
        'DMCUB_CNTL', 'DMCUB_SEC_CNTL', 'DMCUB_MEM_CNTL', 'DMCUB_SCRATCH0', 'DMCUB_SCRATCH7',
        'DMCUB_TIMER_CURRENT', 'DMCUB_INBOX1_BASE_ADDRESS', 'DMCUB_INBOX1_SIZE',
        'DMCUB_INBOX1_RPTR', 'DMCUB_INBOX1_WPTR', 'DMCUB_OUTBOX1_RPTR', 'DMCUB_OUTBOX1_WPTR',
        'DMCUB_REGION3_CW0_BASE_ADDRESS', 'DMCUB_INST_FETCH_FAULT_ADDR',
        'DMCUB_UNDEFINED_ADDRESS_FAULT_ADDR',
        'DMCUB_REGION4_OFFSET', 'DMCUB_REGION4_OFFSET_HIGH', 'DMCUB_REGION4_TOP_ADDRESS',
        'DMCUB_REGION3_CW0_OFFSET', 'DMCUB_REGION3_CW0_OFFSET_HIGH', 'DMCUB_REGION3_CW0_BASE_ADDRESS', 'DMCUB_REGION3_CW0_TOP_ADDRESS',
        'DMCUB_REGION3_CW1_OFFSET', 'DMCUB_REGION3_CW1_OFFSET_HIGH', 'DMCUB_REGION3_CW1_BASE_ADDRESS', 'DMCUB_REGION3_CW1_TOP_ADDRESS',
        'DMCUB_REGION3_CW2_OFFSET', 'DMCUB_REGION3_CW2_OFFSET_HIGH', 'DMCUB_REGION3_CW2_BASE_ADDRESS', 'DMCUB_REGION3_CW2_TOP_ADDRESS',
        'DMCUB_REGION3_CW3_OFFSET', 'DMCUB_REGION3_CW3_OFFSET_HIGH', 'DMCUB_REGION3_CW3_BASE_ADDRESS', 'DMCUB_REGION3_CW3_TOP_ADDRESS',
        'DMCUB_REGION3_CW4_OFFSET', 'DMCUB_REGION3_CW4_OFFSET_HIGH', 'DMCUB_REGION3_CW4_BASE_ADDRESS', 'DMCUB_REGION3_CW4_TOP_ADDRESS',
        'DMCUB_REGION3_CW5_OFFSET', 'DMCUB_REGION3_CW5_OFFSET_HIGH', 'DMCUB_REGION3_CW5_BASE_ADDRESS', 'DMCUB_REGION3_CW5_TOP_ADDRESS',
        'DMCUB_REGION3_CW6_OFFSET', 'DMCUB_REGION3_CW6_OFFSET_HIGH', 'DMCUB_REGION3_CW6_BASE_ADDRESS', 'DMCUB_REGION3_CW6_TOP_ADDRESS',
        'DMCUB_REGION3_CW7_OFFSET', 'DMCUB_REGION3_CW7_OFFSET_HIGH', 'DMCUB_REGION3_CW7_BASE_ADDRESS', 'DMCUB_REGION3_CW7_TOP_ADDRESS']
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
    'HUBP0_DCSURF_TILING_CONFIG__SW_MODE',
    'HUBPREQ0_DCSURF_FLIP_CONTROL__SURFACE_UPDATE_LOCK',
    'HUBPREQ0_DCSURF_FLIP_CONTROL__SURFACE_FLIP_TYPE',
    'HUBPREQ0_DCSURF_FLIP_CONTROL__SURFACE_FLIP_PENDING',
    'HUBPREQ0_DCSURF_SURFACE_EARLIEST_INUSE_HIGH__SURFACE_EARLIEST_INUSE_ADDRESS_HIGH',
    'OTG0_OTG_STATUS__OTG_V_BLANK',
    'OTG0_OTG_STATUS_POSITION__OTG_VERT_COUNT',
    'OTG0_OTG_STATUS_FRAME_COUNT__OTG_FRAME_COUNT',
    'DC_I2C_DDC1_SETUP__DC_I2C_DDC1_ENABLE',
    'DC_I2C_DDC1_SETUP__DC_I2C_DDC1_TIME_LIMIT',
    'DC_I2C_DDC1_SETUP__DC_I2C_DDC1_DATA_DRIVE_EN',
    'DC_I2C_DDC1_SETUP__DC_I2C_DDC1_CLK_DRIVE_EN',
    'DC_I2C_DDC1_SETUP__DC_I2C_DDC1_DATA_DRIVE_SEL',
    'DC_I2C_DDC1_SETUP__DC_I2C_DDC1_INTRA_TRANSACTION_DELAY',
    'DC_I2C_DDC1_SETUP__DC_I2C_DDC1_INTRA_BYTE_DELAY',
    'DC_I2C_DDC1_SETUP__DC_I2C_DDC1_SEND_RESET_LENGTH',
    'DC_I2C_DDC1_SPEED__DC_I2C_DDC1_PRESCALE',
    'DC_I2C_DDC1_SPEED__DC_I2C_DDC1_THRESHOLD',
    'DC_I2C_DDC1_SPEED__DC_I2C_DDC1_START_STOP_TIMING_CNTL',
    'DC_I2C_DDC1_HW_STATUS__DC_I2C_DDC1_HW_STATUS',
    'DC_GPIO_DDC1_MASK__DC_GPIO_DDC1CLK_MASK',
    'DC_GPIO_DDC1_MASK__DC_GPIO_DDC1DATA_MASK',
    'DC_GPIO_DDC1_MASK__DC_GPIO_DDC1CLK_PD_EN',
    'DC_GPIO_DDC1_MASK__DC_GPIO_DDC1DATA_PD_EN',
    'DC_GPIO_DDC1_MASK__AUX_PAD1_MODE',
    'DC_I2C_CONTROL__DC_I2C_GO',
    'DC_I2C_CONTROL__DC_I2C_SOFT_RESET',
    'DC_I2C_CONTROL__DC_I2C_SEND_RESET',
    'DC_I2C_CONTROL__DC_I2C_SW_STATUS_RESET',
    'DC_I2C_CONTROL__DC_I2C_DDC_SELECT',
    'DC_I2C_CONTROL__DC_I2C_TRANSACTION_COUNT',
    'DC_I2C_ARBITRATION__DC_I2C_REG_RW_CNTL_STATUS',
    'DC_I2C_ARBITRATION__DC_I2C_NO_QUEUED_SW_GO',
    'DC_I2C_ARBITRATION__DC_I2C_SW_USE_I2C_REG_REQ',
    'DC_I2C_ARBITRATION__DC_I2C_SW_DONE_USING_I2C_REG',
    'DC_I2C_SW_STATUS__DC_I2C_SW_STATUS',
    'DC_I2C_SW_STATUS__DC_I2C_SW_DONE',
    'DC_I2C_SW_STATUS__DC_I2C_SW_ABORTED',
    'DC_I2C_SW_STATUS__DC_I2C_SW_TIMEOUT',
    'DC_I2C_SW_STATUS__DC_I2C_SW_STOPPED_ON_NACK',
    'DC_I2C_TRANSACTION0__DC_I2C_RW0',
    'DC_I2C_TRANSACTION0__DC_I2C_STOP_ON_NACK0',
    'DC_I2C_TRANSACTION0__DC_I2C_START0',
    'DC_I2C_TRANSACTION0__DC_I2C_STOP0',
    'DC_I2C_TRANSACTION0__DC_I2C_COUNT0',
    'DC_I2C_DATA__DC_I2C_DATA_RW',
    'DC_I2C_DATA__DC_I2C_DATA',
    'DC_I2C_DATA__DC_I2C_INDEX',
    'DC_I2C_DATA__DC_I2C_INDEX_WRITE',
    'MICROSECOND_TIME_BASE_DIV__MICROSECOND_TIME_BASE_DIV',
    'MICROSECOND_TIME_BASE_DIV__XTAL_REF_DIV',
    'DIG0_DIG_FE_CNTL__DIG_SOURCE_SELECT',
    'DIG0_DIG_FE_CNTL__DIG_SYMCLK_FE_ON',
    'DIG0_DIG_BE_CNTL__DIG_FE_SOURCE_SELECT',
    'DIG0_DIG_BE_CNTL__DIG_MODE',
    'DIG0_DIG_BE_CNTL__DIG_HPD_SELECT',
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
    for r in DDC_REGS:
        values.append(('ddc', r.replace('DDCN_', 'DDC_'), [index(r.replace('DDCN_', 'DDC%d_' % d))
                                                         for d in range(1, DDCS + 1)]))
    for r in DIG_REGS:
        values.append(('dig', r.replace('DIGN_', 'DIG_', 1), [index(r.replace('DIGN_', 'DIG%d_' % d, 1))
                                                             for d in range(DIGS)]))
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
           '#define DCN302_PIPES\t%d' % PIPES,
           '#define DCN302_DDCS\t%d' % DDCS,
           '#define DCN302_DIGS\t%d' % DIGS, '',
           '/* A pipe\'s registers, by pipe. */']
    for kind, name, v in values:
        if kind == 'pipe':
            out.append('static const uint32_t dcn302_%s[DCN302_PIPES] = { %s };'
                       % (name, ', '.join('0x%05x' % x for x in v)))
    out += ['', '/* A DDC line\'s registers, by line (DDC1 first). */']
    for kind, name, v in values:
        if kind == 'ddc':
            out.append('static const uint32_t dcn302_%s[DCN302_DDCS] = { %s };'
                       % (name, ', '.join('0x%05x' % x for x in v)))
    out += ['', '/* A DIG\'s registers, by DIG. */']
    for kind, name, v in values:
        if kind == 'dig':
            out.append('static const uint32_t dcn302_%s[DCN302_DIGS] = { %s };'
                       % (name, ', '.join('0x%05x' % x for x in v)))
    out += ['', '/* The engine\'s. */']
    for kind, name, v in values:
        if kind == 'reg':
            out.append('#define dcn302_%s\t0x%05x' % (name, v))
    out += ['', '/* Fields: shift and mask. */']
    for kind, name, v in values:
        if kind == 'field':
            short = name.replace('0_', '_', 1) if re.match(r'^(OTG|HUBP|HUBPREQ|DIG)0_', name) else name
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
