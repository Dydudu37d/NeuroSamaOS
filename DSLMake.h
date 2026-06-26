#pragma once

#include "int.h"

typedef struct Keyword Keyword;
typedef struct Keyword{
    char* Name;
    u16 Type;
    void (*ToJITDo)(void);
}__attribute__((packed)) Keyword;

typedef struct{ 
    u64 KeywordCount;
    Keyword* *Keywords;
}__attribute__((packed)) FishDSL;

typedef struct Grammar Grammar;
typedef struct Grammar{
    Keyword* Base;
    Grammar* Pre;
    Grammar* Next;
}__attribute__((packed)) Grammar;

void AddKeyword(Keyword* KeyWord,FishDSL* DSL);
Grammar* CodeToGrammar(char* Code,FishDSL DSL);
void* GrammarToJIT(Grammar* Grammars,FishDSL DSL);

