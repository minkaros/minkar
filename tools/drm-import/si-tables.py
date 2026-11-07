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
si-tables.py: the GCN 1.0 (SI) graphics tables MinkarDRM needs, as numbers,
from drm-kmod's amdgpu (MIT) at the version dist/FILES pins: each chip's
tiling mode table (gfx_v6_0_tiling_mode_table_init, for each DRAM row
size) and its golden register settings (si.c). The source's own code and
tables are compiled on the host against a minimal device structure and
run; the values are written, with the source's MIT notice, to
amdgpu/amd_si_gfx_tables.h.

    si-tables.py [--source DIR] [--check]

--check compares the header with what the source gives (exit status 1 if
it differs).
"""

import argparse
import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
DRM = os.path.join(HERE, '..', '..', 'Kernel', 'Extensions', 'MinkarDRM')
OUT = os.path.join(DRM, 'amdgpu', 'amd_si_gfx_tables.h')
CHIPS = ['TAHITI', 'PITCAIRN', 'VERDE', 'OLAND', 'HAINAN']
# Registers, fields and values the graphics block's start uses
# (gfx_v6_0_constants_init and what it calls), from the source's headers.
CONSTANTS = """
mmCC_DRM_ID_STRAPS CC_DRM_ID_STRAPS__ATI_REV_ID_MASK CC_DRM_ID_STRAPS__ATI_REV_ID__SHIFT
mmGRBM_CNTL GRBM_CNTL__READ_TIMEOUT__SHIFT mmMC_ARB_RAMCFG MC_ARB_RAMCFG__NOOFCOLS_MASK
MC_ARB_RAMCFG__NOOFCOLS__SHIFT mmGB_ADDR_CONFIG GB_ADDR_CONFIG__ROW_SIZE_MASK
GB_ADDR_CONFIG__ROW_SIZE__SHIFT GB_ADDR_CONFIG__NUM_SHADER_ENGINES_MASK
GB_ADDR_CONFIG__NUM_SHADER_ENGINES__SHIFT TAHITI_GB_ADDR_CONFIG_GOLDEN VERDE_GB_ADDR_CONFIG_GOLDEN
HAINAN_GB_ADDR_CONFIG_GOLDEN mmDMIF_ADDR_CONFIG mmDMIF_ADDR_CALC mmHDP_ADDR_CONFIG
mmDMA_TILING_CONFIG DMA0_REGISTER_OFFSET DMA1_REGISTER_OFFSET mmGB_TILE_MODE0 mmGRBM_GFX_INDEX
GRBM_GFX_INDEX__INSTANCE_BROADCAST_WRITES_MASK GRBM_GFX_INDEX__SH_BROADCAST_WRITES_MASK
GRBM_GFX_INDEX__SE_BROADCAST_WRITES_MASK GRBM_GFX_INDEX__SH_INDEX__SHIFT
GRBM_GFX_INDEX__SE_INDEX__SHIFT mmCC_RB_BACKEND_DISABLE mmGC_USER_RB_BACKEND_DISABLE
GC_USER_RB_BACKEND_DISABLE__BACKEND_DISABLE_MASK GC_USER_RB_BACKEND_DISABLE__BACKEND_DISABLE__SHIFT
mmPA_SC_RASTER_CONFIG PA_SC_RASTER_CONFIG__RB_XSEL2__SHIFT PA_SC_RASTER_CONFIG__RB_XSEL__SHIFT
PA_SC_RASTER_CONFIG__RB_YSEL__SHIFT PA_SC_RASTER_CONFIG__PKR_MAP__SHIFT
PA_SC_RASTER_CONFIG__PKR_YSEL__SHIFT PA_SC_RASTER_CONFIG__SE_MAP__SHIFT
PA_SC_RASTER_CONFIG__SE_XSEL__SHIFT PA_SC_RASTER_CONFIG__SE_YSEL__SHIFT
mmSPI_STATIC_THREAD_MGMT_3 mmCC_GC_SHADER_ARRAY_CONFIG mmGC_USER_SHADER_ARRAY_CONFIG
GC_USER_SHADER_ARRAY_CONFIG__INACTIVE_CUS_MASK GC_USER_SHADER_ARRAY_CONFIG__INACTIVE_CUS__SHIFT
mmCP_QUEUE_THRESHOLDS CP_QUEUE_THRESHOLDS__ROQ_IB1_START__SHIFT
CP_QUEUE_THRESHOLDS__ROQ_IB2_START__SHIFT mmCP_MEQ_THRESHOLDS CP_MEQ_THRESHOLDS__MEQ1_START__SHIFT
CP_MEQ_THRESHOLDS__MEQ2_START__SHIFT mmSX_DEBUG_1 mmSPI_CONFIG_CNTL_1
SPI_CONFIG_CNTL_1__VTX_DONE_DELAY__SHIFT mmPA_SC_FIFO_SIZE
PA_SC_FIFO_SIZE__SC_FRONTEND_PRIM_FIFO_SIZE__SHIFT PA_SC_FIFO_SIZE__SC_BACKEND_PRIM_FIFO_SIZE__SHIFT
PA_SC_FIFO_SIZE__SC_HIZ_TILE_FIFO_SIZE__SHIFT PA_SC_FIFO_SIZE__SC_EARLYZ_TILE_FIFO_SIZE__SHIFT
mmVGT_NUM_INSTANCES mmCP_PERFMON_CNTL mmSQ_CONFIG mmPA_SC_FORCE_EOV_MAX_CNTS
PA_SC_FORCE_EOV_MAX_CNTS__FORCE_EOV_MAX_CLK_CNT__SHIFT
PA_SC_FORCE_EOV_MAX_CNTS__FORCE_EOV_MAX_REZ_CNT__SHIFT mmVGT_CACHE_INVALIDATION
VGT_CACHE_INVALIDATION__CACHE_INVALIDATION__SHIFT VGT_CACHE_INVALIDATION__AUTO_INVLD_EN__SHIFT
VC_AND_TC ES_AND_GS_AUTO mmVGT_GS_VERTEX_REUSE mmPA_SC_LINE_STIPPLE_STATE
mmCB_PERFCOUNTER0_SELECT0 mmCB_PERFCOUNTER0_SELECT1 mmCB_PERFCOUNTER1_SELECT0
mmCB_PERFCOUNTER1_SELECT1 mmCB_PERFCOUNTER2_SELECT0 mmCB_PERFCOUNTER2_SELECT1
mmCB_PERFCOUNTER3_SELECT0 mmCB_PERFCOUNTER3_SELECT1 mmHDP_HOST_PATH_CNTL mmPA_CL_ENHANCE
PA_CL_ENHANCE__CLIP_VTX_REORDER_ENA_MASK PA_CL_ENHANCE__NUM_CLIP_SEQ__SHIFT
mmCGTS_TCC_DISABLE mmCGTS_USER_TCC_DISABLE
mmRLC_CNTL RLC_CNTL__RLC_ENABLE_F32_MASK mmRLC_RL_BASE mmRLC_RL_SIZE mmRLC_LB_CNTL
RLC_LB_CNTL__LOAD_BALANCE_ENABLE_MASK mmRLC_LB_CNTR_MAX mmRLC_LB_CNTR_INIT mmRLC_LB_INIT_CU_MASK
mmRLC_MC_CNTL mmRLC_UCODE_CNTL mmRLC_UCODE_ADDR mmRLC_UCODE_DATA mmRLC_SAVE_AND_RESTORE_BASE
mmRLC_CLEAR_STATE_RESTORE_BASE mmRLC_SERDES_MASTER_BUSY_0 mmRLC_SERDES_MASTER_BUSY_1 mmRLC_STAT
RLC_BUSY_STATUS GFX_POWER_STATUS GFX_CLOCK_STATUS GFX_LS_STATUS mmCP_INT_CNTL_RING0
CP_INT_CNTL__CNTX_BUSY_INT_ENABLE_MASK CP_INT_CNTL__CNTX_EMPTY_INT_ENABLE_MASK mmDB_DEPTH_INFO
mmSPI_LB_CU_MASK mmMC_SEQ_MISC0 GRBM_SOFT_RESET__SOFT_RESET_RLC_MASK
""".split()

GOLDEN = {
    'TAHITI': ['tahiti_golden_registers', 'tahiti_golden_rlc_registers', 'tahiti_golden_registers2'],
    'PITCAIRN': ['pitcairn_golden_registers', 'pitcairn_golden_rlc_registers'],
    'VERDE': ['verde_golden_registers', 'verde_golden_rlc_registers'],
    'OLAND': ['oland_golden_registers', 'oland_golden_rlc_registers'],
    'HAINAN': ['hainan_golden_registers', 'hainan_golden_registers2'],
}


def function_body(text, name):
    m = re.search(r'\n(static [^\n]*\b' + name + r'\([^)]*\)\n\{)', text)
    if not m:
        sys.exit('si-tables: no %s' % name)
    start = m.start(1)
    depth, i = 0, text.index('{', start)
    while True:
        if text[i] == '{':
            depth += 1
        elif text[i] == '}':
            depth -= 1
            if depth == 0:
                return text[start:i + 1]
        i += 1


def array_text(text, name):
    m = re.search(r'static const u32 ' + name + r'\[\] =\s*\{(.*?)\};', text, re.S)
    if not m:
        sys.exit('si-tables: no %s' % name)
    return m.group(1)


def run(source):
    amd = os.path.join(source, 'drivers', 'gpu', 'drm', 'amd')
    gpu = os.path.join(amd, 'amdgpu')
    gfx = open(os.path.join(gpu, 'gfx_v6_0.c')).read()
    si = open(os.path.join(gpu, 'si.c')).read()
    tiling = function_body(gfx, 'gfx_v6_0_tiling_mode_table_init')
    regs = ['gca/gfx_6_0_d.h', 'gmc/gmc_6_0_d.h', 'dce/dce_6_0_d.h', 'oss/oss_1_0_d.h',
            'bif/bif_3_0_d.h', 'uvd/uvd_4_0_d.h']
    prog = ['#include <stdio.h>', '#include <stdint.h>', '#include <string.h>', '#include <stdbool.h>',
            'typedef uint32_t u32;',
            'enum { %s };' % ', '.join('CHIP_' + c for c in CHIPS),
            '#include "clearstate_defs.h"',
            'struct amdgpu_ring { int unused; };',
            'struct amdgpu_device { int asic_type; struct { struct { u32 tile_mode_array[32];'
            ' u32 mem_row_size_in_kb; u32 max_hw_contexts;'
            ' struct { u32 raster_config; } rb_config[4][4]; } config;'
            ' struct { const struct cs_section_def *cs_data; } rlc;'
            ' struct amdgpu_ring gfx_ring[1]; } gfx; };',
            '#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))',
            '#define WREG32(r, v) ((void)(r), (void)(v))',
            '#define DRM_ERROR(...) ((void)0)',
            '#define udelay(n) ((void)(n))',
            '#define cpu_to_le32(v) (v)',
            # The ring, recorded: each commit ends a part.
            'static u32 stream[4096]; static unsigned streamN, parts[8], partN;',
            'static int amdgpu_ring_alloc(struct amdgpu_ring *r, unsigned n) { (void)r; (void)n; return 0; }',
            'static void amdgpu_ring_write(struct amdgpu_ring *r, u32 v) { (void)r; stream[streamN++] = v; }',
            'static void amdgpu_ring_commit(struct amdgpu_ring *r) { (void)r; parts[partN++] = streamN; }']
    prog += ['#include "asic_reg/%s"' % r for r in regs]
    prog += ['#include "asic_reg/%s"' % r.replace('_d.h', '_sh_mask.h') for r in regs
             if os.path.exists(os.path.join(amd, 'include', 'asic_reg', r.replace('_d.h', '_sh_mask.h')))]
    # gfx_v6_0.c gets the tiling macros, and si.c the golden tables'
    # names, from sid.h (through amdgpu.h).
    prog += ['#include "gca/gfx_7_2_enum.h"', '#include "sid.h"', '#include "si_enums.h"']
    prog.append('#include "clearstate_si.h"')
    prog.append(tiling)
    # The clear state (the RLC's buffer and the command processor's start)
    # and the RLC's save and restore list.
    for f in ('gfx_v6_0_get_csb_size', 'gfx_v6_0_get_csb_buffer', 'gfx_v6_0_cp_gfx_enable',
              'gfx_v6_0_cp_gfx_start'):
        prog.append(function_body(gfx, f))
    prog.append('static const u32 verde_rlc_save_restore_register_list[] = {%s};' %
                array_text(gfx, 'verde_rlc_save_restore_register_list'))
    for chip in CHIPS:
        for a in GOLDEN[chip]:
            prog.append('static const u32 %s[] = {%s};' % (a, array_text(si, a)))
    prog.append('int main(void) {')
    prog.append('\tstruct amdgpu_device d;')
    for chip in CHIPS:
        for kb in (1, 2, 4):
            prog += ['\tmemset(&d, 0, sizeof(d));', '\td.asic_type = CHIP_%s;' % chip,
                     '\td.gfx.config.mem_row_size_in_kb = %d;' % kb,
                     '\tgfx_v6_0_tiling_mode_table_init(&d);',
                     '\tprintf("tiling %s %d");' % (chip, kb),
                     '\tfor (int i = 0; i < 32; i++) printf(" 0x%08x", d.gfx.config.tile_mode_array[i]);',
                     '\tprintf("\\n");']
        for a in GOLDEN[chip]:
            prog += ['\tprintf("golden %s %s");' % (chip, a),
                     '\tfor (size_t i = 0; i < ARRAY_SIZE(%s); i++) printf(" 0x%%08x", %s[i]);' % (a, a),
                     '\tprintf("\\n");']
    prog += ['\tmemset(&d, 0, sizeof(d));', '\td.gfx.rlc.cs_data = si_cs_data;',
             '\td.gfx.config.max_hw_contexts = 8;',
             '\td.gfx.config.rb_config[0][0].raster_config = 0x5a5a5a5a;',
             '\t{ u32 n = gfx_v6_0_get_csb_size(&d), buf[4096]; gfx_v6_0_get_csb_buffer(&d, buf);',
             '\t  printf("csb"); for (u32 i = 0; i < n; i++) printf(" 0x%08x", buf[i]); printf("\\n"); }',
             '\tgfx_v6_0_cp_gfx_start(&d);',
             '\tfor (unsigned p = 0, at = 0; p < partN; at = parts[p++]) {',
             '\t  printf("start%u", p); for (unsigned i = at; i < parts[p]; i++) printf(" 0x%08x", stream[i]);',
             '\t  printf("\\n"); }',
             '\tprintf("saverestore"); for (size_t i = 0; i < ARRAY_SIZE(verde_rlc_save_restore_register_list); i++)',
             '\t  printf(" 0x%08x", verde_rlc_save_restore_register_list[i]); printf("\\n");']
    for name in CONSTANTS:
        prog.append('\tprintf("const %s 0x%%llx\\n", (unsigned long long)(%s));' % (name, name))
    prog.append('\treturn 0;\n}')
    with tempfile.TemporaryDirectory() as tmp:
        c = os.path.join(tmp, 'si_tables.c')
        open(c, 'w').write('\n'.join(prog) + '\n')
        exe = os.path.join(tmp, 'si_tables')
        r = subprocess.run(['cc', '-std=gnu11', '-w', '-I', gpu, '-I', os.path.join(amd, 'include'),
                            '-I', os.path.join(amd, 'include', 'asic_reg'), '-o', exe, c],
                           capture_output=True, text=True)
        if r.returncode != 0:
            sys.exit('si-tables: cannot build the tables:\n' + r.stderr[:3000])
        return subprocess.run([exe], capture_output=True, text=True, check=True).stdout, \
            gfx[:gfx.index('*/') + 2]


def header(values, notice, version):
    out = [notice, '',
           '/*',
           ' * GCN 1.0 (SI) graphics tables, as numbers: generated by',
           ' * tools/drm-import/si-tables.py from drm-kmod %s (gfx_v6_0.c\'s' % version,
           ' * tiling mode tables, clear state and command processor start,',
           ' * si.c\'s golden registers and the registers the graphics block\'s',
           ' * start uses, under the notice',
           ' * above). Do not edit: run the tool again.',
           ' */', '',
           '#ifndef _MINKAR_AMD_SI_GFX_TABLES_H',
           '#define _MINKAR_AMD_SI_GFX_TABLES_H', '',
           '#include <stdint.h>', '']
    tiling = {}
    for line in values.splitlines():
        f = line.split()
        if f[0] == 'tiling':
            tiling.setdefault(f[1], {})[int(f[2])] = f[3:]
        elif f[0] == 'golden':
            name = 'si_%s' % f[2].replace('_golden', '_golden')
            out.append('/* %s: register, mask, value (the mask all ones: the value as is). */' % f[2])
            out.append('static const uint32_t %s[] = {' % name)
            for i in range(3, len(f), 3):
                out.append('\t%s, %s, %s,' % tuple(f[i:i + 3]))
            out += ['};', '']
    def words(name, comment, vals):
        out.append('/* %s */' % comment)
        out.append('static const uint32_t %s[] = {' % name)
        for i in range(0, len(vals), 4):
            out.append('\t%s,' % ', '.join(vals[i:i + 4]))
        out.extend(['};', ''])
    for line in values.splitlines():
        f = line.split()
        if f[0] == 'csb':
            words('si_clear_state', 'The clear state buffer\'s commands (gfx_v6_0_get_csb_buffer); '
                  'PA_SC_RASTER_CONFIG\'s value is 0x5a5a5a5a, for the driver to set.', f[1:])
            out.append('#define SI_CLEAR_STATE_RASTER_INDEX\t%d' % f[1:].index('0x5a5a5a5a'))
            out.append('')
        elif f[0].startswith('start'):
            words('si_cp_start%s' % f[0][5:], 'The command processor\'s start, part %s '
                  '(gfx_v6_0_cp_gfx_start: %s).' % (f[0][5:], 'before it is on' if f[0] == 'start0'
                  else 'once it is on'), f[1:])
        elif f[0] == 'saverestore':
            words('si_rlc_save_restore', 'The RLC\'s save and restore list '
                  '(verde_rlc_save_restore_register_list, every GCN 1.0 chip\'s).', f[1:])
    consts = [line.split()[1:] for line in values.splitlines() if line.startswith('const ')]
    out.append('/* Registers (dword indices), fields and values (gfx_v6_0.c\'s headers). */')
    for name, value in consts:
        out.append('#define SI_%s\t%s' % (name, value))
    out.append('')
    out.append('/* Each chip\'s 32 tiling modes (GB_TILE_MODE0-31), for DRAM rows of 1, 2')
    out.append(' * and 4 KB. */')
    for chip in CHIPS:
        out.append('static const uint32_t si_%s_tiling[3][32] = {' % chip.lower())
        for kb in (1, 2, 4):
            v = tiling[chip][kb]
            out.append('\t{ /* %d KB rows */' % kb)
            for i in range(0, 32, 4):
                out.append('\t\t%s,' % ', '.join(v[i:i + 4]))
            out.append('\t},')
        out += ['};', '']
    out.append('#endif')
    return '\n'.join(out) + '\n'


def main():
    p = argparse.ArgumentParser(description=__doc__.strip().splitlines()[0])
    p.add_argument('--source', default=os.environ.get('MINKAR_DRM_SOURCE'))
    p.add_argument('--check', action='store_true')
    args = p.parse_args()
    if args.source is None:
        sys.exit('si-tables: no source (--source or $MINKAR_DRM_SOURCE)')
    version = None
    for line in open(os.path.join(DRM, 'dist', 'FILES')):
        if line.startswith('version '):
            version = line.split()[1]
    values, notice = run(args.source)
    text = header(values, notice, version)
    if args.check:
        same = os.path.exists(OUT) and open(OUT).read() == text
        print('si-tables: %s' % ('matches the source' if same else 'differs from the source'))
        sys.exit(0 if same else 1)
    open(OUT, 'w').write(text)
    print('si-tables: wrote %s' % os.path.relpath(OUT))


if __name__ == '__main__':
    main()
