#pragma once

#include "int.h"

typedef enum {
    CORB_2Entry=0b00,
    CORB_16Entry=0b01,
    CORB_256Entry=0b10,
    CORB_NoWayBro_Its_RESERVED_And_ERROR=0b11
} __attribute__((packed)) CORB_SIZE_TYPE;

typedef enum {
    RATE_48hz=0b0,
    RATE_44Point1hz=0b1,
} __attribute__((packed)) FMT_BASE_RATE;

typedef struct {
    union {
        volatile u16 U;
        struct {
            volatile u16 roOSSCount     : 4;
            volatile u16 roISSCount     : 4;
            volatile u16 roBSSCount     : 5;
            volatile u16 roSDOLineCount : 2;
            volatile u16 roHave64bit    : 1;
        };
    };
} __attribute__((packed)) HDAudioRegGCAP;

typedef struct {
    union {
        volatile u8 U;
        struct {
            volatile u8 roSubVersion : 8;
        };
    };
} __attribute__((packed)) HDAudioRegVMIN;

typedef struct {
    union {
        volatile u8 U;
        struct {
            volatile u8 roMainVersion : 8;
        };
    };
} __attribute__((packed)) HDAudioRegVMAJ;

typedef struct {
    union {
        volatile u16 U;
        struct {
            volatile u16 roOutPayLoadCount : 16;
        };
    };
} __attribute__((packed)) HDAudioRegOUTPAY;

typedef struct {
    union {
        volatile u16 U;
        struct {
            volatile u16 roInPayLoadCount : 16;
        };
    };
} __attribute__((packed)) HDAudioRegINPAY;

typedef struct {
    union {
        volatile u32 U;
        struct {
            volatile u32 rwsControllerReset : 1;
            volatile u32 rwFlushControl : 1;
            volatile u32 rsvdpRsvd1 : 6;
            volatile u32 rwUnsolicitedResponse : 1;
            volatile u32 rsvdpRsvd2 : 23;
        };
    };
} __attribute__((packed)) HDAudioRegGCTL;

typedef struct {
    union {
        volatile u16 U;
        struct {
            volatile u16 rwsSerialDataIn : 15;
            volatile u16 rsvdpRsvd1      : 1;
        };
    };
} __attribute__((packed)) HDAudioRegWAKEEN;

typedef struct {
    union {
        volatile u16 U;
        struct {
            volatile u16 rw1csSerialDataIn : 15;
            volatile u16 rsvdzRsvd1        : 1;
        };
    };
} __attribute__((packed)) HDAudioRegSTATESTS;

typedef struct {
    union {
        volatile u16 U;
        struct {
            volatile u16 rsvdzRsvd1    : 14;
            volatile u16 rw1cFSTS      : 1;
            volatile u16 rsvdzRsvd2    : 1;
        };
    };
} __attribute__((packed)) HDAudioRegGSTS;

typedef struct {
    volatile u16 roOutStreamPayLoadCapacity;
} __attribute__((packed)) HDAudioRegOUTSTRMPAY;

typedef struct {
    volatile u16 roInStreamPayLoadCapacity;
} __attribute__((packed)) HDAudioRegINSTRMPAY;

typedef struct {
    union {
        volatile u32 U;
        struct {
            volatile u32 rwGlobalInterruptEnable     : 1;
            volatile u32 rwControllerInterruptEnable : 1;
            volatile u32 rwStreamInterruptEnable     : 30;
        };
    };
} __attribute__((packed)) HDAudioRegINTCTL;

typedef struct {
    union {
        volatile u32 U;
        struct {
            volatile u32 roGlobalInterruptStatus     : 1;
            volatile u32 roControllerInterruptStatus : 1;
            volatile u32 roStreamInterruptionStatus  : 30;
        };
    };
} __attribute__((packed)) HDAudioRegINTSTS;

typedef struct {
    volatile u32 Counter;
} __attribute__((packed)) HDAudioRegWALCLK;

typedef struct {
    union {
        volatile u32 U;
        struct {
            volatile u32 rsvdpRsvd1              : 2;
            volatile u32 rwStreamSynchronization : 30;
        };
    };
} __attribute__((packed)) HDAudioRegSSYNC;

typedef struct {
    union {
        volatile u64 U;
        struct {
            volatile u64 roLowerBaseAddressUnimplemented : 7;
            volatile u64 rwLowBase                       : 25;
            volatile u64 rwHighBase                      : 32;
        };
    };
} __attribute__((packed)) HDAudioRegCORBBase;

typedef struct {
    union {
        volatile u16 U;
        struct {
            volatile u16 rwWritePtr : 8;
            volatile u16 rsvdpRsvd1 : 8;
        };
    };
} __attribute__((packed)) HDAudioRegCORBWritePtr;

typedef struct {
    union {
        volatile u16 U;
        struct {
            volatile u16 roReadPtr  : 8;
            volatile u16 rsvdpRsvd1 : 7;
            volatile u16 rwReset    : 1;
        };
    };
} __attribute__((packed)) HDAudioRegCORBReadPtr;

typedef struct {
    union {
        volatile volatile u8 U;
        struct {
            volatile volatile u8 rwMemoryErrorInterruptEnable : 1;
            volatile volatile u8 rwEnableDMAEngine            : 1;
            volatile volatile u8 rsvdpRsvd1                   : 6;
        };
    };
} __attribute__((packed)) HDAudioRegCORBCTL;

typedef struct {
    union {
        volatile u8 U;
        struct {
            volatile u8 rsvdzRsvd1                     : 7;
            volatile u8 rw1cMemoryErrorInterruptStatus : 1;
        };
    };
} __attribute__((packed)) HDAudioRegCORBSTS;

typedef struct {
    union {
        volatile u8 U;
        struct {
            volatile u8 roSizeCapability : 4;
            volatile u8 rsvdpRsvd1       : 2;
            volatile CORB_SIZE_TYPE roSize : 2;
        };
    };
} __attribute__((packed)) HDAudioRegCORBSize;

typedef struct {
    union {
        volatile u64 U;
        struct {
            volatile u64 roLowerBaseAddressUnimplemented : 7;
            volatile u64 rwLowBase                       : 25;
            volatile u64 rwHighBase                      : 32;
        };
    };
} __attribute__((packed)) HDAudioRegRIRBBase;

typedef struct {
    union {
        volatile u16 U;
        struct {
            volatile u16 roWritePtr      : 8;
            volatile u16 rsvdpRsvd1      : 7;
            volatile u16 rwWritePtrReset : 1;
        };
    };
} __attribute__((packed)) HDAudioRegRIRBWritePtr;

typedef struct {
    union {
        volatile u16 U;
        struct {
            volatile u16 rwResponseInterruptCount : 8;
            volatile u16 rsvdpRsvd1               : 8;
        };
    };
} __attribute__((packed)) HDAudioRegRINTCNT;

typedef struct {
    union {
        volatile u8 U;
        struct {
            volatile u8 rwResponseInterruptControl        : 1;
            volatile u8 rwDMAEnable                       : 1;
            volatile u8 rwResponseOverrunInterruptControl : 1;
            volatile u8 rsvdpRsvd1                        : 5;
        };
    };
} __attribute__((packed)) HDAudioRegRIRBCTL;

typedef struct {
    union {
        volatile u8 U;
        struct {
            volatile u8 rsvdzRsvd1                         : 5;
            volatile u8 rw1cResponseOverrunInterruptStatus : 1;
            volatile u8 rsvdzRsvd2                         : 1;
            volatile u8 rw1cResponseInterruptStatus        : 1;
        };
    };
} __attribute__((packed)) HDAudioRegRIRBSTS;

typedef struct {
    union {
        volatile u8 U;
        struct {
            volatile u8 roSizeCapability   : 4;
            volatile u8 rsvdpRsvd1         : 2;
            volatile CORB_SIZE_TYPE roSize : 2;
        };
    };
} __attribute__((packed)) HDAudioRegRIRBSize;

typedef struct {
    union {
        volatile u64 U;
        struct {
            volatile u64 rsvdzRsvd1             : 7;
            volatile u64 rwLowBase              : 24;
            volatile u64 rwPositionBufferEnable : 1;
            volatile u64 rwHighBase             : 32;
        };
    };
} __attribute__((packed)) HDAudioRegDPBase;

typedef struct{
    union {
        volatile u8 U[3];
        struct {
            volatile u8 rwStreamReset : 1;
            volatile u8 rwStreamRun : 1;
            volatile u8 rwInterruptOnComplete : 1;
            volatile u8 rwFIFOErrorIntEnable : 1;
            volatile u8 rwDescriptorErrorIntEnable : 1;
            volatile u8 rsvdpRsvd1 : 3;
            
            volatile u8 rwStreamNumber : 4;
            volatile u8 rwTrafficPriority : 1;
            volatile u8 rwBidirectionalDir : 1;
            volatile u8 rsvdpRsvd2 : 2;
            
            volatile u8 rsvdpRsvd3 : 8;
        };
    };
} __attribute__((packed)) HDAudioRegSDOCTL;

typedef struct {
    union {
        volatile u8 U;
        struct {
            volatile u8 rsvdzRsvd1          : 2;
            volatile u8 roFIFOReady         : 1;
            volatile u8 rw1cDescriptorError : 1;
            volatile u8 rw1cFIFOError       : 4;
        };
    };
} __attribute__((packed)) HDAudioRegSDOSTS;

typedef struct {
    volatile u32 roLinkPositionInBuffer : 32;
} __attribute__((packed)) HDAudioRegSDOLPIB;

typedef struct {
    volatile u32 rwCyclicBufferLength : 32;
} __attribute__((packed)) HDAudioRegSDOCBL;

typedef struct {
    union {
        volatile u16 U;
        struct {
            volatile u16 rwLastValidIndex : 8;
            volatile u16 rsvdpRsvd1 : 8;
        };
    };
} __attribute__((packed)) HDAudioRegSDOLVI;

typedef struct {
    volatile u16 FIFOSize;
} __attribute__((packed)) HDAudioRegSDOFIFOS;

typedef struct {
    union {
        volatile u16 U;
        struct {
            volatile u16 roRsvd1          : 1;
            union {
                volatile u16 BaseRateU            : 1;
                FMT_BASE_RATE rwBaseRate : 1;
            };
            volatile u16 rwRateMultiplier : 3;
            volatile u16 rwRateDivisor    : 3;
            volatile u16 rsvdpRsvd2       : 1;
            volatile u16 rwBitDepth       : 3;
            volatile u16 rwChannelCount   : 4;
        };
    };
} __attribute__((packed)) HDAudioRegSDOFMT;

typedef struct {
    union {
        volatile u64 U;
        struct {
            volatile u64 rsvdzRsvd1 : 7;
            volatile u64 rwLowBase : 25;
            volatile u64 rwHighBase : 32;
        };
    };
} __attribute__((packed)) HDAudioRegSDODPBase;

typedef struct {
    volatile HDAudioRegSDOCTL CTL;
    volatile HDAudioRegSDOSTS STS;
    volatile HDAudioRegSDOLPIB LPIB;
    volatile HDAudioRegSDOCBL CBL;
    volatile HDAudioRegSDOLVI LVI;
    volatile HDAudioRegSDOFIFOS FIFOS;
    volatile HDAudioRegSDOFMT FMT;
    volatile HDAudioRegSDODPBase DPBase;
    volatile u8 Pad[4];
} __attribute__((packed)) HDAudioRegsStreamChannel;

typedef struct {
    volatile u32 roCounter;
} __attribute__((packed)) HDAudioRegWALCLKA;

typedef struct {
    volatile u32 roLinkPositioninBufferAlias;
} __attribute__((packed)) HDAudioRegSDOLICBA;

typedef struct {
    volatile u32 rwImmediateCommandWrite;
} __attribute__((packed)) HDAudioRegICOI;

typedef struct {
    volatile u32 roImmediateResponseRead;
} __attribute__((packed)) HDAudioRegICII;

typedef struct {
    union {
        volatile u16 U;
        struct {
            volatile u16 rwImmediateCommandBusy               : 1;
            volatile u16 rw1cImmediateResultValid             : 1;
            volatile u16 roImmediateCommandVersion            : 1;
            volatile u16 roImmediateResponseResultUnsolicited : 1;
            volatile u16 roImmediateResponseResultAddress     : 4;
            volatile u16 rsvdzRsvd1                           : 8;
        };
    };
} __attribute__((packed)) HDAudioRegICIS;

typedef struct {
    volatile HDAudioRegGCAP GCAP;
    volatile HDAudioRegVMIN VMIN;
    volatile HDAudioRegVMAJ VMAJ;
    volatile HDAudioRegOUTPAY OUTPAY;
    volatile HDAudioRegINPAY INPAY;
    volatile HDAudioRegGCTL GCTL;
    volatile HDAudioRegWAKEEN WAKEEN;
    volatile HDAudioRegSTATESTS STATESTS;
    volatile HDAudioRegGSTS GSTS;
    volatile u8 Rsvd0[6];
    volatile HDAudioRegOUTSTRMPAY OUTSTRMPAY;
    volatile HDAudioRegINSTRMPAY INSTRMPAY;
    volatile u8 Rsvd1[4];
    volatile HDAudioRegINTCTL INTCTL;
    volatile HDAudioRegINTSTS INTSTS;
    volatile u8 Rsvd2[8];
    volatile HDAudioRegWALCLK WALCLK;
    volatile u8 Rsvd3[4];
    volatile HDAudioRegSSYNC SSYNC;
    volatile u8 Rsvd4[4];
    volatile HDAudioRegCORBBase CORBBase;
    volatile HDAudioRegCORBWritePtr CORBWritePtr;
    volatile HDAudioRegCORBReadPtr CORBReadPtr;
    volatile HDAudioRegCORBCTL CORBCTL;
    volatile HDAudioRegCORBSTS CORBSTS;
    volatile HDAudioRegCORBSize CORBSize;
    volatile u8 Rsvd5;
    volatile HDAudioRegRIRBBase RIRBBase;
    volatile HDAudioRegRIRBWritePtr RIRBWritePtr;
    volatile HDAudioRegRINTCNT RINTCNT;
    volatile HDAudioRegRIRBCTL RIRBCTL;
    volatile HDAudioRegRIRBSTS RIRBSTS;
    volatile HDAudioRegRIRBSize RIRBSize;
    volatile u8 Rsvd6;
    volatile HDAudioRegICOI ICOI;
    volatile HDAudioRegICII ICII;
    volatile HDAudioRegICIS ICIS;
    volatile u8 Rsvd7[6];
    volatile HDAudioRegDPBase DPBase;
    volatile u8 Rsvd8[8];
    volatile HDAudioRegsStreamChannel Channels[30];
    volatile u8 Rsvd9[7092];
    volatile HDAudioRegWALCLKA WALCLKA;
    volatile u8 RsvdA[80];
    volatile HDAudioRegSDOLICBA LPIBA[30];
} __attribute__((packed)) HDAudioRegs;

typedef struct {
    union {
        u64 U;
        struct {
            volatile u32 roResponse;
            union {
                volatile u32 roResponseEx;
                struct {
                    volatile u32 roCodecAddr : 4;
                    volatile u32 roUnsolicited : 1;
                    volatile u32 roRsvdEX : 27;
                };
            };
        };
    };
} __attribute__((packed)) HDAudioRirbEntry;

typedef struct {
    union {
        volatile u32 U;
        struct {
            volatile u32 rwParameter   : 8;
            volatile u32 rwVerbId      : 12;
            volatile u32 rwNodeId      : 8;
            volatile u32 rwCodecAddr   : 4;
        };
    };
} __attribute__((packed)) HDAudioCorbEntry;

typedef struct {
    volatile HDAudioRegs* Regs;
    u64 Bar0;
    u8 NumStreams;
    u16 NumCORB;
    u16 NumRIRB;
    u16 CodecVendorId;
    u16 CodecDeviceId;
    struct {
        volatile HDAudioCorbEntry* Corb;
        volatile HDAudioRegCORBWritePtr* CorbWritePtr;
        volatile HDAudioRirbEntry* Rirb;
        volatile HDAudioRegRIRBWritePtr* RirbWritePtr;
        volatile u32* Lpib;
    } DmaAlloc;
    enum {
        HDA_STATE_UNINITIALIZED = 0,
        HDA_STATE_RESETTING,
        HDA_STATE_CODEC_DISCOVERY,
        HDA_STATE_READY,
        HDA_STATE_ERROR
    } State;
} HDAudio;

u64 HDAudioMMIO();
HDAudio* HDAudioInit();