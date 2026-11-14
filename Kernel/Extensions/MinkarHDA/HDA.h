/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Intel High Definition Audio: the controller's registers and the codec
 * verbs and parameters this driver uses, as the specification (revision
 * 1.0a) names them.
 */

#ifndef _MINKAR_HDA_H_
#define _MINKAR_HDA_H_

/* ---- Controller registers ---------------------------------------------- */

#define HDA_GCAP            0x00    /* 16: global capabilities */
#define  HDA_GCAP_64OK      0x0001
#define  HDA_GCAP_OSS(g)    (((g) >> 12) & 0xF)    /* output streams */
#define  HDA_GCAP_ISS(g)    (((g) >> 8) & 0xF)     /* input streams */
#define  HDA_GCAP_BSS(g)    (((g) >> 3) & 0x1F)    /* bidirectional streams */
#define HDA_VMIN            0x02
#define HDA_VMAJ            0x03
#define HDA_GCTL            0x08    /* 32 */
#define  HDA_GCTL_CRST      0x00000001
#define  HDA_GCTL_UNSOL     0x00000100
#define HDA_WAKEEN          0x0C    /* 16 */
#define HDA_STATESTS        0x0E    /* 16: a codec answered the reset */
#define HDA_INTCTL          0x20    /* 32 */
#define  HDA_INTCTL_GIE     0x80000000
#define  HDA_INTCTL_CIE     0x40000000
#define HDA_INTSTS          0x24    /* 32 */
#define  HDA_INTSTS_GIS     0x80000000
#define  HDA_INTSTS_CIS     0x40000000
#define HDA_WALCLK          0x30    /* 32: 24 MHz wall clock */
#define HDA_SSYNC           0x38    /* 32 */
#define HDA_CORBLBASE       0x40
#define HDA_CORBUBASE       0x44
#define HDA_CORBWP          0x48    /* 16 */
#define HDA_CORBRP          0x4A    /* 16 */
#define  HDA_CORBRP_RST     0x8000
#define HDA_CORBCTL         0x4C    /* 8 */
#define  HDA_CORBCTL_RUN    0x02
#define HDA_CORBSTS         0x4D    /* 8 */
#define HDA_CORBSIZE        0x4E    /* 8 */
#define HDA_RIRBLBASE       0x50
#define HDA_RIRBUBASE       0x54
#define HDA_RIRBWP          0x58    /* 16 */
#define  HDA_RIRBWP_RST     0x8000
#define HDA_RINTCNT         0x5A    /* 16 */
#define HDA_RIRBCTL         0x5C    /* 8 */
#define  HDA_RIRBCTL_INT    0x01
#define  HDA_RIRBCTL_RUN    0x02
#define  HDA_RIRBCTL_OIC    0x04
#define HDA_RIRBSTS         0x5D    /* 8 */
#define  HDA_RIRBSTS_INTFL  0x01
#define  HDA_RIRBSTS_OIS    0x04
#define HDA_RIRBSIZE        0x5E    /* 8 */
/* CORBSIZE and RIRBSIZE: bits 7:4 what the controller can have, 1:0 the
   size chosen (0: 2 entries, 1: 16, 2: 256). */
#define  HDA_RINGSIZE_CAP256    0x40
#define  HDA_RINGSIZE_CAP16     0x20
#define  HDA_RINGSIZE_256       0x02
#define  HDA_RINGSIZE_16        0x01
#define  HDA_RINGSIZE_2         0x00
#define HDA_DPLBASE         0x70    /* DMA position buffer */
#define  HDA_DPLBASE_ENABLE 0x00000001
#define HDA_DPUBASE         0x74

/* ---- Stream descriptors (inputs first, then outputs, then both) -------- */

#define HDA_SD_BASE         0x80
#define HDA_SD_SIZE         0x20
#define HDA_SD_CTL          0x00    /* 24 bits (and STS in the next byte) */
#define  HDA_SDCTL_SRST     0x000001
#define  HDA_SDCTL_RUN      0x000002
#define  HDA_SDCTL_IOCE     0x000004
#define  HDA_SDCTL_FEIE     0x000008
#define  HDA_SDCTL_DEIE     0x000010
#define  HDA_SDCTL_STRM(t)  ((uint32_t)(t) << 20)
#define HDA_SD_STS          0x03    /* 8 */
#define  HDA_SDSTS_BCIS     0x04
#define  HDA_SDSTS_FIFOE    0x08
#define  HDA_SDSTS_DESE     0x10
#define  HDA_SDSTS_FIFORDY  0x20
#define HDA_SD_LPIB         0x04    /* 32: link position in the buffer */
#define HDA_SD_CBL          0x08    /* 32: cyclic buffer length */
#define HDA_SD_LVI          0x0C    /* 16: last valid index */
#define HDA_SD_FIFOS        0x10    /* 16 */
#define HDA_SD_FMT          0x12    /* 16 */
#define HDA_SD_BDPL         0x18
#define HDA_SD_BDPU         0x1C

/* A buffer descriptor list entry. */
struct HDABufferDescriptor {
    uint64_t address;
    uint32_t length;
    uint32_t flags;                 /* bit 0: interrupt on completion */
} __attribute__((packed));

/* A response in the RIRB. */
struct HDAResponse {
    uint32_t response;
    uint32_t extended;              /* bits 3:0 codec, bit 4 unsolicited */
} __attribute__((packed));

#define HDA_RESPONSE_UNSOL  0x10

/* ---- Codec verbs ------------------------------------------------------- */

/* A command: codec address, node, then a 12-bit verb with an 8-bit
   payload, or a 4-bit verb with a 16-bit payload. */
#define HDA_CMD12(cad, nid, verb, payload) \
    (((uint32_t)(cad) << 28) | ((uint32_t)(nid) << 20) | \
     ((uint32_t)(verb) << 8) | ((uint32_t)(payload) & 0xFF))
#define HDA_CMD4(cad, nid, verb, payload) \
    (((uint32_t)(cad) << 28) | ((uint32_t)(nid) << 20) | \
     ((uint32_t)(verb) << 16) | ((uint32_t)(payload) & 0xFFFF))

#define HDA_VERB_GET_PARAMETER          0xF00
#define HDA_VERB_GET_CONNECTION_SELECT  0xF01
#define HDA_VERB_SET_CONNECTION_SELECT  0x701
#define HDA_VERB_GET_CONNECTION_LIST    0xF02
#define HDA_VERB_GET_POWER_STATE        0xF05
#define HDA_VERB_SET_POWER_STATE        0x705
#define HDA_VERB_GET_CONVERTER_STREAM   0xF06
#define HDA_VERB_SET_CONVERTER_STREAM   0x706
#define HDA_VERB_GET_PIN_CONTROL        0xF07
#define HDA_VERB_SET_PIN_CONTROL        0x707
#define  HDA_PIN_CONTROL_IN             0x20
#define  HDA_PIN_CONTROL_OUT            0x40
#define  HDA_PIN_CONTROL_HP             0x80
#define HDA_VERB_GET_EAPD               0xF0C
#define HDA_VERB_SET_EAPD               0x70C
#define  HDA_EAPD_ENABLE                0x02
#define HDA_VERB_GET_CONFIG_DEFAULT     0xF1C
#define HDA_VERB_FUNCTION_RESET         0x7FF
/* 4-bit verbs. */
#define HDA_VERB_SET_CONVERTER_FORMAT   0x2
#define HDA_VERB_GET_CONVERTER_FORMAT   0xA
#define HDA_VERB_SET_AMP                0x3
#define  HDA_AMP_SET_OUTPUT             0x8000
#define  HDA_AMP_SET_INPUT              0x4000
#define  HDA_AMP_SET_LEFT               0x2000
#define  HDA_AMP_SET_RIGHT              0x1000
#define  HDA_AMP_SET_INDEX(i)           ((uint32_t)(i) << 8)
#define  HDA_AMP_MUTE                   0x0080
#define HDA_VERB_GET_AMP                0xB
#define  HDA_AMP_GET_OUTPUT             0x8000
#define  HDA_AMP_GET_LEFT               0x2000

/* Parameters (GET_PARAMETER). */
#define HDA_PARAM_VENDOR_ID             0x00
#define HDA_PARAM_REVISION_ID           0x02
#define HDA_PARAM_SUB_NODE_COUNT        0x04
#define  HDA_SUB_NODE_START(r)          (((r) >> 16) & 0xFF)
#define  HDA_SUB_NODE_TOTAL(r)          ((r) & 0xFF)
#define HDA_PARAM_FUNCTION_GROUP_TYPE   0x05
#define  HDA_FG_TYPE(r)                 ((r) & 0xFF)
#define  HDA_FG_TYPE_AUDIO              0x01
#define  HDA_FG_TYPE_MODEM              0x02
#define HDA_PARAM_AUDIO_FG_CAPS         0x08
#define HDA_PARAM_WIDGET_CAPS           0x09
#define  HDA_WCAP_TYPE(r)               (((r) >> 20) & 0xF)
#define  HDA_WCAP_STEREO                0x00000001
#define  HDA_WCAP_IN_AMP                0x00000002
#define  HDA_WCAP_OUT_AMP               0x00000004
#define  HDA_WCAP_AMP_OVERRIDE          0x00000008
#define  HDA_WCAP_FORMAT_OVERRIDE       0x00000010
#define  HDA_WCAP_CONN_LIST             0x00000100
#define  HDA_WCAP_DIGITAL               0x00000200
#define  HDA_WCAP_POWER                 0x00000400
#define HDA_PARAM_PCM                   0x0A
#define HDA_PARAM_STREAM_FORMATS        0x0B
#define HDA_PARAM_PIN_CAPS              0x0C
#define  HDA_PINCAP_OUTPUT              0x00000010
#define  HDA_PINCAP_INPUT               0x00000020
#define  HDA_PINCAP_HP                  0x00000008
#define  HDA_PINCAP_EAPD                0x00010000
#define HDA_PARAM_IN_AMP_CAPS           0x0D
#define HDA_PARAM_CONN_LIST_LENGTH      0x0E
#define  HDA_CONN_LONG_FORM             0x80
#define HDA_PARAM_POWER_STATES          0x0F
#define HDA_PARAM_OUT_AMP_CAPS          0x12
#define  HDA_AMPCAP_OFFSET(r)           ((r) & 0x7F)
#define  HDA_AMPCAP_STEPS(r)            (((r) >> 8) & 0x7F)

/* Widget types (WIDGET_CAPS bits 23:20). */
enum {
    HDA_WIDGET_OUTPUT = 0,          /* a DAC */
    HDA_WIDGET_INPUT = 1,           /* an ADC */
    HDA_WIDGET_MIXER = 2,
    HDA_WIDGET_SELECTOR = 3,
    HDA_WIDGET_PIN = 4,
    HDA_WIDGET_POWER = 5,
    HDA_WIDGET_VOLUME_KNOB = 6,
    HDA_WIDGET_BEEP = 7,
    HDA_WIDGET_VENDOR = 15,
};

/* A pin's default configuration (CONFIG_DEFAULT). */
#define HDA_CONFIG_CONNECTIVITY(c)      (((c) >> 30) & 0x3)  /* 1: none */
#define  HDA_CONNECT_JACK               0
#define  HDA_CONNECT_NONE               1
#define  HDA_CONNECT_FIXED              2
#define  HDA_CONNECT_BOTH               3
#define HDA_CONFIG_DEVICE(c)            (((c) >> 20) & 0xF)
#define  HDA_DEVICE_LINE_OUT            0x0
#define  HDA_DEVICE_SPEAKER             0x1
#define  HDA_DEVICE_HP_OUT              0x2
#define  HDA_DEVICE_SPDIF_OUT           0x4
#define  HDA_DEVICE_DIGITAL_OUT         0x5
#define  HDA_DEVICE_LINE_IN             0x8
#define  HDA_DEVICE_MIC_IN              0xA
#define HDA_CONFIG_ASSOCIATION(c)       (((c) >> 4) & 0xF)
#define HDA_CONFIG_SEQUENCE(c)          ((c) & 0xF)

/* A stream format (SDnFMT and SET_CONVERTER_FORMAT). */
#define HDA_FORMAT_BASE_44K             0x4000
#define HDA_FORMAT_MULT(m)              ((uint16_t)((m) - 1) << 11)
#define HDA_FORMAT_DIV(d)               ((uint16_t)((d) - 1) << 8)
#define HDA_FORMAT_BITS_16              (1 << 4)
#define HDA_FORMAT_BITS_20              (2 << 4)
#define HDA_FORMAT_BITS_24              (3 << 4)
#define HDA_FORMAT_BITS_32              (4 << 4)
#define HDA_FORMAT_CHANNELS(n)          ((uint16_t)((n) - 1))

#endif /* _MINKAR_HDA_H_ */
