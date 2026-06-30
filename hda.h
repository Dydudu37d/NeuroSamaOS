#pragma once

#include "int.h"
#include "pci.h"

typedef struct HdaRegs
{
    volatile u32 Gcap;
    volatile u8 Vmin;
    volatile u8 Vmaj;
    volatile u16 Outpay;
    volatile u16 Inpay;
    volatile u32 Gctl;
    volatile u16 Wakeen;
    volatile u16 Statests;
    volatile u16 Gsts;
    volatile u8 Reserved[6];
    volatile u32 Intctl;
    volatile u32 Intsts;
    volatile u8 Reserved2[8];
    volatile u32 Walclk;
    volatile u8 Reserved3[4];
    volatile u32 Ssync;
    volatile u8 Reserved4[4];
    volatile u32 Corblbase;
    volatile u32 Corbubase;
    volatile u16 Corbwp;
    volatile u16 Corbrp;
    volatile u8 Corbctl;
    volatile u8 Corbsts;
    volatile u8 Corbsize;
    volatile u8 Reserved5[3];
    volatile u32 Rirblbase;
    volatile u32 Rirbubbase;
    volatile u16 Rirbwp;
    volatile u16 Rintcnt;
    volatile u8 Rirbctl;
    volatile u8 Rirbsts;
    volatile u8 Rirbsize;
    volatile u8 Reserved6[5];
    volatile u32 Ico;
    volatile u32 Ici;
    volatile u16 Ics;
} __attribute__((packed)) HdaRegs;

typedef struct HdaStreamRegs
{
    volatile u8 Ctl[3];
    volatile u8 Sts;
    volatile u32 Lpib;
    volatile u16 Cvi;
    volatile u16 Reserved;
    volatile u16 Lvi;
    volatile u16 Reserved2;
    volatile u16 Fmt;
    volatile u16 Reserved3;
    volatile u32 Bdlp;
    volatile u32 Bdlp_hi;
} __attribute__((packed)) HdaStreamRegs;

typedef struct HdaBdlEntry
{
    u64 Address;
    u32 Length;
    u32 Reserved;
} __attribute__((packed)) HdaBdlEntry;

typedef struct HdaController
{
    u64 MmioBase;
    HdaRegs *Regs;
    HdaStreamRegs *Streams;
    u8 HaveCodec;
    u8 OutputStream;
    u64 BdlPhys;
    HdaBdlEntry *Bdl;
    u8 *AudioBuffer;
    u32 OutputNid;
    u8 CodecAddr;
} HdaController;

HdaController *HdaInit(u64 MmioBase);
void HdaPlay(HdaController *hda, u16 *samples, u32 count);
void HdaSetVolume(HdaController *hda, u8 volume);
u64 GetHdaMMIO();
u8 HdaIsPlaying(HdaController *hda);
void HdaWait(HdaController *hda);
