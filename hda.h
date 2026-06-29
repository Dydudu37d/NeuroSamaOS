#pragma once

#include "int.h"

typedef struct HdaRegs {
    volatile u32 Gcap;
    volatile u8 Vmin;
    volatile u8 Vmaj;
    volatile u16 Outpay;
    volatile u16 Inpay;
    volatile u8 Gctl;
    volatile u8 Wakeen;
    volatile u16 Statests;
    volatile u32 Gsts;
    volatile u32 Outbase;
    volatile u32 Outbase_hi;
    volatile u32 Inbase;
    volatile u32 Inbase_hi;
    volatile u32 Dibbase;
    volatile u32 Dibbase_hi;
    volatile u32 Rirbwp;
    volatile u32 Rirbcnt;
} __attribute__((packed)) HdaRegs;

typedef struct HdaStreamRegs {
    volatile u32 Ctl;
    volatile u32 Ctl_hi;
    volatile u32 Lpib;
    volatile u32 Cvi;
    volatile u32 Lvi;
    volatile u32 Fmt;
    volatile u32 Bdlp;
    volatile u32 Bdlp_hi;
} __attribute__((packed)) HdaStreamRegs;

typedef struct HdaBdlEntry {
    u64 Address;
    u32 Length;
    u32 Reserved;
} __attribute__((packed)) HdaBdlEntry;

typedef struct HdaController {
    u64 MmioBase;
    HdaRegs* Regs;
    HdaStreamRegs* Streams;
    u8 HaveCodec;
    u8 OutputStream;
    u64 BdlPhys;
    HdaBdlEntry* Bdl;
    u8* AudioBuffer;
    u32 OutputNid;
} HdaController;

HdaController* HdaInit(u64 MmioBase);
void HdaPlay(HdaController* hda, u16* samples, u32 count);
void HdaSetVolume(HdaController* hda, u8 volume);
