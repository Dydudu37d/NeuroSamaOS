#include "DSLMake.h"
#include "int.h"
#include "kmalloc.h"
#include "str.h"

extern AllocPool KernelPool;

void AddKeyword(Keyword *KeyWord, FishDSL *DSL){
    Keyword* *KeywordList=AlignedAlloc(&KernelPool, sizeof(Keyword*)*(DSL->KeywordCount+1), 64);
    MemCopy(KeywordList,DSL->Keywords,sizeof(Keyword*)*DSL->KeywordCount);
    KeywordList[DSL->KeywordCount]=KeyWord;
    DSL->KeywordCount++;
}

Grammar* CodeToGrammar(char *Code, FishDSL DSL){
    Grammar a={0};
}

void* GrammarToJIT(Grammar *Grammars, FishDSL DSL){
    
}

