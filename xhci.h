#pragma once
#include "int.h"
#include "pci.h"
#include "debug.h"
#include "flash.h"
#include "str.h"

typedef struct {
    volatile u32 caplen_hciver;
    volatile u32 hcsparams1;
    volatile u32 hcsparams2;
    volatile u32 hcsparams3;
    volatile u32 hccparams1;
    volatile u32 dboff;
    volatile u32 rtsoff;
    volatile u32 hccparams2;
} __attribute__((aligned(64))) xhci_cap_regs;

typedef struct {
    volatile u32 usbcmd;
    volatile u32 usbsts;
    volatile u32 pagesize;
    volatile u32 reserved1[2];
    volatile u32 dnctrl;
    volatile u64 crcr;
    volatile u32 reserved2[4];
    volatile u64 dcbaap;
    volatile u32 config;
} __attribute__((aligned(64))) xhci_op_regs;

typedef struct {
    volatile u32 mfindex;
    volatile u32 reserved[7];
    volatile u32 iman;
    volatile u32 imod;
    volatile u32 erstsz;
    volatile u32 reserved2;
    volatile u64 erstba;
    volatile u64 erdp;
} __attribute__((aligned(64))) xhci_run_regs;

typedef struct {
    volatile u32 doorbell[256];
} __attribute__((aligned(64))) xhci_doorbell_regs;

typedef struct {
    u64 ring_segment_base;
    u32 ring_segment_size;
    u32 reserved;
} __attribute__((aligned(64))) erst_entry_t;
#define TRB_NORMAL          1
#define TRB_SETUP_STAGE     2
#define TRB_DATA_STAGE      3
#define TRB_STATUS_STAGE    4
#define TRB_LINK            6
#define TRB_EVENT_CMD_COMP  32
#define TRB_EVENT_TRANSFER  33
#define TRB_CMD_ENABLE_SLOT 9
#define TRB_CMD_ADDRESS_DEV 11
#define TRB_CMD_CONFIG_EP   12
#define TRB_CMD_EVAL_CTX    13
#define TRB_CMD_RESET_DEV   14
#define TRB_CMD_STOP_EP     15
#define TRB_CMD_NOOP        23

typedef struct {
    volatile u64 param;
    volatile u32 status;
    volatile u32 control;
} __attribute__((aligned(64))) trb_t;

typedef struct {
    trb_t trbs[256];
} __attribute__((aligned(64))) transfer_ring;

typedef struct {
    trb_t trbs[256];
} __attribute__((aligned(64))) event_ring;

typedef struct {
    xhci_cap_regs *cap;
    xhci_op_regs *op;
    xhci_run_regs *run;
    xhci_doorbell_regs *db;
    u64 mmio_base;

    erst_entry_t *erst;
    event_ring *event_ring;
    transfer_ring *cmd_ring;
    transfer_ring *transfer_rings[32];

    u8 max_slots;
    u8 enabled_slot;
} __attribute__((aligned(64))) xhci_controller;

static u8 xhc_cmd_cycle = 1;
static u32 xhc_cmd_idx = 0;
static xhci_controller xhc __attribute__((aligned(64)));
static u8 erst_buffer[4096] __attribute__((aligned(64)));static u8 event_ring_buffer[4096] __attribute__((aligned(64)));
static u8 cmd_ring_buffer[4096] __attribute__((aligned(64)));
static u64 dcbaa[256] __attribute__((aligned(64)));

static inline void xhci_init(PCIDevice *dev) {
    u64 mmio = dev->mmio_base;

    DebugStr("Initializing xHCI at 0x");
    DebugU64(mmio);
    DebugChar('\n');

    for (volatile int i = 0; i < 1000; i++) asm("pause");

    xhc.mmio_base = mmio;
    xhc.cap = (xhci_cap_regs*)mmio;

    u8 caplen = xhc.cap->caplen_hciver & 0xFF;
    DebugStr("CAPLEN: ");
    DebugU8(caplen);
    DebugChar('\n');

    xhc.op = (xhci_op_regs*)(mmio + caplen);
    xhc.run = (xhci_run_regs*)(mmio + xhc.cap->rtsoff);
    xhc.db = (xhci_doorbell_regs*)(mmio + xhc.cap->dboff);

    DebugStr("Doorbell base: 0x");
    DebugU64((u64)xhc.db);
    DebugChar('\n');

    u32 hcsparams1 = xhc.cap->hcsparams1;
    xhc.max_slots = hcsparams1 & 0xFF;
    DebugStr("Max slots: ");
    DebugU64(xhc.max_slots);
    DebugChar('\n');

    DebugStr("Resetting controller...\n");

    while (xhc.op->usbsts & (1 << 0)) asm("pause");

    xhc.op->usbcmd |= (1 << 1);
    while (xhc.op->usbcmd & (1 << 1)) asm("pause");
    DebugStr("Reset complete\n");

    xhc.op->config &= ~0xFF;
    xhc.op->config |= xhc.max_slots;
    DebugStr("Config space set\n");

    MemSet((u8*)dcbaa, 0, sizeof(dcbaa));
    xhc.op->dcbaap = (u64)dcbaa;
    DebugStr("DCBAP initialized\n");
    MemSet(erst_buffer, 0, sizeof(erst_buffer));
    MemSet(event_ring_buffer, 0, sizeof(event_ring_buffer));
    MemSet(cmd_ring_buffer, 0, sizeof(cmd_ring_buffer));

    xhc.erst = (erst_entry_t*)erst_buffer;
    xhc.event_ring = (event_ring*)event_ring_buffer;
    xhc.cmd_ring = (transfer_ring*)cmd_ring_buffer;

    xhc.erst[0].ring_segment_base = (u64)xhc.event_ring;
    xhc.erst[0].ring_segment_size = 256;
    xhc.erst[0].reserved = 0;

    xhc.run->erstsz = 1;
    xhc.run->erstba = (u64)xhc.erst;
    xhc.run->erdp = (u64)xhc.event_ring | (1<<3);

    xhc.op->crcr = (u64)xhc.cmd_ring | 1;
    CompilerBarrier();
    MemFlash();

    while ((xhc.op->crcr & 0xFFFFFFC0) != (u64)xhc.cmd_ring) asm("pause");

    MemFlash();

    u64 crcr_val = xhc.op->crcr;
    DebugStr("CRCR value: 0x");
    DebugU64(crcr_val);
    DebugChar('\n');

    xhc.op->usbcmd &= ~(1 << 2);
    xhc.op->usbcmd |= 1;
    DebugStr("RS bit set\n");

    int timeout = 1000000;
    while ((xhc.op->usbsts & 0x01) && timeout--) asm("pause");
    DebugStr("Controller started\n");

    xhc.enabled_slot = 0;
}

static inline void xhci_send_command(trb_t *cmd) {
    cmd->control = (cmd->control & ~1U) | xhc_cmd_cycle;

    xhc.cmd_ring->trbs[xhc_cmd_idx] = *cmd;
    CompilerBarrier();
    MemFlash();

    xhc.db->doorbell[0] = 0;
    CompilerBarrier();    MemFlash();

    xhc_cmd_idx++;
    if (xhc_cmd_idx == 255) {
        trb_t link_trb = {0};
        link_trb.param = (u64)xhc.cmd_ring;
        link_trb.control = (TRB_LINK << 10) | (1 << 1) | xhc_cmd_cycle;
        xhc.cmd_ring->trbs[255] = link_trb;
        CompilerBarrier();
        MemFlash();

        xhc_cmd_idx = 0;
        xhc_cmd_cycle ^= 1;
    }

    static u8 expected_event_cycle = 1;
    static u32 event_idx = 0;

    int timeout = 1000000;
    while (--timeout) {
        trb_t *event = &xhc.event_ring->trbs[event_idx];
        MemFullFlash();

        if ((event->control & 1) == expected_event_cycle) {
            u8 trb_type = (event->control >> 10) & 0x3F;
            u8 comp_code = (event->status >> 24) & 0xFF;
            u8 slot_id = (event->control >> 24) & 0xFF;

            event_idx++;
            if (event_idx >= 256) {
                event_idx = 0;
                expected_event_cycle ^= 1;
            }

            u64 next_erdp_phys = (u64)&xhc.event_ring->trbs[event_idx];
            xhc.run->erdp = next_erdp_phys | (1 << 3);
            MemFlash();

            if (trb_type == TRB_EVENT_CMD_COMP) {
                if (comp_code == 1) {
                    if (slot_id) {
                        xhc.enabled_slot = slot_id;
                        DebugStr("Command completed, slot: ");
                        DebugU8(xhc.enabled_slot);
                        DebugChar('\n');
                    }
                    return;
                } else {
                    DebugStr("Command failed with Completion Code: ");
                    DebugU8(comp_code);                    DebugChar('\n');
                    return;
                }
            }
            continue;
        }
        asm("pause");
    }
    DebugStr("Event timeout (No event received)\n");
}

static inline void xhci_enable_slot(void) {
    trb_t cmd = {0};
    cmd.control = (TRB_CMD_ENABLE_SLOT << 10);
    xhci_send_command(&cmd);
}

static inline void xhci_full_test(PCIDevice *dev) {
    xhci_init(dev);

    DebugStr("\n=== xHCI Test ===\n");

    xhci_enable_slot();
    if (!xhc.enabled_slot) {
        DebugStr("No slot enabled\n");
        return;
    }

    DebugStr("xHCI ready, slot ");
    DebugU8(xhc.enabled_slot);
    DebugStr(" enabled\n");

    while (1) {
        for (volatile int i = 0; i < 1000000; i++) asm("pause");
    }
}
