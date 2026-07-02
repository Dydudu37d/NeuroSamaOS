#include "NeuroLossless.h"
#include "int.h"
#include "kmalloc.h"
#include "str.h"

extern AllocPool KernelPool;

_Bool InList(u32* List,u32 Obj,u64 Size){
    for (u64 Idx=0;Idx<Size;Idx++) if(List[Idx]==Obj) return 1;
    return 0;
}

u64 FindList(u32* List,u32 Obj,u64 Size){
    for (u64 Idx=0;Idx<Size;Idx++) if(List[Idx]==Obj) return Idx;
    return Size+1;
}

NeuroLossless Compress(u32* Data,u64 Size){
    NeuroLossless Return;
    Return.Char1='N';
    
    if (Size<=50){
        Return.Compression=0;
        Return.Data=Data;
        Return.PosMap=0;
        Return.Size=Size;
        Return.OriginalSize=Size;
        return Return;
    }
    
    u32* UniqueData=AlignedAlloc(&KernelPool,sizeof(u32)*Size,16);
    u32* PosMap=AlignedAlloc(&KernelPool,sizeof(u32)*Size,16);
    u64 UniqueCount=0;
    
    for (u64 Idx=0;Idx<Size;Idx++){
        u64 Pos=FindList(UniqueData,Data[Idx],UniqueCount);
        if (Pos>UniqueCount){
            UniqueData[UniqueCount]=Data[Idx];
            PosMap[Idx]=UniqueCount;
            UniqueCount++;
        }else{
            PosMap[Idx]=Pos;
        }
    }
    
    u64 CompressedSize=UniqueCount*sizeof(u32)+Size*sizeof(u32);
    u64 OriginalSize=Size*sizeof(u32);
    
    if (CompressedSize>=OriginalSize){
        Free(&KernelPool,UniqueData);
        Free(&KernelPool,PosMap);
        Return.Compression=0;
        Return.Data=Data;
        Return.PosMap=0;
        Return.Size=Size;
        Return.OriginalSize=Size;
        return Return;
    }
    
    u32* CompressedData=AlignedAlloc(&KernelPool,sizeof(u32)*UniqueCount,16);
    for (u64 Idx=0;Idx<UniqueCount;Idx++){
        CompressedData[Idx]=UniqueData[Idx];
    }
    
    Return.Compression=1;
    Return.Data=CompressedData;
    Return.PosMap=PosMap;
    Return.Size=UniqueCount;
    Return.OriginalSize=Size;
    
    Free(&KernelPool,UniqueData);
    return Return;
}

u32* DeCompress(NeuroLossless Data){
    if (Data.Char1!='N') return NULL;
    if (!Data.Compression){
        return Data.Data;
    }
    
    u32* DecompressedData=AlignedAlloc(&KernelPool,sizeof(u32)*Data.OriginalSize,16);
    if (!DecompressedData) return NULL;
    
    for (u64 Idx=0;Idx<Data.OriginalSize;Idx++){
        DecompressedData[Idx]=Data.Data[Data.PosMap[Idx]];
    }
    return DecompressedData;
}