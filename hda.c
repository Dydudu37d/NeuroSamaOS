#include "hda.h"
#include "kmalloc.h"
#include "str.h"
#include "debug.h"

extern AllocPool KernelPool;

static u32 HdaSendVerb(HdaController *hda, u8 codec, u8 nid, u32 verb, u32 payload)
{
    volatile u32 *ic = (volatile u32 *)(hda->MmioBase + 0x60);
    volatile u16 *ics = (volatile u16 *)(hda->MmioBase + 0x64);

    while (*ics & (1 << 0))
        __asm__ volatile("pause");
    *ics = (1 << 1);
    *ic = ((u32)codec << 28) | ((u32)nid << 20) | (verb << 8) | (payload & 0xFF);
    *ics = (1 << 0);

    for (int i = 0; i < 500000; i++)
    {
        if (!(*ics & (1 << 0)) && (*ics & (1 << 1)))
        {
            return *ic;
        }
        __asm__ volatile("pause");
    }
    return 0;
}

static u8 HdaFindOutputNid(HdaController *hda, u8 codec)
{
    u32 func_group = HdaSendVerb(hda, codec, 0, 0xF00, 0x04);
    u8 start_nid = (func_group >> 16) & 0xFF;
    u8 nid_count = func_group & 0xFF;

    for (u8 nid = start_nid; nid < (start_nid + nid_count); nid++)
    {
        u32 widget = HdaSendVerb(hda, codec, nid, 0xF00, 0x09);
        u8 widget_type = (widget >> 20) & 0x0F;

        if (widget_type == 0x00)
        {
            hda->OutputNid = nid;
            return nid;
        }
    }
    return 0;
}

static void HdaSetStreamFormat(HdaController *hda, u8 stream, u16 rate, u8 bits, u8 channels)
{
    HdaStreamRegs *s = (HdaStreamRegs *)((u8 *)hda->Streams + stream * 0x20);
    u16 fmt = 0;

    fmt |= (channels - 1) & 0x0F;

    if (bits == 16)
        fmt |= (1 << 4);
    else if (bits == 24)
        fmt |= (2 << 4);
    else if (bits == 32)
        fmt |= (3 << 4);

    if (rate == 44100)
    {
        fmt |= (1 << 14);
    }
    else if (rate == 48000)
    {
        fmt |= 0;
    }
    s->Fmt = fmt;
}

static void HdaStartStream(HdaController *hda, u8 stream, u8 codec, u8 nid, u8 *buffer, u32 size)
{
    if (size == 0 || !buffer)
        return;
    size &= ~3;

    HdaStreamRegs *s = (HdaStreamRegs *)((u8 *)hda->Streams + stream * 0x20);
    HdaBdlEntry *bdl = hda->Bdl;

    s->Ctl[0] &= ~(1 << 0);
    while (s->Ctl[0] & (1 << 0))
        __asm__ volatile("pause");

    u32 num_pages = (size + 4095) / 4096;
    for (u32 i = 0; i < num_pages && i < 256; i++)
    {
        bdl[i].Address = (u64)buffer + i * 4096;
        bdl[i].Length = (i == (num_pages - 1)) ? (size - (i * 4096)) : 4096;
        bdl[i].Reserved = 0;
    }

    s->Bdlp = (u32)(hda->BdlPhys);
    s->Bdlp_hi = (u32)(hda->BdlPhys >> 32);
    s->Lvi = num_pages - 1;
    s->Cvi = 0;

    HdaSetStreamFormat(hda, stream, 44100, 16, 2);

    HdaSendVerb(hda, codec, nid, 0x706, (stream << 4) | 0);

    s->Ctl[0] |= (1 << 0);
}

HdaController *HdaInit(u64 MmioBase)
{
    HdaController *hda = (HdaController *)AlignedAlloc(&KernelPool, sizeof(HdaController), 64);
    MemSet(hda, 0, sizeof(HdaController));

    hda->MmioBase = MmioBase;
    hda->Regs = (HdaRegs *)MmioBase;
    hda->Streams = (HdaStreamRegs *)(MmioBase + 0x80);

    hda->Regs->Gctl &= ~1;
    while (hda->Regs->Gctl & 1)
        __asm__ volatile("pause");
    for (int i = 0; i < 100000; i++)
        __asm__ volatile("pause");

    hda->Regs->Gctl |= 1;
    while (!(hda->Regs->Gctl & 1))
        __asm__ volatile("pause");
    for (int i = 0; i < 100000; i++)
        __asm__ volatile("pause");

    hda->Regs->Intctl &= ~(1 << 31);

    u16 statests = hda->Regs->Statests;
    for (int i = 0; i < 15; i++)
    {
        if (statests & (1 << i))
        {
            hda->HaveCodec = 1;
            hda->CodecAddr = i;
            DebugStr("HDA: Codec found at address ");
            DebugU8(i);
            DebugChar('\n');
            break;
        }
    }

    if (!hda->HaveCodec)
    {
        DebugStr("HDA: No codec found\n");
        return hda;
    }

    u8 nid = HdaFindOutputNid(hda, hda->CodecAddr);
    DebugStr("HDA: Output NID = ");
    DebugU8(nid);
    DebugChar('\n');

    hda->OutputStream = 1;

    hda->AudioBuffer = (u8 *)AlignedAlloc(&KernelPool, 65536, 4096);
    MemSet(hda->AudioBuffer, 0, 65536);

    hda->Bdl = (HdaBdlEntry *)AlignedAlloc(&KernelPool, 256 * sizeof(HdaBdlEntry), 128);
    hda->BdlPhys = (u64)hda->Bdl;

    HdaStartStream(hda, hda->OutputStream, hda->CodecAddr, nid, hda->AudioBuffer, 65536);

    HdaSendVerb(hda, hda->CodecAddr, nid, 0x300, 0xB07F);

    DebugStr("HDA: Init done\n");

    return hda;
}

void HdaPlay(HdaController *hda, u16 *samples, u32 count)
{
    if (!hda->HaveCodec)
        return;

    u32 bytes = count * 2;
    if (bytes > 65536)
        bytes = 65536;
    bytes &= ~3;

    MemCopy(hda->AudioBuffer, samples, bytes);

    HdaStreamRegs *s = (HdaStreamRegs *)((u8 *)hda->Streams + hda->OutputStream * 0x20);
    s->Ctl[0] &= ~(1 << 0);
    while (s->Ctl[0] & (1 << 0))
        __asm__ volatile("pause");

    s->Lvi = (bytes + 4095) / 4096 - 1;
    s->Cvi = 0;
    s->Sts = 0x1C;
    s->Ctl[0] |= (1 << 0);
}

u8 HdaIsPlaying(HdaController *hda)
{
    HdaStreamRegs *s = (HdaStreamRegs *)((u8 *)hda->Streams + hda->OutputStream * 0x20);
    if (s->Cvi <= s->Lvi)
    {
        return 1;
    }
    return 0;
}

void HdaWait(HdaController *hda)
{
    while (HdaIsPlaying(hda))
    {
        __asm__ volatile("pause");
    }
}

void HdaSetVolume(HdaController *hda, u8 volume)
{
    if (!hda->HaveCodec)
        return;

    u8 gain = (u8)(63.0 * (volume / 100.0) * (volume / 100.0) + 0.5);
    if (gain > 63)
        gain = 63;

    u32 amp = gain | (1 << 12) | (1 << 13) | (1 << 15);
    HdaSendVerb(hda, hda->CodecAddr, hda->OutputNid, 0x300, amp);
}

u64 GetHdaMMIO()
{
    u8 Func, Slot, Bus;
    PCIFindDeviceByClass(0x04, 0x03, 0x00, &Bus, &Slot, &Func);
    return PCIGetBARAddress(Bus, Slot, Func, 0);
}
