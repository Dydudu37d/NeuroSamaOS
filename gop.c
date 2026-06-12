#include "gop.h"
#include "efi.h"
#include "str.h"

static inline int abs(int x) {
    return x < 0 ? -x : x;
}

void GOPPixel(u32 x, u32 y, u32 rgba){
    extern u32* GopBack;
    extern EFI_GOP_MODE_INFO GopInfo;
    if (x>=GopInfo.HorizontalResolution || \
        y>=GopInfo.VerticalResolution) return;

    GopBack[y*GopInfo.PixelsPerScanLine+x]=GopInfo.PixelFormat==0?RGBA2ABGR(rgba):rgba;
}

void GOPLine(u32 x[2], u32 y[2], u32 rgba){
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
        GOPPixel((u32)x1, (u32)y1, rgba);
    
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
}

void GOPRect(u32 x[2], u32 y[2], u32 rgba){
    extern u32* GopBack;
    extern EFI_GOP_MODE_INFO GopInfo;
    
    u32 startX = x[0], startY = y[0];
    u32 endX = x[1], endY = y[1];
    
    if (startX >= GopInfo.HorizontalResolution || startY >= GopInfo.VerticalResolution) return;
    if (endX > GopInfo.HorizontalResolution) endX = GopInfo.HorizontalResolution;
    if (endY > GopInfo.VerticalResolution) endY = GopInfo.VerticalResolution;
    if (startX >= endX || startY >= endY) return;
    
    u32 pixel = (GopInfo.PixelFormat == 0) ? RGBA2ABGR(rgba) : rgba;
    
    for (u32 col = startX; col < endX; col++) {
        GOPPixel(col, startY, pixel);
    }
    
    for (u32 col = startX; col < endX; col++) {
        GOPPixel(col, endY - 1, pixel);
    }
    
    for (u32 row = startY + 1; row < endY - 1; row++) {
        GOPPixel(startX, row, pixel);
    }
    
    for (u32 row = startY + 1; row < endY - 1; row++) {
        GOPPixel(endX - 1, row, pixel);
    }
}

void GOPRectFill(u32 x[2], u32 y[2], u32 rgba){
    extern u32* GopBack;
    extern EFI_GOP_MODE_INFO GopInfo;
    
    u32 startX = x[0], startY = y[0];
    u32 endX = x[1], endY = y[1];
    
    if (startX >= GopInfo.HorizontalResolution || startY >= GopInfo.VerticalResolution) return;
    if (endX > GopInfo.HorizontalResolution) endX = GopInfo.HorizontalResolution;
    if (endY > GopInfo.VerticalResolution) endY = GopInfo.VerticalResolution;
    if (startX >= endX || startY >= endY) return;
    
    u32 width = endX - startX;
    u32 stride = GopInfo.PixelsPerScanLine;
    u32 pixel = (GopInfo.PixelFormat == 0) ? RGBA2ABGR(rgba) : rgba;
    
    u32* firstRow = GopBack + startY * stride + startX;
    for (u32 i = 0; i < width; i++) {
        firstRow[i] = pixel;
    }
    
    u32 rowSize = width * sizeof(u32);
    for (u32 row = startY + 1; row < endY; row++) {
        u32* dst = GopBack + row * stride + startX;
        MemCopy(dst, firstRow, rowSize);
    }
}

void GOPClear(u32 rgba){
    extern u32* GopBack;
    extern EFI_GOP_MODE_INFO GopInfo;
    
    u32 width = GopInfo.HorizontalResolution;
    u32 height = GopInfo.VerticalResolution;
    u32 stride = GopInfo.PixelsPerScanLine;
    u32 pixel = (GopInfo.PixelFormat == 0) ? RGBA2ABGR(rgba) : rgba;
    
    u32* firstRow = GopBack;
    for (u32 i = 0; i < width; i++) {
        firstRow[i] = pixel;
    }
    
    u32 rowSize = width * sizeof(u32);
    for (u32 row = 1; row < height; row++) {
        u32* dst = GopBack + row * stride;
        MemCopy(dst, firstRow, rowSize);
    }
}

void GOPFlash(){
    extern u32* GopBack;
    extern u32* GopOut;
    extern EFI_GOP_MODE GopMode;

    MemCopy(GopOut, GopBack, GopMode.FrameBufferSize);

    asm volatile("sfence" ::: "memory");
}
