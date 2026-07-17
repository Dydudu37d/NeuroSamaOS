#pragma once

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC
#define PCI_BRIDGE_PRIMARY_BUS    0x18
#define PCI_BRIDGE_SECONDARY_BUS  0x19
#define PCI_BRIDGE_SUBORDINATE_BUS 0x1A

#include "port.h"
#include "int.h"
#include "debug.h"

static inline u32 PCIAddr(u8 Bus, u8 Slot, u8 Func, u8 Offset)
{
    return ((1 << 31) | (Bus << 16) | ((Slot & 0x1F) << 11) | ((Func & 0x07) << 8) | (Offset & 0xFC));
}

static inline u32 PCIReadDWORD(u8 Bus, u8 Slot, u8 Func, u8 Offset)
{
    outl(PCI_CONFIG_ADDRESS, PCIAddr(Bus, Slot, Func, Offset));
    return inl(PCI_CONFIG_DATA);
}

static inline void PCIWriteDWORD(u8 Bus, u8 Slot, u8 Func, u8 Offset, u32 Value)
{
    outl(PCI_CONFIG_ADDRESS, PCIAddr(Bus, Slot, Func, Offset));
    outl(PCI_CONFIG_DATA, Value);
}

static inline void PCIWriteWORD(u8 Bus, u8 Slot, u8 Func, u8 Offset, u16 Value)
{
    outl(PCI_CONFIG_ADDRESS, PCIAddr(Bus, Slot, Func, Offset));
    outw(PCI_CONFIG_DATA, Value);
}

static inline u16 PCIReadWORD(u8 Bus, u8 Slot, u8 Func, u8 Offset)
{
    outl(PCI_CONFIG_ADDRESS, PCIAddr(Bus, Slot, Func, Offset));
    u32 dword = inl(PCI_CONFIG_DATA);
    if (Offset & 0x2) {
        return (dword >> 16) & 0xFFFF;
    } else {
        return dword & 0xFFFF;
    }
}

static inline u8 PCIReadBYTE(u8 Bus, u8 Slot, u8 Func, u8 Offset)
{
    outl(PCI_CONFIG_ADDRESS, PCIAddr(Bus, Slot, Func, Offset));
    u32 dword = inl(PCI_CONFIG_DATA);
    u8 shift = (Offset & 0x3) * 8;
    return (dword >> shift) & 0xFF;
}

static inline void PCIWriteBYTE(u8 Bus, u8 Slot, u8 Func, u8 Offset, u8 Value)
{
    outl(PCI_CONFIG_ADDRESS, PCIAddr(Bus, Slot, Func, Offset));
    outb(PCI_CONFIG_DATA, Value);
}

static inline void PCIEnableDevice(u8 Bus, u8 Slot, u8 Func)
{
    u16 Command = PCIReadWORD(Bus, Slot, Func, 0x04);
    Command |= (1 << 0) | (1 << 1) | (1 << 2);
    PCIWriteWORD(Bus, Slot, Func, 0x04, Command);
}

static inline u64 PCIGetBARAddress(u8 Bus, u8 Slot, u8 Func, u8 BarIndex) {
    u32 barLow = PCIReadDWORD(Bus, Slot, Func, 0x10 + BarIndex * 4);
    
    if (barLow & 0x1) {
        return (u64)(barLow & ~0x3);
    }
    
    if ((barLow & 0x6) == 0x4) {
        u32 barHigh = PCIReadDWORD(Bus, Slot, Func, 0x10 + (BarIndex + 1) * 4);
        return ((u64)barHigh << 32) | ((u64)barLow & ~0xFULL);
    }
    
    return (u64)barLow & ~0xFULL;
}

static inline u64 PCIGetBARSize64(u8 Bus, u8 Slot, u8 Func, u8 BARIndex)
{
    u32 OriginalBAR_Low = PCIReadDWORD(Bus, Slot, Func, 0x10 + BARIndex * 4);
    u32 OriginalBAR_High = PCIReadDWORD(Bus, Slot, Func, 0x10 + BARIndex * 4 + 4);
    
    PCIWriteDWORD(Bus, Slot, Func, 0x10 + BARIndex * 4, 0xFFFFFFFF);
    PCIWriteDWORD(Bus, Slot, Func, 0x10 + BARIndex * 4 + 4, 0xFFFFFFFF);
    
    u32 SizeMask_Low = PCIReadDWORD(Bus, Slot, Func, 0x10 + BARIndex * 4);
    u32 SizeMask_High = PCIReadDWORD(Bus, Slot, Func, 0x10 + BARIndex * 4 + 4);
    
    PCIWriteDWORD(Bus, Slot, Func, 0x10 + BARIndex * 4, OriginalBAR_Low);
    PCIWriteDWORD(Bus, Slot, Func, 0x10 + BARIndex * 4 + 4, OriginalBAR_High);
    
    u64 SizeMask = ((u64)SizeMask_High << 32) | (SizeMask_Low & 0xFFFFFFF0);
    u64 Size = (~SizeMask + 1) & 0xFFFFFFF0;
    
    return Size;
}

static inline void PCIScanBus(void)
{
    for (u16 Bus = 0; Bus < 256; Bus++)
    {
        for (u8 Dev = 0; Dev < 32; Dev++)
        {
            for (u8 Func = 0; Func < 8; Func++)
            {
                u32 VendorDevice = PCIReadDWORD(Bus, Dev, Func, 0x00);
                u16 Vendor = VendorDevice & 0xFFFF;
                
                if (Vendor == 0xFFFF)
                {
                    if (Func == 0)
                        break;
                    continue;
                }
                
                u32 ClassRev = PCIReadDWORD(Bus, Dev, Func, 0x08);
                u8 ClassCode = (ClassRev >> 24) & 0xFF;
                u8 SubClass = (ClassRev >> 16) & 0xFF;
                
                u32 Header = PCIReadDWORD(Bus, Dev, Func, 0x0C);
                u8 HeaderType = (Header >> 16) & 0xFF;
                
                if (!(HeaderType & 0x80))
                    break;
            }
        }
    }
}

static inline void PCIFindDeviceByClass(u8 TargetClass, u8 TargetSubClass, u8 TargetProgIF, u8 *OutBus, u8 *OutDev, u8 *OutFunc)
{
    *OutBus = 0xFF;
    *OutDev = 0xFF;
    *OutFunc = 0xFF;
    
    for (u16 Bus = 0; Bus < 256; Bus++)
    {
        for (u16 Dev = 0; Dev < 32; Dev++)
        {
            u32 HeaderTypeReg = PCIReadDWORD(Bus, Dev, 0, 0x0C);
            u8 HeaderType = (HeaderTypeReg >> 16) & 0xFF;
            u16 MaxFunc = (HeaderType & 0x80) ? 8 : 1;

            for (u16 Func = 0; Func < MaxFunc; Func++)
            {
                u32 VendorDevice = PCIReadDWORD(Bus, Dev, Func, 0x00);
                u16 Vendor = VendorDevice & 0xFFFF;
                u16 Device = (VendorDevice >> 16) & 0xFFFF;
                
                if (Vendor == 0xFFFF) {
                    continue;
                }
                
                u32 ClassRev = PCIReadDWORD(Bus, Dev, Func, 0x08);
                u8 ClassCode = (ClassRev >> 24) & 0xFF;
                u8 SubClass = (ClassRev >> 16) & 0xFF;
                u8 ProgIF = (ClassRev >> 8) & 0xFF;

                if (ClassCode == TargetClass && SubClass == TargetSubClass) {
                    DebugStr("Found controller: Bus=");
                    DebugU8(Bus);
                    DebugStr(" Dev=");
                    DebugU8(Dev);
                    DebugStr(" Func=");
                    DebugU8(Func);
                    DebugStr(" Vendor=0x");
                    DebugU16(Vendor);
                    DebugStr(" Device=0x");
                    DebugU16(Device);
                    DebugStr(" ProgIF=0x");
                    DebugU8(ProgIF);
                    DebugChar('\n');
                    
                    if (ProgIF == TargetProgIF) {
                        PCIEnableDevice(Bus, Dev, Func);
                        *OutBus = Bus;
                        *OutDev = Dev;
                        *OutFunc = Func;
                        return;
                    }
                }
            }
        }
    }
}

static inline void PCIFindDeviceByClassList(u8 TargetClass[], u8 TargetSubClass[], u8 TargetProgIF[], u64 ListSize, u8 *OutBus, u8 *OutDev, u8 *OutFunc)
{
    *OutBus = 0xFF;
    *OutDev = 0xFF;
    *OutFunc = 0xFF;
    
    for (u16 Bus = 0; Bus < 256; Bus++)
    {
        for (u16 Dev = 0; Dev < 32; Dev++)
        {
            u32 HeaderTypeReg = PCIReadDWORD(Bus, Dev, 0, 0x0C);
            u8 HeaderType = (HeaderTypeReg >> 16) & 0xFF;
            u16 MaxFunc = (HeaderType & 0x80) ? 8 : 1;

            for (u16 Func = 0; Func < MaxFunc; Func++)
            {
                u32 VendorDevice = PCIReadDWORD(Bus, Dev, Func, 0x00);
                u16 Vendor = VendorDevice & 0xFFFF;
                u16 Device = (VendorDevice >> 16) & 0xFFFF;
                
                if (Vendor == 0xFFFF) {
                    continue;
                }
                
                u32 ClassRev = PCIReadDWORD(Bus, Dev, Func, 0x08);
                u8 ClassCode = (ClassRev >> 24) & 0xFF;
                u8 SubClass = (ClassRev >> 16) & 0xFF;
                u8 ProgIF = (ClassRev >> 8) & 0xFF;

                for (u64 Idx=0;Idx<ListSize;Idx++){

                    if (ClassCode == TargetClass[Idx] && SubClass == TargetSubClass[Idx]) {
                        DebugStr("Found controller: Bus=");
                        DebugU8(Bus);
                        DebugStr(" Dev=");
                        DebugU8(Dev);
                        DebugStr(" Func=");
                        DebugU8(Func);
                        DebugStr(" Vendor=0x");
                        DebugU16(Vendor);
                        DebugStr(" Device=0x");
                        DebugU16(Device);
                        DebugStr(" ProgIF=0x");
                        DebugU8(ProgIF);
                        DebugStr(" TargetClass=0x");
                        DebugU8(TargetClass[Idx]);
                        DebugStr(" TargetSubClass=0x");
                        DebugU8(TargetSubClass[Idx]);
                        DebugStr(" TargetProgIF=0x");
                        DebugU8(TargetProgIF[Idx]);
                        DebugChar('\n');
                        
                        if (ProgIF == TargetProgIF[Idx]) {
                            DebugStr("ProgIF==TargetProgIF.Enable.\n");
                            DebugStr("TargetProgIF=0x");
                            DebugU8(TargetProgIF[Idx]);
                            DebugChar('\n');
                            PCIEnableDevice(Bus, Dev, Func);
                            *OutBus = Bus;
                            *OutDev = Dev;
                            *OutFunc = Func;
                            return;
                        }
                    }
                }
            }
        }
    }
}


static inline void PCIScanBusRecursive(u8 Bus)
{
    for (u8 Dev = 0; Dev < 32; Dev++)
    {
        for (u8 Func = 0; Func < 8; Func++)
        {
            u32 VendorDevice = PCIReadDWORD(Bus, Dev, Func, 0x00);
            if ((VendorDevice & 0xFFFF) == 0xFFFF)
            {
                if (Func == 0) break;
                continue;
            }
            
            u32 ClassRev = PCIReadDWORD(Bus, Dev, Func, 0x08);
            u8 ClassCode = (ClassRev >> 24) & 0xFF;
            u8 SubClass = (ClassRev >> 16) & 0xFF;
            
            if (ClassCode == 0x06 && SubClass == 0x04)
            {
                u32 SecondaryBus = PCIReadDWORD(Bus, Dev, Func, 0x18);
                u8 SecondaryBusNum = (SecondaryBus >> 8) & 0xFF;
                PCIScanBusRecursive(SecondaryBusNum);
            }
        }
    }
}

static inline u8 PCIReadBusNumber(u8 Bus, u8 Slot, u8 Func) {
    u32 busReg = PCIReadDWORD(Bus, Slot, Func, 0x18);
    return (busReg >> 8) & 0xFF;
}

static inline _Bool PCIIsBridge(u8 Bus, u8 Slot, u8 Func) {
    u32 ClassRev = PCIReadDWORD(Bus, Slot, Func, 0x08);
    u8 ClassCode = (ClassRev >> 24) & 0xFF;
    u8 SubClass = (ClassRev >> 16) & 0xFF;
    return (ClassCode == 0x06 && SubClass == 0x04);
}

static inline u8 PCIGetPrimaryBus(u8 Bus, u8 Slot, u8 Func) {
    return PCIReadBYTE(Bus, Slot, Func, PCI_BRIDGE_PRIMARY_BUS);
}

static inline u8 PCIGetSecondaryBus(u8 Bus, u8 Slot, u8 Func) {
    return PCIReadBYTE(Bus, Slot, Func, PCI_BRIDGE_SECONDARY_BUS);
}

static inline u8 PCIGetSubordinateBus(u8 Bus, u8 Slot, u8 Func) {
    return PCIReadBYTE(Bus, Slot, Func, PCI_BRIDGE_SUBORDINATE_BUS);
}
