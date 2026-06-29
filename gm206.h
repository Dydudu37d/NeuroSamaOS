#pragma once

#define NVIDIA_VENDOR_ID 0x10DE
#define GTX960_DEVICE_ID 0x1401

#define PCI_CLASS_DISPLAY 0x03
#define PCI_SUBCLASS_VGA 0x00
#define PCI_SUBCLASS_3D 0x02

#define NVIDIA_PMC_BOOT_0 0x00000000
#define NVIDIA_BOOTED 0x0000C0DE
#define NVIDIA_PMC_ENABLE  0x00000200
#define NVIDIA_PMC_ENABLE_ENABLE (1 << 0)
#define NVIDIA_GR_RESET 0x00400000

#define NVIDIA_PFIFO_INTR       0x00002000
#define NVIDIA_PFIFO_INTR_EN    0x00002004
#define NVIDIA_PFIFO_RUNLIST    0x00002010
#define NVIDIA_PFIFO_STATUS     0x00002014

#define NVIDIA_PFIFO_PUSHBUF_BASE    0x00002040
#define NVIDIA_PFIFO_PUSHBUF_LIMIT   0x00002044
#define NVIDIA_PFIFO_PUSHBUF_PUT     0x00002048
#define NVIDIA_PFIFO_PUSHBUF_GET     0x0000204C

#define NVIDIA_PGRAPH_STATUS    0x00400000
#define NVIDIA_PGRAPH_INTR      0x00400100
#define NVIDIA_PGRAPH_INTR_EN   0x00400140
#include "int.h"

typedef struct{
    u8 Bus;
    u8 Slot;
    u8 Func;
    u64 Bar0Base;
    u64 Bar0Size;
    u32 BootStatus;
    u32 GrStatus;
    u64 VramSize;
    u64 VramBase;
} NvidiaGPU;

_Bool NvidiaGPUInit(NvidiaGPU *GPU);
_Bool PCIFindNvidiaGPU(NvidiaGPU *GPU);
u64 PCIGetNvidiaGPUBar64(u8 Bus,u8 Slot,u8 Func,u8 BARIndex);
u64 PCIGetNvidiaGPUBarSize64(u8 Bus,u8 Slot,u8 Func,u8 BARIndex);

static inline u32 NvidiaGPURead32(NvidiaGPU *GPU,u32 Offset){return *((volatile u32*)(u64)(GPU->Bar0Base+Offset));}
static inline void NvidiaGPUWrite32(NvidiaGPU *GPU,u32 Offset,u32 Val){*((volatile u32*)(u64)(GPU->Bar0Base+Offset))=Val;}

