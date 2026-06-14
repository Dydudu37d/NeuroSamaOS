#include "ohci.h"
#include "kmalloc.h"
#include "str.h"
#include "debug.h"

extern AllocPool KernelPool;

static u8 OhciControlTransfer(OhciController* ohci, u8 addr, u8 bmRequestType, u8 bRequest, u16 wValue, u16 wIndex, u16 wLength, u8* data)
{
    OhciTd* setupTd = (OhciTd*)AlignedAlloc(&KernelPool, sizeof(OhciTd), 16);
    OhciTd* dataTd = (OhciTd*)AlignedAlloc(&KernelPool, sizeof(OhciTd), 16);
    OhciTd* statusTd = (OhciTd*)AlignedAlloc(&KernelPool, sizeof(OhciTd), 16);

    if (!setupTd || !dataTd || !statusTd) {
        if (setupTd) Free(&KernelPool, setupTd);
        if (dataTd) Free(&KernelPool, dataTd);
        if (statusTd) Free(&KernelPool, statusTd);
        return 0;
    }

    MemSet(setupTd, 0, sizeof(OhciTd));
    MemSet(dataTd, 0, sizeof(OhciTd));
    MemSet(statusTd, 0, sizeof(OhciTd));

    u8* setupBuf = (u8*)AlignedAlloc(&KernelPool, 8, 16);
    setupBuf[0] = bmRequestType;
    setupBuf[1] = bRequest;
    setupBuf[2] = wValue & 0xFF;
    setupBuf[3] = (wValue >> 8) & 0xFF;
    setupBuf[4] = wIndex & 0xFF;
    setupBuf[5] = (wIndex >> 8) & 0xFF;
    setupBuf[6] = wLength & 0xFF;
    setupBuf[7] = (wLength >> 8) & 0xFF;

    setupTd->Control = 0x20000000 | (0x2D << 8) | (0 << 24);
    setupTd->CurrentBuffer = (u32)(u64)setupBuf;
    setupTd->BufferEnd = (u32)(u64)(setupBuf + 7);
    setupTd->Next = (u32)(u64)statusTd;

    u8* dataBuf = NULL;
    if (wLength > 0 && data) {
        dataBuf = (u8*)AlignedAlloc(&KernelPool, wLength, 16);
        if (bmRequestType & 0x80) {
            dataTd->Control = 0x20000000 | (0x25 << 8) | (1 << 24);
        } else {
            MemCopy(dataBuf, data, wLength);
            dataTd->Control = 0x20000000 | (0x21 << 8) | (1 << 24);
        }
        dataTd->CurrentBuffer = (u32)(u64)dataBuf;
        dataTd->BufferEnd = (u32)(u64)(dataBuf + wLength - 1);
        dataTd->Next = (u32)(u64)statusTd;
        setupTd->Next = (u32)(u64)dataTd;
    }

    if (bmRequestType & 0x80) {
        statusTd->Control = 0x20000000 | (0x21 << 8) | (1 << 24);
    } else {
        statusTd->Control = 0x20000000 | (0x25 << 8) | (1 << 24);
    }
    statusTd->CurrentBuffer = 0;
    statusTd->BufferEnd = 0;
    statusTd->Next = 0;

    OhciEd* controlEd = ohci->ControlEd;
    controlEd->Control = (addr & 0x7F) | (8 << 16);
    controlEd->Head = (u32)(u64)setupTd;
    controlEd->Tail = 0;

    ohci->Regs->HcCommandStatus |= 2;

    u32 timeout = 1000000;
    u8 success = 0;
    while (timeout--) {
        if ((ohci->Hcca->HccaDoneHead & ~1) != 0) {
            u32 dh = ohci->Hcca->HccaDoneHead & ~1;
            if (dh == (u32)(u64)statusTd || dh == (u32)(u64)dataTd || dh == (u32)(u64)setupTd) {
                if (wLength > 0 && data && (bmRequestType & 0x80)) {
                    MemCopy(data, dataBuf, wLength);
                }
                ohci->Hcca->HccaDoneHead = 0;
                ohci->Regs->HcInterruptStatus = 2;
                success = 1;
                break;
            }
        }
        __asm__ volatile ("pause");
    }

    if (dataBuf) Free(&KernelPool, dataBuf);
    Free(&KernelPool, setupBuf);
    Free(&KernelPool, setupTd);
    Free(&KernelPool, dataTd);
    Free(&KernelPool, statusTd);

    return success;
}

static void OhciSetAddress(OhciController* ohci, u8 oldAddr, u8 newAddr)
{
    OhciControlTransfer(ohci, oldAddr, 0x00, 0x05, newAddr, 0, 0, NULL);
}

static u8 OhciGetDeviceDescriptor(OhciController* ohci, u8 addr, u8* buf)
{
    return OhciControlTransfer(ohci, addr, 0x80, 0x06, 0x0100, 0, 8, buf);
}

static u8 OhciSetConfiguration(OhciController* ohci, u8 addr, u8 config)
{
    return OhciControlTransfer(ohci, addr, 0x00, 0x09, config, 0, 0, NULL);
}

static u8 OhciSetProtocol(OhciController* ohci, u8 addr, u8 protocol)
{
    return OhciControlTransfer(ohci, addr, 0x21, 0x0B, protocol, 0, 0, NULL);
}

static u8 OhciSetIdle(OhciController* ohci, u8 addr, u8 idle)
{
    return OhciControlTransfer(ohci, addr, 0x21, 0x0A, idle, 0, 0, NULL);
}

OhciController* OhciInit(u64 MmioBase)
{
    if ((MmioBase & 0xFFFFFFFF00000000ULL) != 0) {
        return NULL;
    }

    OhciController* ohci = (OhciController*)AlignedAlloc(&KernelPool, sizeof(OhciController), 64);
    MemSet(ohci, 0, sizeof(OhciController));

    ohci->MmioBase = MmioBase;
    ohci->Regs = (OhciRegs*)MmioBase;

    ohci->Regs->HcControl = 0;
    ohci->Regs->HcCommandStatus = 1;
    u32 timeout = 100000;
    while ((ohci->Regs->HcCommandStatus & 1) && timeout--) {
        __asm__ volatile ("pause");
    }

    ohci->Hcca = (OhciHcca*)AlignedAlloc(&KernelPool, sizeof(OhciHcca), 256);
    if ((u64)ohci->Hcca & 0xFFFFFFFF00000000ULL) {
        return NULL;
    }
    MemSet(ohci->Hcca, 0, sizeof(OhciHcca));
    ohci->Regs->HcHCCA = (u32)(u64)ohci->Hcca;

    ohci->ControlEd = (OhciEd*)AlignedAlloc(&KernelPool, sizeof(OhciEd), 16);
    ohci->BulkEd = (OhciEd*)AlignedAlloc(&KernelPool, sizeof(OhciEd), 16);
    MemSet(ohci->ControlEd, 0, sizeof(OhciEd));
    MemSet(ohci->BulkEd, 0, sizeof(OhciEd));

    ohci->ControlEd->Control = (0 << 8) | (0 << 5) | (8 << 16);
    ohci->BulkEd->Control = (0 << 8) | (0 << 5) | (8 << 16);

    ohci->Regs->HcControlHeadED = (u32)(u64)ohci->ControlEd;
    ohci->Regs->HcBulkHeadED = (u32)(u64)ohci->BulkEd;

    ohci->Regs->HcFmInterval = (12000 << 16) | 0x2EDF;
    ohci->Regs->HcPeriodicStart = 0x25E0;
    ohci->Regs->HcLSThreshold = 0x02A8;

    ohci->Regs->HcControl = 0x80;
    ohci->Regs->HcInterruptEnable = 0x80000042;

    u8 num_ports = ohci->Regs->HcRhDescriptorA & 0xFF;

    for (u8 port = 0; port < num_ports; port++) {
        volatile u32* portsc = &ohci->Regs->HcRhPortStatus[port];
        u32 status = *portsc;

        if (status & 1) {
            *portsc = 0x100;
            for (volatile u32 i = 0; i < 2000000; i++) __asm__ volatile ("pause");
            *portsc = 0x10000;
            for (volatile u32 i = 0; i < 200000; i++) __asm__ volatile ("pause");

            u8 addr = 1;
            OhciSetAddress(ohci, 0, addr);
            for (volatile u32 i = 0; i < 200000; i++) __asm__ volatile ("pause");

            u8 desc[8];
            if (OhciGetDeviceDescriptor(ohci, addr, desc)) {
                if (desc[4] == 0x03) {
                    OhciSetConfiguration(ohci, addr, 1);
                    OhciSetProtocol(ohci, addr, 1);
                    OhciSetIdle(ohci, addr, 0);

                    ohci->KbdEd = (OhciEd*)AlignedAlloc(&KernelPool, sizeof(OhciEd), 16);
                    MemSet(ohci->KbdEd, 0, sizeof(OhciEd));

                    u32 speedBit = (status & 0x200) ? (1 << 13) : 0;
                    ohci->KbdEd->Control = (addr & 0x7F) | (1 << 7) | (8 << 16) | speedBit;

                    ohci->KbdBuffer = (u8*)AlignedAlloc(&KernelPool, 8, 16);
                    MemSet(ohci->KbdBuffer, 0, 8);

                    ohci->KbdTd = (OhciTd*)AlignedAlloc(&KernelPool, sizeof(OhciTd), 16);
                    MemSet(ohci->KbdTd, 0, sizeof(OhciTd));

                    ohci->KbdTd->Control = 0x20000000 | (0x25 << 8) | (1 << 24);
                    ohci->KbdTd->CurrentBuffer = (u32)(u64)ohci->KbdBuffer;
                    ohci->KbdTd->BufferEnd = (u32)(u64)(ohci->KbdBuffer + 7);
                    ohci->KbdTd->Next = 0;

                    ohci->KbdEd->Head = (u32)(u64)ohci->KbdTd;
                    ohci->KbdEd->Tail = 0;

                    for (int i = 0; i < 32; i++) {
                        ohci->Hcca->HccaInterruptTable[i] = (u32)(u64)ohci->KbdEd;
                    }

                    ohci->HaveKeyboard = 1;
                    ohci->KbdAddress = addr;

                    ohci->Regs->HcControl |= 0x04;
                    break;
                }
            }
        }
    }

    return ohci;
}

void OhciPollEvents(OhciController* ohci)
{
    if (!ohci->HaveKeyboard) return;

    if (ohci->Hcca->HccaDoneHead != 0) {
        u32 dh = ohci->Hcca->HccaDoneHead & ~1;
        ohci->Hcca->HccaDoneHead = 0;
        ohci->Regs->HcInterruptStatus = 2;

        if (dh == (u32)(u64)ohci->KbdTd) {
            u8 raw = ohci->KbdBuffer[2];
            if (raw && raw != 0xFF) {
                if (raw >= 4 && raw <= 29) {
                    static const char ascii[] = "abcdefghijklmnopqrstuvwxyz";
                    ohci->LastChar = ascii[raw - 4];
                } else if (raw == 40) {
                    ohci->LastChar = '\n';
                } else if (raw >= 30 && raw <= 38) {
                    static const char ascii[] = "123456789";
                    ohci->LastChar = ascii[raw - 30];
                } else if (raw == 39) {
                    ohci->LastChar = '0';
                } else if (raw == 44) {
                    ohci->LastChar = ' ';
                } else {
                    ohci->LastChar = 0;
                }
            } else {
                ohci->LastChar = 0;
            }

            ohci->KbdTd->Control = 0x20000000 | (0x25 << 8) | (1 << 24);
            ohci->KbdTd->CurrentBuffer = (u32)(u64)ohci->KbdBuffer;
            ohci->KbdTd->BufferEnd = (u32)(u64)(ohci->KbdBuffer + 7);
            ohci->KbdTd->Next = 0;
            ohci->KbdEd->Head = (u32)(u64)ohci->KbdTd;
        }
    }
}

u8 OhciGetChar(OhciController* ohci)
{
    if (!ohci->HaveKeyboard) return 0;
    u8 c = ohci->LastChar;
    ohci->LastChar = 0;
    return c;
}
