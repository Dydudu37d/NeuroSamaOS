#include "gop.h"
#include "efi.h"
#include "flash.h"
#include "str.h"
#include "debug.h"

extern EFI_GOP_MODE_INFO GopInfo;
extern EFI_GOP_MODE GopMode;
extern HDR_PIXEL* GopBack;
extern void* GopOut;

static inline int abs(int x) {
    return x < 0 ? -x : x;
}

static inline HDR_PIXEL HDRMix_Fixed(HDR_PIXEL dst, HDR_PIXEL src, u8 BitCount) {
    u32 sA = HDR_Unpack_A(src);
    if (sA == 0) return dst;
    if (sA == 0xFFFF) return src;
    
    u32 sR = HDR_Unpack_R(src);
    u32 sG = HDR_Unpack_G(src);
    u32 sB = HDR_Unpack_B(src);
    
    u32 dR = HDR_Unpack_R(dst);
    u32 dG = HDR_Unpack_G(dst);
    u32 dB = HDR_Unpack_B(dst);
    u32 dA = HDR_Unpack_A(dst);
    
    u32 tA = 0xFFFF - sA;
    
    u16 oR = (sR * sA + dR * tA + 0x8000) >> 16;
    u16 oG = (sG * sA + dG * tA + 0x8000) >> 16;
    u16 oB = (sB * sA + dB * tA + 0x8000) >> 16;
    u16 oA = (sA + ((dA * tA + 0x8000) >> 16)) & 0xFFFF;
    
    HDR_PIXEL result = HDR_Pack(oR, oG, oB, oA);
    
    if (BitCount == 10 || BitCount == 12) {
        u64 mask = (1ULL << BitCount) - 1;
        result &= (mask << HDR_R_SHIFT) |
                  (mask << HDR_G_SHIFT) |
                  (mask << HDR_B_SHIFT) |
                  (mask << HDR_A_SHIFT);
    }
    
    return result;
}

void GOPPixel(u32 x, u32 y, HDR_PIXEL hdr) {
    if (x >= GopInfo.PixelsPerScanLine || y >= GopInfo.VerticalResolution) return;

    u32 offset = y * GopInfo.PixelsPerScanLine + x;
    u8 bitCount = 16;
    if (GopInfo.PixelFormat == PixelBitMask) {
        bitCount = GetBitCountFromMask(GopInfo.PixelInformation.RedMask);
    } else if (GopInfo.PixelFormat == PixelRedGreenBlueReserved8BitPerColor ||
               GopInfo.PixelFormat == PixelBlueGreenRedReserved8BitPerColor) {
        bitCount = 8;
    }
    GopBack[offset] = HDRMix_Fixed(GopBack[offset], hdr, bitCount);
}

void GOPLine(u32 x[2], u32 y[2], HDR_PIXEL hdr){
    int x1 = (int)x[0];
    int y1 = (int)y[0];
    int x2 = (int)x[1];
    int y2 = (int)y[1];
    
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);

    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;

    while (1) {
        GOPPixel((u32)x1, (u32)y1, hdr);
    
        if (x1 == x2 && y1 == y2) {
            break;
        }

        int e2 = 2 * err;
        
        if (e2 > -dy) {
              err -= dy;
            x1 += sx;
        }
        
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
    MemFlash();
}

void GOPRect(u32 x[2], u32 y[2], HDR_PIXEL hdr){
    extern HDR_PIXEL* GopBack;
    extern EFI_GOP_MODE_INFO GopInfo;
    
    u32 startX = x[0], startY = y[0];
    u32 endX = x[1], endY = y[1];
    
    if (startX >= GopInfo.PixelsPerScanLine || startY >= GopInfo.VerticalResolution) return;
    if (endX > GopInfo.PixelsPerScanLine) endX = GopInfo.PixelsPerScanLine;
    if (endY > GopInfo.VerticalResolution) endY = GopInfo.VerticalResolution;
    if (startX >= endX || startY >= endY) return;
    
    for (u32 col = startX; col < endX; col++) {
        GOPPixel(col, startY, hdr);
    }
    
    for (u32 col = startX; col < endX; col++) {
        GOPPixel(col, endY - 1, hdr);
    }
    
    for (u32 row = startY + 1; row < endY - 1; row++) {
        GOPPixel(startX, row, hdr);
    }
    
    for (u32 row = startY + 1; row < endY - 1; row++) {
        GOPPixel(endX - 1, row, hdr);
    }
    MemFlash();
}

void GOPRectFill(u32 x[2], u32 y[2], HDR_PIXEL hdr){
    u32 startX = x[0], startY = y[0];
    u32 endX = x[1], endY = y[1];
    
    if (startX >= GopInfo.PixelsPerScanLine || startY >= GopInfo.VerticalResolution) return;
    if (endX > GopInfo.PixelsPerScanLine) endX = GopInfo.PixelsPerScanLine;
    if (endY > GopInfo.VerticalResolution) endY = GopInfo.VerticalResolution;
    if (startX >= endX || startY >= endY) return;
    
    if (((hdr >> HDR_A_SHIFT) & 0xFFFF) == 0xFFFF) {
        for (u32 y = startY; y < endY; y++) {
            u32 offset = y * GopInfo.PixelsPerScanLine + startX;
            for (u32 x = startX; x < endX; x++) {
                GopBack[offset + (x - startX)] = hdr;
            }
        }
    } else {
        u8 bitCount = 16;
        if (GopInfo.PixelFormat == PixelBitMask) {
            bitCount = GetBitCountFromMask(GopInfo.PixelInformation.RedMask);
        } else if (GopInfo.PixelFormat == PixelRedGreenBlueReserved8BitPerColor ||
                   GopInfo.PixelFormat == PixelBlueGreenRedReserved8BitPerColor) {
            bitCount = 8;
        }
        for (u32 y = startY; y < endY; y++) {
            for (u32 x = startX; x < endX; x++) {
                u32 offset = y * GopInfo.PixelsPerScanLine + x;
                GopBack[offset] = HDRMix_Fixed(GopBack[offset], hdr, bitCount);
            }
        }
    }
}

void GOPClear(HDR_PIXEL hdr) {
    u32 pixelCount = GopInfo.PixelsPerScanLine * GopInfo.VerticalResolution;
    MemSet64(GopBack, hdr, pixelCount);
    MemFlash();
}

void GOPClearAlpha(HDR_PIXEL hdr) {
    u32 totalPixels = GopInfo.PixelsPerScanLine * GopInfo.VerticalResolution;
    u8 bitCount = 16;
    if (GopInfo.PixelFormat == PixelBitMask) {
        bitCount = GetBitCountFromMask(GopInfo.PixelInformation.RedMask);
    } else if (GopInfo.PixelFormat == PixelRedGreenBlueReserved8BitPerColor ||
               GopInfo.PixelFormat == PixelBlueGreenRedReserved8BitPerColor) {
        bitCount = 8;
    }

    for (u32 i = 0; i < totalPixels; i++) {
        GopBack[i] = HDRMix_Fixed(GopBack[i], hdr, bitCount);
    }
}

void GOPFlash() {
    u32 pixelCount = GopInfo.PixelsPerScanLine * GopInfo.VerticalResolution;
    u32* fb = (u32*)GopOut;

    if (GopInfo.PixelFormat == PixelBitMask && GopInfo.PixelInformation.RedMask == 0xFFFF000000000000ULL) {
        MemCopy(GopOut, GopBack, pixelCount * sizeof(HDR_PIXEL));
        return;
    }

    if (GopInfo.PixelFormat == PixelBitMask) {
        u32 rMask = GopInfo.PixelInformation.RedMask;
        u32 gMask = GopInfo.PixelInformation.GreenMask;
        u32 bMask = GopInfo.PixelInformation.BlueMask;
        u32 aMask = GopInfo.PixelInformation.ReservedMask;
        
        u32 rShift = __builtin_ctz(rMask);
        u32 gShift = __builtin_ctz(gMask);
        u32 bShift = __builtin_ctz(bMask);
        u32 aShift = __builtin_ctz(aMask);
        
        u32 rBits = __builtin_popcount(rMask);
        u32 gBits = __builtin_popcount(gMask);
        u32 bBits = __builtin_popcount(bMask);
        u32 aBits = __builtin_popcount(aMask);
        
        for (u32 i = 0; i < pixelCount; i++) {
            HDR_PIXEL p = GopBack[i];
            u32 r = (p >> HDR_R_SHIFT) & 0xFFFF;
            u32 g = (p >> HDR_G_SHIFT) & 0xFFFF;
            u32 b = (p >> HDR_B_SHIFT) & 0xFFFF;
            u32 a = (p >> HDR_A_SHIFT) & 0xFFFF;
            
            r = (r * ((1 << rBits) - 1) + 32767) / 65535;
            g = (g * ((1 << gBits) - 1) + 32767) / 65535;
            b = (b * ((1 << bBits) - 1) + 32767) / 65535;
            a = (a * ((1 << aBits) - 1) + 32767) / 65535;
            
            fb[i] = (r << rShift) | (g << gShift) | (b << bShift) | (a << aShift);
        }
        return;
    }

    for (u32 i = 0; i < pixelCount; i++) {
        HDR_PIXEL p = GopBack[i];
        u32 r = ((p >> HDR_R_SHIFT) & 0xFFFF) >> 6; 
        u32 g = ((p >> HDR_G_SHIFT) & 0xFFFF) >> 6; 
        u32 b = ((p >> HDR_B_SHIFT) & 0xFFFF) >> 6; 
        u32 a = ((p >> HDR_A_SHIFT) & 0xFFFF) >> 14;

        if (GopInfo.PixelFormat == PixelRedGreenBlueReserved8BitPerColor) {
            fb[i] = (a << 30) | (b << 20) | (g << 10) | r;
        } else if (GopInfo.PixelFormat == PixelBlueGreenRedReserved8BitPerColor) {
            fb[i] = (a << 30) | (r << 20) | (g << 10) | b;
        }
    }
}