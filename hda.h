#pragma once

#include "int.h"

#define VMIN_RESET   0x00
#define VMAJ_RESET   0x01
#define OUTPAY_RESET 0x3C
#define INPAY_RESET  0x1D
#define GCTL_RESET   0x00

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
        u16 U;
        struct {
            u16 roOSSCount     : 4;
            u16 roISSCount     : 4;
            u16 roBSSCount     : 5;
            u16 roSDOLineCount : 2;
            u16 roHave64bit    : 1;
        };
    };
} __attribute__((packed)) HDAudioRegGCAP;

typedef struct {
    union {
        u8 U;
        struct {
            u8 roSubVersion : 8;
        };
    };
} __attribute__((packed)) HDAudioRegVMIN;

typedef struct {
    union {
        u8 U;
        struct {
            u8 roMainVersion : 8;
        };
    };
} __attribute__((packed)) HDAudioRegVMAJ;

typedef struct {
    union {
        u16 U;
        struct {
            u16 roOutPayLoadCount : 16;
        };
    };
} __attribute__((packed)) HDAudioRegOUTPAY;

typedef struct {
    union {
        u16 U;
        struct {
            u16 roInPayLoadCount : 16;
        };
    };
} __attribute__((packed)) HDAudioRegINPAY;

typedef struct {
    union {
        u32 U;
        struct {
            u32 rsvdpRsvd1            : 24;
            u32 rwUnsolicitedResponse : 1;
            u32 rsvdpRsvd2            : 6;
            u32 rwFlushControl        : 1;
            u32 rwsControllerReset    : 1;
        };
    };
} __attribute__((packed)) HDAudioRegGCTL;

typedef struct {
    union {
        u16 U;
        struct {
            u16 rsvdpRsvd1      : 1;
            u16 rwsSerialDataIn : 15;
        };
    };
} __attribute__((packed)) HDAudioRegWAKEEN;

typedef struct {
    union {
        u16 U;
        struct {
            u16 rsvdzRsvd1        : 1;
            u16 rw1csSerialDataIn : 15;
        };
    };
} __attribute__((packed)) HDAudioRegSTATESTS;

typedef struct {
    union {
        u16 U;
        struct {
            u16 rsvdzRsvd1    : 14;
            u16 rw1cFSTS      : 1;
            u16 rsvdzRsvd2    : 1;
        };
    };
} __attribute__((packed)) HDAudioRegGSTS;

typedef struct {
    u16 roOutStreamPayLoadCapacity;
} __attribute__((packed)) HDAudioRegOUTSTRMPAY;

typedef struct {
    u16 roInStreamPayLoadCapacity;
} __attribute__((packed)) HDAudioRegINSTRMPAY;

typedef struct {
    union {
        u32 U;
        struct {
            u32 rwGlobalInterruptEnable     : 1;
            u32 rwControllerInterruptEnable : 1;
            union {
                u32 rwStreamInterruptEnable : 32;
                struct {
                    u32 rsvdzRsvd1                 : 24;
                    u32 rwBidirectionalStreaming11 : 1;
                    u32 rwInputStream31            : 1;
                    u32 rwInputStream21            : 1;
                    u32 rwInputStream11            : 1;
                    u32 rwInputStream22            : 1;
                    u32 rwInputStream12            : 1;
                };
            };
        };
    };
} __attribute__((packed)) HDAudioRegINTCTL;

typedef struct {
    union {
        u32 U;
        struct {
            u32 roGlobalInterruptStatus     : 1;
            u32 roControllerInterruptStatus : 1;
            u32 roStreamInterruptionStatus  : 30;
        };
    };
} __attribute__((packed)) HDAudioRegINTSTS;

typedef struct {
    u32 Counter;
} __attribute__((packed)) HDAudioRegWALCLK;

typedef struct {
    union {
        u32 U;
        struct {
            u32 rsvdpRsvd1              : 2;
            u32 rwStreamSynchronization : 30;
        };
    };
} __attribute__((packed)) HDAudioRegSSYNC;

typedef struct {
    union {
        u64 U;
        struct {
            union{
                u64 Low : 32;
                struct {
                    u64 rwLowBase                       : 25;
                    u64 roLowerBaseAddressUnimplemented : 7;
                };
            };
            union {
                u64 High : 32;
                struct {
                    u64 rwHighBase : 32;
                };
            };
        };
    };
} __attribute__((packed)) HDAudioRegCORBBase;

typedef struct {
    union {
        u16 U;
        struct {
            u16 rsvdpRsvd1 : 8;
            u16 rwWritePtr : 8;
        };
    };
} __attribute__((packed)) HDAudioRegCORBWritePtr;

typedef struct {
    union {
        u16 U;
        struct {
            u16 rwReset    : 1;
            u16 rsvdpRsvd1 : 7;
            u16 roReadPtr  : 8;
        };
    };
} __attribute__((packed)) HDAudioRegCORBReadPtr;

typedef struct {
    union {
        u8 U;
        struct {
            u8 rsvdpRsvd1                   : 6;
            u8 rwEnableDMAEngine            : 1;
            u8 rwMemoryErrorInterruptEnable : 1;
        };
    };
} __attribute__((packed)) HDAudioRegCORBCTL;

typedef struct {
    union {
        u8 U;
        struct {
            u8 rsvdzRsvd1                     : 7;
            u8 rw1cMemoryErrorInterruptStatus : 1;
        };
    };
} __attribute__((packed)) HDAudioRegCORBSTS;

typedef struct {
    union {
        u8 U;
        struct {
            u8 roSizeCapability : 4;
            u8 rsvdpRsvd1 : 2;
            union {
                u8 roSizeU : 2;
                CORB_SIZE_TYPE roSize : 2;
            };
        };
    };
} __attribute__((packed)) HDAudioRegCORBSize;

typedef struct {
    union {
        u64 U;
        struct {
            union{
                u64 Low : 32;
                struct {
                    u64 rwLowBase                       : 25;
                    u64 roLowerBaseAddressUnimplemented : 7;
                };
            };
            union {
                u64 High : 32;
                struct {
                    u64 rwHighBase : 32;
                };
            };
        };
    };
} __attribute__((packed)) HDAudioRegRIRBBase;

typedef struct {
    union {
        u16 U;
        struct {
            u16 wWritePtrReSet : 1;
            u16 rsvdpRsvd1     : 7;
            u16 roWritePtr     : 8;
        };
    };
} __attribute__((packed)) HDAudioRegRIRBWritePtr;

typedef struct {
    union {
        u16 U;
        struct {
            u16 rsvdpRsvd1               : 8;
            u16 rwResponseInterruptCount : 8;
        };
    };
} __attribute__((packed)) HDAudioRegRINTCNT;

typedef struct {
    union {
        u8 U;
        struct {
            u8 rsvdpRsvd1                        : 5;
            u8 rwResponseOverrunInterruptControl : 1;
            u8 rwDMAEnable                       : 1;
            u8 rwResponseInterruptControl        : 1;
        };
    };
} __attribute__((packed)) HDAudioRegRIRBCTL;

typedef struct {
    union {
        u8 U;
        struct {
            u8 rsvdzRsvd1                         : 5;
            u8 rw1cResponseOverrunInterruptStatus : 1;
            u8 rsvdzRsvd2                         : 1;
            u8 rw1cResponseInterruptStatus        : 1;
        };
    };
} __attribute__((packed)) HDAudioRegRIRBSTS;

typedef struct {
    union {
        u8 U;
        struct {
            u8 roSizeCapability : 4;
            u8 rsvdpRsvd1       : 2;
            u8 roSize           : 2;
        };
    };
} __attribute__((packed)) HDAudioRegRIRBSize;

typedef struct {
    union {
        u64 U;
        struct {
            u64 rwLowBase              : 25;
            u64 rsvdzRsvd1             : 6;
            u64 rwPositionBufferEnable : 1;
            u64 rwHighBase             : 32;
        };
    };
} __attribute__((packed)) HDAudioRegDPBase;


typedef struct{
    union {
        u8 U[3];
        struct {
            u8 rwStreamReset              : 1;
            u8 rwStreamRun                : 1;
            u8 rwInterruptOnComplete      : 1;
            u8 rwFIFOErrorIntEnable       : 1;
            u8 rwDescriptorErrorIntEnable : 1;
            u8 rsvdpRsvd1                 : 3;
            
            u8 rsvdpRsvd2                 : 4;
            u8 rwStreamNumber             : 4;
            u8 rwTrafficPriority          : 1;
            u8 rwBidirectionalDir         : 1;
            u8 rsvdpRsvd3                 : 6;
        };
    };
} __attribute__((packed)) HDAudioRegSDOCTL;

typedef struct {
    union {
        u8 U;
        struct {
            u8 rsvdzRsvd1          : 2;
            u8 roFIFOReady         : 1;
            u8 rw1cDescriptorError : 1;
            u8 rw1cFIFOError       : 4;
        };
    };
} __attribute__((packed)) HDAudioRegSDOSTS;

typedef struct {
    u32 roLinkPositionInBuffer : 32;
} __attribute__((packed)) HDAudioRegSDOLPIB;

typedef struct {
    u32 rwCyclicBufferLength : 32;
} __attribute__((packed)) HDAudioRegSDOCBL;

typedef struct {
    union {
        u16 U;
        struct {
            u16 rsvdpRsvd1       : 8;
            u16 rwLastValidIndex : 8;
        };
    };
} __attribute__((packed)) HDAudioRegSDOLVI;

typedef struct {
    u16 FIFOSize;
} __attribute__((packed)) HDAudioRegSDOFIFOS;

typedef struct {
    union {
        u16 U;
        struct {
            u16 roRsvd1          : 1;
            union {
                u16 BaseRateU            : 1;
                FMT_BASE_RATE rwBaseRate : 1;
            };
            u16 rwRateMultiplier : 3;
            u16 rwRateDivisor    : 3;
            u16 rsvdpRsvd2       : 1;
            u16 rwBitDepth       : 3;
            u16 rwChannelCount   : 4;
        };
    };
} __attribute__((packed)) HDAudioRegSDOFMT;

typedef struct {
    union {
        u64 U;
        struct {
            u64 rwLowBase              : 24;
            u64 rsvdzRsvd1             : 7;
            u64 rwPositionBufferEnable : 1;
            u64 rwHighBase             : 32;
        };
    };
} __attribute__((packed)) HDAudioRegSDODPBase;

typedef struct {
    HDAudioRegSDOCTL CTL;
    HDAudioRegSDOSTS STS;
    HDAudioRegSDOLPIB LPIB;
    HDAudioRegSDOCBL CBL;
    HDAudioRegSDOLVI LVI;
    HDAudioRegSDOFIFOS FIFOS;
    HDAudioRegSDODPBase DPBase;
    u8 Pad[8];
} __attribute__((packed)) HDAudioRegsStreamChannel;

typedef struct {
    u32 roCounter;
} __attribute__((packed)) HDAudioRegWALCLKA;

typedef struct {
    u32 roLinkPositioninBufferAlias;
} __attribute__((packed)) HDAudioRegSDOLICBA;

typedef struct {
    HDAudioRegGCAP GCAP;
    HDAudioRegVMIN VMIN;
    HDAudioRegVMAJ VMAJ;
    HDAudioRegOUTPAY OUTPAY;
    HDAudioRegINPAY INPAY;
    HDAudioRegGCTL GCTL;
    HDAudioRegWAKEEN WAKEEN;
    u8 RsvdToStatesTS[0x20 - 0x12];
    HDAudioRegSTATESTS STATESTS;
    HDAudioRegGSTS GSTS;
    u8 Rsvd1[6];
    HDAudioRegOUTSTRMPAY OUTSTRMPAY;
    HDAudioRegINSTRMPAY INSTRMPAY;
    u8 Rsvd2[4];
    HDAudioRegINTCTL INTCTL;
    HDAudioRegINTSTS INTSTS;
    u8 Rsvd3[8];
    HDAudioRegWALCLK WALCLK;
    u8 Rsvd4[4];
    HDAudioRegSSYNC SSYNC;
    u8 Rsvd5[4];
    HDAudioRegCORBBase CORBBase;
    HDAudioRegCORBWritePtr CORBWritePtr;
    HDAudioRegCORBReadPtr CORBReadPtr;
    HDAudioRegCORBCTL CORBCTL;
    HDAudioRegCORBSTS CORBSTS;
    HDAudioRegCORBSize CORBSize;
    u8 Rsvd6[1];
    HDAudioRegRIRBBase RIRBBase;
    HDAudioRegRIRBWritePtr RIRBWritePtr;
    HDAudioRegRINTCNT RINTCount;
    HDAudioRegRIRBCTL RIRBCTL;
    HDAudioRegRIRBSTS RIRBSTS;
    HDAudioRegRIRBSize RIRBSize;
    u8 Rsvd7[17];
    HDAudioRegDPBase DPBase;
    u8 Rsvd8[8];
    HDAudioRegsStreamChannel Channels[30];
    u8 Rsvd9[7636];
    HDAudioRegWALCLKA WALCLKA;
    HDAudioRegSDOLICBA LPIBA[30];
} __attribute__((packed)) HDAudioRegs;

typedef struct {
    u8 StreamIndex;
    u8 Direction;
    u8 IsActive;
    
    u8* RingBuffer;
    u32 BufferSize;
    u32 ChunkSize;
    
    u16 NextToFeed; 
    void* Bdl;               
} HDAudioStreamContext;

typedef struct {
    union {
        u64 U;
        
        struct {
            union {
                u32 roResponseLow;
            };
            union {
                u32 roResponseHigh;
                struct {
                    u32 roResponseIndex : 4;
                    u32 roUnsolicited   : 1;
                    u32 roRsvdEX        : 23;
                    u32 roCodecAddr     : 4;
                };
            };
        };
    };
} __attribute__((packed)) HDAudioRirbEntry;

typedef struct {
    union {
        u32 U;
        struct {
            u32 rwParameter   : 8;
            u32 rwVerbId      : 12;
            u32 rwNodeId      : 8;
            u32 rwCodecAddr   : 4;
        };
    };
} __attribute__((packed)) HDAudioCorbEntry;

typedef struct {
    volatile HDAudioRegs* Regs; 
    u64 Bar0;
    u8 NumStreams;
    u16 NumCORB;
    
    struct {
        HDAudioCorbEntry* Corb;
        HDAudioRegCORBWritePtr CorbWritePtr;
        HDAudioRirbEntry* Rirb;
        HDAudioRegRIRBWritePtr RirbReadPtr;
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