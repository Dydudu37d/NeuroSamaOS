#include "DSLMake.h"
#include "int.h"
#include "kmalloc.h"
#include "str.h"

extern AllocPool KernelPool;

void AddKeyword(Keyword *KeyWord, FishDSL *DSL){
    Keyword* *KeywordList=AlignedAlloc(&KernelPool, sizeof(Keyword*)*(DSL->KeywordCount+1), 64);
    MemCopy(KeywordList,DSL->Keywords,sizeof(Keyword*)*DSL->KeywordCount);
    Free(&KernelPool,DSL->Keywords);
    KeywordList[DSL->KeywordCount]=KeyWord;
    DSL->KeywordCount++;
    DSL->Keywords=KeywordList;
}

Grammar* CodeToGrammar(char *Code, FishDSL DSL){
    Grammar *Grammars=Alloc(&KernelPool,sizeof(Grammar)*(StrLen(Code)>>8));
    
}

void* GrammarToJIT(Grammar *Grammars, FishDSL DSL){
    
}

