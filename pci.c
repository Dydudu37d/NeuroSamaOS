#include "pci.h"
#include "debug.h"

PCIDevice pci_find_xhci(void) {
    PCIDevice dev = {0};
    
    DebugStr("Scanning PCI(e) bus for xHCI...\n");
    
    for (u16 bus = 0; bus < 256; bus++) {
        for (u8 slot = 0; slot < 32; slot++) {
            u16 vendor = pci_read_word(bus, slot, 0, 0x00);
            if (vendor == 0xFFFF) continue;
            
            u16 class = pci_read_word(bus, slot, 0, 0x0A);
            u8 prog_if = pci_read_byte(bus, slot, 0, 0x09);
            
            DebugStr("Found device: ");
            DebugU64(bus); DebugStr(":");
            DebugU64(slot); DebugStr(".0 Vendor=0x");
            DebugU64(vendor); DebugStr(" Class=0x");
            DebugU64(class); DebugStr(" ProgIF=0x");
            DebugU64(prog_if); DebugChar('\n');
            
            if (class == 0x0C03 && prog_if == 0x30) {
                DebugStr("Found xHCI controller!\n");
                
                dev.bus = bus;
                dev.slot = slot;
                dev.func = 0;
                dev.vendor_id = vendor;
                dev.device_id = pci_read_word(bus, slot, 0, 0x02);
                
                u8 pcie_cap = pci_find_capability(bus, slot, 0, PCI_CAP_ID_PCIE);
                if (pcie_cap) {
                    DebugStr("PCIe capability found at offset 0x");
                    DebugU8(pcie_cap);
                    
                    u16 pcie_flags = pci_read_word(bus, slot, 0, pcie_cap + 2);
                    DebugStr("\nPCIe device type: ");
                    switch ((pcie_flags >> 4) & 0xF) {
                        case 0: DebugStr("Endpoint"); break;
                        case 4: DebugStr("Root Port"); break;
                        case 5: DebugStr("Upstream Port"); break;
                        case 6: DebugStr("Downstream Port"); break;
                        case 7: DebugStr("PCI-to-PCIe Bridge"); break;
                        case 8: DebugStr("PCIe-to-PCI Bridge"); break;
                        default: DebugStr("Reserved");
                    }
                    DebugChar('\n');
                }
                
                dev.mmio_base = pci_read_bar0(bus, slot, 0);
                u16 cmd = pci_read_word(bus, slot, 0, 0x04);
                DebugStr("Original command register: 0x");
                DebugU64(cmd); DebugChar('\n');
                
                cmd |= (1 << 2) | (1 << 1);
                pci_write_dword(bus, slot, 0, 0x04, cmd);
                
                DebugStr("Final MMIO base: 0x");
                DebugU64(dev.mmio_base); DebugChar('\n');
                
                return dev;
            }
        }
    }
    
    DebugStr("No xHCI controller found!\n");
    return dev;
}
