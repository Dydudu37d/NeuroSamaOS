#pragma once
#include "int.h"
#include "port.h"
#include "debug.h"

#define PCI_CONFIG_ADDR 0xCF8
#define PCI_CONFIG_DATA 0xCFC
#define PCI_CAP_PTR          0x34
#define PCI_CAP_ID_PCIE      0x10
#define PCI_CAP_ID_MSI       0x05
#define PCI_CAP_ID_MSIX      0x11
#define PCIE_EXT_CAP_START   0x100

static inline u32 pci_make_addr(u8 bus, u8 slot, u8 func, u8 offset) {
    return (1 << 31)
         | ((u32)bus << 16)
         | ((u32)slot << 11)
         | ((u32)func << 8)
         | (offset & 0xFC);
}

static inline u32 pci_read_dword(u8 bus, u8 slot, u8 func, u8 offset) {
    outl(PCI_CONFIG_ADDR, pci_make_addr(bus, slot, func, offset));
    return inl(PCI_CONFIG_DATA);
}

static inline u16 pci_read_word(u8 bus, u8 slot, u8 func, u8 offset) {
    outl(PCI_CONFIG_ADDR, pci_make_addr(bus, slot, func, offset));
    return inw(PCI_CONFIG_DATA + (offset & 2));
}

static inline u8 pci_read_byte(u8 bus, u8 slot, u8 func, u8 offset) {
    outl(PCI_CONFIG_ADDR, pci_make_addr(bus, slot, func, offset));
    return inb(PCI_CONFIG_DATA + (offset & 3));
}

static inline void pci_write_dword(u8 bus, u8 slot, u8 func, u8 offset, u32 val) {
    outl(PCI_CONFIG_ADDR, pci_make_addr(bus, slot, func, offset));
    outl(PCI_CONFIG_DATA, val);
}

static inline u8 pci_find_capability(u8 bus, u8 slot, u8 func, u8 cap_id) {
    u8 cap_ptr = pci_read_byte(bus, slot, func, PCI_CAP_PTR);
    while (cap_ptr >= 0x40) {
        u8 this_id = pci_read_byte(bus, slot, func, cap_ptr);
        if (this_id == cap_id) return cap_ptr;
        cap_ptr = pci_read_byte(bus, slot, func, cap_ptr + 1);
        if (cap_ptr == 0xFF) break;
    }
    return 0;
}

static inline u16 pcie_find_ext_cap(u8 bus, u8 slot, u8 func, u16 cap_id) {
    u16 offset = PCIE_EXT_CAP_START;
    for (int i = 0; i < 480; i++) {
        u32 hdr = pci_read_dword(bus, slot, func, offset);
        if ((hdr & 0xFFFF) == cap_id) return offset;
        offset = (hdr >> 20) & 0xFFC;
        if (offset < PCIE_EXT_CAP_START) break;
    }
    return 0;
}

static inline u64 pci_read_bar0(u8 bus, u8 slot, u8 func) {
    u32 bar0_low = pci_read_dword(bus, slot, func, 0x10);
    
    if ((bar0_low & 0x7) == 0x4) {
        u32 bar0_high = pci_read_dword(bus, slot, func, 0x14);
        
        u32 orig_low = bar0_low;
        u32 orig_high = bar0_high;
        
        pci_write_dword(bus, slot, func, 0x10, 0xFFFFFFFF);
        pci_write_dword(bus, slot, func, 0x14, 0xFFFFFFFF);
        
        u32 mask_low = pci_read_dword(bus, slot, func, 0x10);
        u32 mask_high = pci_read_dword(bus, slot, func, 0x14);
        
        pci_write_dword(bus, slot, func, 0x10, orig_low);
        pci_write_dword(bus, slot, func, 0x14, orig_high);
        
        u64 addr = ((u64)orig_high << 32) | (orig_low & ~0xFULL);
        u64 size = (~(((u64)mask_high << 32) | (mask_low & ~0xFULL))) + 1;
        
        DebugStr("BAR0: 64-bit MMIO\n");
        DebugStr("  Addr: 0x"); DebugU64(addr); DebugChar('\n');
        DebugStr("  Size: 0x"); DebugU64(size); DebugChar('\n');
        
        return addr;
    }
    
    u32 orig_low = bar0_low;
    pci_write_dword(bus, slot, func, 0x10, 0xFFFFFFFF);
    u32 mask_low = pci_read_dword(bus, slot, func, 0x10);
    pci_write_dword(bus, slot, func, 0x10, orig_low);
    
    u64 addr = orig_low & ~0xF;
    u64 size = (~(mask_low & ~0xF)) + 1;
    
    DebugStr("BAR0: 32-bit MMIO\n");
    DebugStr("  Addr: 0x"); DebugU64(addr); DebugChar('\n');
    DebugStr("  Size: 0x"); DebugU64(size); DebugChar('\n');
    
    return addr;
}

typedef struct {
    u8 bus, slot, func;
    u64 mmio_base;
    u16 vendor_id, device_id;
} PCIDevice;

PCIDevice pci_find_xhci(void);
