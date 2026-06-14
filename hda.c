#include "hda.h"
#include "kmalloc.h"
#include "str.h"
#include "debug.h"

extern AllocPool KernelPool;

static u32 HdaSendVerb(HdaController* hda, u8 codec, u8 nid, u32 verb, u32 payload) {
    volatile u32* ic = (volatile u32*)(hda->MmioBase + 0x60);
    volatile u16* ics = (volatile u16*)(hda->MmioBase + 0x64);

    while (*ics & (1 << 0)) __asm__ volatile ("pause");
    *ics = (1 << 1);
    *ic = (codec << 28) | (nid << 20) | (verb << 8) | payload;
    *ics = (1 << 0);
    
    for (int i = 0; i < 100000; i++) {
        if (!(*ics & (1 << 0)) && (*ics & (1 << 1))) {
            return *ic;
        }
        __asm__ volatile ("pause");
    }
    return 0;
}

static u8 HdaFindOutputNid(HdaController* hda, u8 codec)
{
    u32 func_group = HdaSendVerb(hda, codec, 0, 0xF00, 0);
    u8 nid_count = (func_group >> 16) & 0xFF;
    
    for (u8 nid = 1; nid <= nid_count; nid++)
    {
        u32 widget = HdaSendVerb(hda, codec, nid, 0xF00, 0x09);
        u8 widget_type = (widget >> 16) & 0xFF;
        
        if (widget_type == 0x04)
        {
            u32 pin_cap = HdaSendVerb(hda, codec, nid, 0xF00, 0x12);
            hda->OutputNid=pin_cap;
            
            if (pin_cap & (1 << 5))
            {
                return nid;
            }
        }
    }
    return 0;
}

static void HdaSetStreamFormat(HdaController* hda, u8 stream, u16 rate, u8 bits, u8 channels)
{
    HdaStreamRegs* s = &hda->Streams[stream];
    
    u32 fmt = 0;
    fmt |= (channels - 1) << 14;
    fmt |= (bits - 8) << 11;
    fmt |= rate;
    
    s->Fmt = fmt;
}

static void HdaStartStream(HdaController* hda, u8 stream, u8 codec, u8 nid, u8* buffer, u32 size)
{
    HdaStreamRegs* s = &hda->Streams[stream];
    HdaBdlEntry* bdl = hda->Bdl;
    
    u32 num_pages = (size + 4095) / 4096;
    for (u32 i = 0; i < num_pages && i < 256; i++)
    {
        bdl[i].Address = (u64)buffer + i * 4096;
        bdl[i].Length = 4096;
        bdl[i].Reserved = 0;
    }
    bdl[num_pages - 1].Length = size - (num_pages - 1) * 4096;
    
    s->Bdlp = (u32)(hda->BdlPhys);
    s->Bdlp_hi = (u32)(hda->BdlPhys >> 32);
    s->Lvi = num_pages - 1;
    s->Cvi = 0;
    
    HdaSetStreamFormat(hda, stream, 44100, 16, 2);
    
    HdaSendVerb(hda, codec, nid, 0x706, (1 << 8) | stream);
    
    s->Ctl = (1 << 1) | (1 << 2);
}

HdaController* HdaInit(u64 MmioBase)
{
    HdaController* hda = (HdaController*)AlignedAlloc(&KernelPool, sizeof(HdaController), 64);
    MemSet(hda, 0, sizeof(HdaController));
    
    hda->MmioBase = MmioBase;
    hda->Regs = (HdaRegs*)MmioBase;
    hda->Streams = (HdaStreamRegs*)(MmioBase + 0x80);
    
    hda->Regs->Gctl = 0;
    for (int i = 0; i < 100000; i++) __asm__ volatile ("pause");
    hda->Regs->Gctl = 1;
    for (int i = 0; i < 100000; i++) __asm__ volatile ("pause");
    
    u16 statests = hda->Regs->Statests;
    for (int i = 0; i < 15; i++)
    {
        if (statests & (1 << i))
        {
            hda->HaveCodec = 1;
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
    
    u8 nid = HdaFindOutputNid(hda, 0);
    DebugStr("HDA: Output NID = ");
    DebugU8(nid);
    DebugChar('\n');
    
    hda->OutputStream = 1;
    
    hda->AudioBuffer = (u8*)AlignedAlloc(&KernelPool, 65536, 4096);
    MemSet(hda->AudioBuffer, 0, 65536);
    
    hda->Bdl = (HdaBdlEntry*)AlignedAlloc(&KernelPool, 256 * sizeof(HdaBdlEntry), 128);
    hda->BdlPhys = (u64)hda->Bdl;
    
    HdaStartStream(hda, hda->OutputStream, 0, nid, hda->AudioBuffer, 65536);
    
    HdaSendVerb(hda, 0, nid, 0x708, 0x8080);
    
    DebugStr("HDA: Init done\n");
    
    return hda;
}

void HdaPlay(HdaController* hda, u16* samples, u32 count)
{
    if (!hda->HaveCodec) return;
    
    u32 bytes = count * 2;
    if (bytes > 65536) bytes = 65536;
    
    MemCopy(hda->AudioBuffer, samples, bytes);
    
    HdaStreamRegs* s = &hda->Streams[hda->OutputStream];
    s->Lvi = (bytes + 4095) / 4096 - 1;
    s->Cvi = 0;
    s->Ctl |= (1 << 0);
}

void HdaSetVolume(HdaController* hda, u8 volume)
{
    if (!hda->HaveCodec) return;
    
    u8 gain = (volume * 63) / 100;
    if (gain > 63) gain = 63;

    u32 amp = gain | (1 << 12) | (1 << 13) | (1 << 15);
    HdaSendVerb(hda, 0, hda->OutputNid, 0x708, amp);
}
