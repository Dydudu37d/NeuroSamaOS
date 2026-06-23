#include "gop.h"
#include "efi.h"
#include "flash.h"
#include "str.h"

extern EFI_GOP_MODE_INFO GopInfo;
extern EFI_GOP_MODE GopMode;
extern u32* GopBack;
extern u32* GopOut;

static inline int abs(int x) {
    return x < 0 ? -x : x;
}

void GOPPixel(u32 x, u32 y, u32 argb) {
    extern u32* GopBack;
    extern EFI_GOP_MODE_INFO GopInfo;
    if (x >= GopInfo.HorizontalResolution || y >= GopInfo.VerticalResolution) return;

    u32 offset = y * GopInfo.PixelsPerScanLine + x;
    GopBack[offset] = ARGBMix(GopBack[offset], argb);
}

void GOPLine(u32 x[2], u32 y[2], u32 argb){
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
        GOPPixel((u32)x1, (u32)y1, argb);
    
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

void GOPRect(u32 x[2], u32 y[2], u32 argb){
    extern u32* GopBack;
    extern EFI_GOP_MODE_INFO GopInfo;
    
    u32 startX = x[0], startY = y[0];
    u32 endX = x[1], endY = y[1];
    
    if (startX >= GopInfo.HorizontalResolution || startY >= GopInfo.VerticalResolution) return;
    if (endX > GopInfo.HorizontalResolution) endX = GopInfo.HorizontalResolution;
    if (endY > GopInfo.VerticalResolution) endY = GopInfo.VerticalResolution;
    if (startX >= endX || startY >= endY) return;
    
    
    for (u32 col = startX; col < endX; col++) {
        GOPPixel(col, startY, argb);
    }
    
    for (u32 col = startX; col < endX; col++) {
        GOPPixel(col, endY - 1, argb);
    }
    
    for (u32 row = startY + 1; row < endY - 1; row++) {
        GOPPixel(startX, row, argb);
    }
    
    for (u32 row = startY + 1; row < endY - 1; row++) {
        GOPPixel(endX - 1, row, argb);
    }
    MemFlash();
}

void GOPRectFill(u32 x[2], u32 y[2], u32 argb){
    extern u32* GopBack;
    extern EFI_GOP_MODE_INFO GopInfo;
    
    u32 startX = x[0], startY = y[0];
    u32 endX = x[1], endY = y[1];
    
    if (startX >= GopInfo.HorizontalResolution || startY >= GopInfo.VerticalResolution) return;
    if (endX > GopInfo.HorizontalResolution) endX = GopInfo.HorizontalResolution;
    if (endY > GopInfo.VerticalResolution) endY = GopInfo.VerticalResolution;
    if (startX >= endX || startY >= endY) return;
    
    for (u32 y=startY;y<endY;y++)
    for (u32 x=startX;x<endX;x++){
        GOPPixel(x, y, argb);
    }
}

void GOPClear(u32 argb){
    extern u32* GopBack;
    extern EFI_GOP_MODE_INFO GopInfo;
    extern EFI_GOP_MODE GopMode;

    MemSet32(GopBack, argb, GopMode.FrameBufferSize/4);

    MemFlash();
}

void GOPClearAlpha(u32 argb) {
    extern u32* GopBack;
    extern EFI_GOP_MODE_INFO GopInfo;
    extern EFI_GOP_MODE GopMode;

    u32 totalPixels = GopMode.FrameBufferSize / 4;

    for (u32 i = 0; i < totalPixels; i++) {
        GopBack[i] = ARGBMix(GopBack[i], argb);
    }
}

void GOPFlash() {
    extern u32* GopBack;
    extern u32* GopOut;
    extern EFI_GOP_MODE_INFO GopInfo;
    extern EFI_GOP_MODE GopMode;

    u32 pixelCount = GopMode.FrameBufferSize / 4;

    if (GopInfo.PixelFormat == 0) {
        for (u32 i = 0; i < pixelCount; i++) {
            GopOut[i] = RGBA2ABGR(GopBack[i]);
        }
    } else {
        MemCopySize32CountByte(GopOut, GopBack, GopMode.FrameBufferSize);
    }
}
