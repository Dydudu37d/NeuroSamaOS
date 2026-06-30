#include "DSLMake.h"
#include "int.h"
#include "kmalloc.h"
#include "str.h"
#include "JIT.h"

extern AllocPool KernelPool;

void AddKeyword(Keyword* KeyWord, FishDSL* DSL) {
    Keyword** NewList = (Keyword**)AlignedAlloc(&KernelPool, sizeof(Keyword*) * (DSL->KeywordCount + 1), 64);
    for (u64 i = 0; i < DSL->KeywordCount; i++) {
        NewList[i] = DSL->Keywords[i];
    }
    if (DSL->Keywords) {
        Free(&KernelPool, DSL->Keywords);
    }
    NewList[DSL->KeywordCount] = KeyWord;
    DSL->KeywordCount++;
    DSL->Keywords = NewList;
}

void AddSyntaxRule(SyntaxRule* Rule, FishDSL* DSL) {
    SyntaxRule** NewList = (SyntaxRule**)AlignedAlloc(&KernelPool, sizeof(SyntaxRule*) * (DSL->RuleCount + 1), 64);
    for (u64 i = 0; i < DSL->RuleCount; i++) {
        NewList[i] = DSL->Rules[i];
    }
    if (DSL->Rules) {
        Free(&KernelPool, DSL->Rules);
    }
    NewList[DSL->RuleCount] = Rule;
    DSL->RuleCount++;
    DSL->Rules = NewList;
}

void RemoveSyntaxRule(char* Name, FishDSL* DSL) {
    for (u64 i = 0; i < DSL->RuleCount; i++) {
        if (StrIs(DSL->Rules[i]->Name, Name)) {
            for (u64 j = i; j < DSL->RuleCount - 1; j++) {
                DSL->Rules[j] = DSL->Rules[j+1];
            }
            DSL->RuleCount--;
            return;
        }
    }
}

void ClearRules(FishDSL* DSL) {
    if (DSL->Rules) {
        Free(&KernelPool, DSL->Rules);
        DSL->Rules = 0;
    }
    DSL->RuleCount = 0;
}

ASTNode* CreateASTNode(ASTNodeType type, void* pool) {
    ASTNode* node = (ASTNode*)Alloc((void*)pool, sizeof(ASTNode));
    node->type = type;
    node->children = 0;
    node->next = 0;
    node->parent = 0;
    return node;
}

void AddChild(ASTNode* parent, ASTNode* child) {
    if (!parent->children) {
        parent->children = child;
    } else {
        ASTNode* last = parent->children;
        while (last->next) last = last->next;
        last->next = child;
    }
    child->parent = parent;
}

void SkipWhitespace(ParserContext* ctx) {
    while (*ctx->pos == ' ' || *ctx->pos == '\t' || *ctx->pos == '\n' || *ctx->pos == '\r')
        ctx->pos++;
}

int IsDigit(char c) {
    return c >= '0' && c <= '9';
}

int IsAlpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

int IsIdentChar(char c) {
    return IsAlpha(c) || IsDigit(c) || c == '*' || c == '^' || c == '@' || c == '_' || c == '?';
}

int IsWhitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

char* ReadIdent(ParserContext* ctx) {
    char* start = ctx->pos;
    while (*ctx->pos && IsIdentChar(*ctx->pos))
        ctx->pos++;
    size_t len = ctx->pos - start;
    if (!len) return 0;
    char* result = (char*)Alloc(ctx->pool, len + 1);
    MemCopy(result, start, len);
    result[len] = 0;
    return result;
}

u64 ReadNumber(ParserContext* ctx) {
    u64 value = 0;
    while (*ctx->pos && IsDigit(*ctx->pos)) {
        value = value * 10 + (*ctx->pos - '0');
        ctx->pos++;
    }
    if (*ctx->pos == '_') {
        ctx->pos++;
        while (*ctx->pos && IsDigit(*ctx->pos)) {
            value = value * 10 + (*ctx->pos - '0');
            ctx->pos++;
        }
    }
    if (*ctx->pos == 'M' || *ctx->pos == 'm') {
        ctx->pos++;
        if (*ctx->pos == 'i' || *ctx->pos == 'I') {
            ctx->pos++;
            value = value << 20;
        } else {
            value = value * 1000000;
        }
    } else if (*ctx->pos == 'K' || *ctx->pos == 'k') {
        ctx->pos++;
        if (*ctx->pos == 'i' || *ctx->pos == 'I') {
            ctx->pos++;
            value = value << 10;
        } else {
            value = value * 1000;
        }
    }
    return value;
}

int MatchPattern(ParserContext* ctx, char* pattern, CaptureNode** out_captures) {
    char* p = pattern;
    char* pos = ctx->pos;
    char* save = ctx->pos;
    CaptureNode* head = 0;
    CaptureNode* tail = 0;
    int capture_count = 0;
    
    while (*p) {
        if (*p == '%') {
            p++;
            char* start = pos;
            char* name = p;
            char* name_end = p;
            while (*name_end && (IsAlpha(*name_end) || IsDigit(*name_end))) name_end++;
            char* next_capture = p;
            p = name_end;
            
            if (StrIs("id", name)) {
                while (*pos && IsIdentChar(*pos)) pos++;
            } else if (StrIs("type", name)) {
                while (*pos && IsAlpha(*pos)) pos++;
            } else if (StrIs("expr", name)) {
                int depth = 0;
                while (*pos && !(depth == 0 && (*pos == ';' || *pos == ')' || *pos == ',' || *pos == '}'))) {
                    if (*pos == '(') depth++;
                    if (*pos == ')') depth--;
                    pos++;
                }
            } else if (StrIs("num", name)) {
                while (*pos && IsDigit(*pos)) pos++;
            } else if (StrIs("cond", name)) {
                int depth = 0;
                while (*pos && !(depth == 0 && (*pos == '=' || *pos == ')' || *pos == ';' || *pos == ','))) {
                    if (*pos == '(') depth++;
                    if (*pos == ')') depth--;
                    pos++;
                }
            } else if (StrIs("init", name)) {
                int depth = 0;
                while (*pos && !(depth == 0 && (*pos == '=' || *pos == ')' || *pos == ';'))) {
                    if (*pos == '(') depth++;
                    if (*pos == ')') depth--;
                    pos++;
                }
            } else if (StrIs("inc", name)) {
                while (*pos && *pos != ')' && *pos != ';') pos++;
            } else if (StrIs("str", name)) {
                if (*pos == '"') {
                    pos++;
                    while (*pos && *pos != '"') pos++;
                    if (*pos == '"') pos++;
                }
            } else if (StrIs("body", name)) {
                int depth = 0;
                while (*pos) {
                    if (*pos == '{') depth++;
                    if (*pos == '}') {
                        if (depth == 0) break;
                        depth--;
                    }
                    pos++;
                }
                if (*pos == '}') pos++;
            } else if (StrIs("kw", name)) {
                while (*pos && IsIdentChar(*pos)) pos++;
            } else {
                p = name_end;
                continue;
            }
            
            if (out_captures) {
                CaptureNode* cap = (CaptureNode*)Alloc(ctx->pool, sizeof(CaptureNode));
                cap->Start = start;
                cap->End = pos;
                cap->Name = name;
                cap->Next = 0;
                if (!head) {
                    head = cap;
                } else {
                    tail->Next = cap;
                }
                tail = cap;
                capture_count++;
            }
            continue;
        }
        if (*p == ' ') {
            while (*p == ' ') p++;
            while (*pos == ' ') pos++;
            continue;
        }
        if (*p != *pos) {
            ctx->pos = save;
            return 0;
        }
        p++;
        pos++;
    }
    ctx->pos = pos;
    if (out_captures) {
        *out_captures = head;
    }
    return capture_count;
}

char* CaptureToString(CaptureNode* captures, int index) {
    int i = 0;
    CaptureNode* cur = captures;
    while (cur) {
        if (i == index) {
            int len = cur->End - cur->Start;
            char* result = (char*)Alloc(&KernelPool, len + 1);
            MemCopy(result, cur->Start, len);
            result[len] = 0;
            return result;
        }
        i++;
        cur = cur->Next;
    }
    return 0;
}

void FreeCaptures(CaptureNode* captures, void* pool) {
    CaptureNode* cur = captures;
    while (cur) {
        CaptureNode* next = cur->Next;
        cur = next;
    }
}

ASTNode* ParseIdent(ParserContext* ctx) {
    ASTNode* node = CreateASTNode(AST_IDENT, ctx->pool);
    node->ident_name = ReadIdent(ctx);
    return node;
}

ASTNode* ParseNumber(ParserContext* ctx) {
    ASTNode* node = CreateASTNode(AST_NUMBER, ctx->pool);
    node->num_value = ReadNumber(ctx);
    return node;
}

ASTNode* ParseString(ParserContext* ctx) {
    if (*ctx->pos != '"') return 0;
    ctx->pos++;
    char* start = ctx->pos;
    while (*ctx->pos && *ctx->pos != '"')
        ctx->pos++;
    size_t len = ctx->pos - start;
    ASTNode* node = CreateASTNode(AST_STRING, ctx->pool);
    char* str = (char*)Alloc(ctx->pool, len + 1);
    MemCopy(str, start, len);
    str[len] = 0;
    node->string_value = str;
    if (*ctx->pos == '"') ctx->pos++;
    return node;
}

ASTNode* ParsePrimary(ParserContext* ctx) {
    SkipWhitespace(ctx);
    if (*ctx->pos == '(') {
        ctx->pos++;
        ASTNode* node = ParseExpression(ctx);
        SkipWhitespace(ctx);
        if (*ctx->pos == ')') ctx->pos++;
        return node;
    }
    if (*ctx->pos == '"')
        return ParseString(ctx);
    if (IsDigit(*ctx->pos))
        return ParseNumber(ctx);
    if (IsAlpha(*ctx->pos) || *ctx->pos == '*' || *ctx->pos == '^' || *ctx->pos == '@')
        return ParseIdent(ctx);
    return 0;
}

ASTNode* ParseUnary(ParserContext* ctx) {
    SkipWhitespace(ctx);
    char op = 0;
    FishDSL* dsl = (FishDSL*)ctx->dsl;
    char ptr_sym = dsl ? dsl->PointerSymbol : '*';
    char addr_sym = dsl ? dsl->AddressSymbol : '&';
    char deref_sym = dsl ? dsl->DerefSymbol : '*';
    
    if (*ctx->pos == '-' || *ctx->pos == '!' || *ctx->pos == '~') {
        op = *ctx->pos;
        ctx->pos++;
        ASTNode* operand = ParseUnary(ctx);
        ASTNode* node = CreateASTNode(AST_UNARY, ctx->pool);
        node->unary_node.op = op;
        node->unary_node.operand = operand;
        return node;
    }
    if (*ctx->pos == ptr_sym || *ctx->pos == deref_sym) {
        ctx->pos++;
        ASTNode* operand = ParseUnary(ctx);
        ASTNode* node = CreateASTNode(AST_UNARY, ctx->pool);
        node->unary_node.op = '*';
        node->unary_node.operand = operand;
        return node;
    }
    if (*ctx->pos == addr_sym) {
        ctx->pos++;
        ASTNode* operand = ParseUnary(ctx);
        ASTNode* node = CreateASTNode(AST_UNARY, ctx->pool);
        node->unary_node.op = '&';
        node->unary_node.operand = operand;
        return node;
    }
    return ParsePrimary(ctx);
}

ASTNode* ParseMul(ParserContext* ctx) {
    ASTNode* left = ParseUnary(ctx);
    SkipWhitespace(ctx);
    while (*ctx->pos == '*' || *ctx->pos == '/' || *ctx->pos == '%') {
        char op = *ctx->pos;
        ctx->pos++;
        ASTNode* right = ParseUnary(ctx);
        ASTNode* node = CreateASTNode(AST_BINOP, ctx->pool);
        node->binop_node.left = left;
        node->binop_node.right = right;
        node->binop_node.op = op;
        left = node;
        SkipWhitespace(ctx);
    }
    if (left && left->type == AST_NUMBER && *ctx->pos && IsAlpha(*ctx->pos)) {
        ASTNode* right = ParseUnary(ctx);
        if (right) {
            ASTNode* node = CreateASTNode(AST_BINOP, ctx->pool);
            node->binop_node.left = left;
            node->binop_node.right = right;
            node->binop_node.op = '*';
            left = node;
        }
    }
    return left;
}

ASTNode* ParseAdd(ParserContext* ctx) {
    ASTNode* left = ParseMul(ctx);
    SkipWhitespace(ctx);
    while (*ctx->pos == '+' || *ctx->pos == '-') {
        char op = *ctx->pos;
        ctx->pos++;
        ASTNode* right = ParseMul(ctx);
        ASTNode* node = CreateASTNode(AST_BINOP, ctx->pool);
        node->binop_node.left = left;
        node->binop_node.right = right;
        node->binop_node.op = op;
        left = node;
        SkipWhitespace(ctx);
    }
    return left;
}

ASTNode* ParseCompare(ParserContext* ctx) {
    ASTNode* left = ParseAdd(ctx);
    SkipWhitespace(ctx);
    if (*ctx->pos == '<' || *ctx->pos == '>' || *ctx->pos == '=') {
        char op = *ctx->pos;
        ctx->pos++;
        if (*ctx->pos == '=') {
            ctx->pos++;
            op = (op == '=') ? 'E' : (op == '<') ? 'L' : 'G';
        }
        ASTNode* right = ParseAdd(ctx);
        ASTNode* node = CreateASTNode(AST_BINOP, ctx->pool);
        node->binop_node.left = left;
        node->binop_node.right = right;
        node->binop_node.op = op;
        left = node;
        SkipWhitespace(ctx);
    }
    return left;
}

ASTNode* ParseExpression(ParserContext* ctx) {
    return ParseCompare(ctx);
}

ASTNode* ParseBlock(ParserContext* ctx) {
    SkipWhitespace(ctx);
    if (*ctx->pos != '{') return 0;
    ctx->pos++;
    ASTNode* node = CreateASTNode(AST_BLOCK, ctx->pool);
    while (*ctx->pos && *ctx->pos != '}') {
        SkipWhitespace(ctx);
        if (*ctx->pos == '}') break;
        ASTNode* stmt = ParseStatement(ctx);
        if (stmt) AddChild(node, stmt);
        SkipWhitespace(ctx);
        if (*ctx->pos == ';') ctx->pos++;
    }
    if (*ctx->pos == '}') ctx->pos++;
    return node;
}

ASTNode* ParseCustomStatement(ParserContext* ctx) {
    FishDSL* dsl = (FishDSL*)ctx->dsl;
    if (!dsl || !dsl->Rules) return 0;
    
    for (u64 i = 0; i < dsl->RuleCount; i++) {
        SyntaxRule* rule = dsl->Rules[i];
        char* save = ctx->pos;
        CaptureNode* captures = 0;
        
        int matched = MatchPattern(ctx, rule->Pattern, &captures);
        if (matched > 0) {
            ASTNode* node = CreateASTNode(AST_CUSTOM, ctx->pool);
            node->custom_node.rule = rule;
            node->custom_node.captures = captures;
            if (rule->EmitCustom) {
                rule->EmitCustom((void*)node, (void*)dsl, 0);
            }
            return node;
        }
        ctx->pos = save;
    }
    return 0;
}

ASTNode* ParseKeyword(ParserContext* ctx) {
    FishDSL* dsl = (FishDSL*)ctx->dsl;
    if (!dsl) return 0;
    char* save = ctx->pos;
    char* name = ReadIdent(ctx);
    if (!name) {
        ctx->pos = save;
        return 0;
    }
    for (u64 i = 0; i < dsl->KeywordCount; i++) {
        if (StrIs(name, dsl->Keywords[i]->Name)) {
            ASTNode* node = CreateASTNode(AST_KEYWORD, ctx->pool);
            node->keyword_name = name;
            node->keyword_ptr = dsl->Keywords[i];
            return node;
        }
    }
    ctx->pos = save;
    return 0;
}

ASTNode* ParseVarDecl(ParserContext* ctx) {
    char* save = ctx->pos;
    char* type_name = ReadIdent(ctx);
    if (!type_name) return 0;
    
    SkipWhitespace(ctx);
    char* var_name = ReadIdent(ctx);
    if (!var_name) {
        ctx->pos = save;
        return 0;
    }
    
    ASTNode* node = CreateASTNode(AST_VAR_DECL, ctx->pool);
    node->var_decl_node.var_name = var_name;
    SkipWhitespace(ctx);
    if (*ctx->pos == '=') {
        ctx->pos++;
        node->var_decl_node.init_value = ParseExpression(ctx);
    }
    return node;
}

ASTNode* ParseAssign(ParserContext* ctx) {
    ASTNode* target = ParseIdent(ctx);
    if (!target) return 0;
    SkipWhitespace(ctx);
    if (*ctx->pos != '=') return target;
    ctx->pos++;
    ASTNode* value = ParseExpression(ctx);
    ASTNode* node = CreateASTNode(AST_ASSIGN, ctx->pool);
    node->assign_node.target = target;
    node->assign_node.value = value;
    return node;
}

ASTNode* ParseFuncCall(ParserContext* ctx) {
    char* name = ReadIdent(ctx);
    if (!name) return 0;
    SkipWhitespace(ctx);
    if (*ctx->pos != '(') {
        ctx->pos -= StrLen(name);
        return 0;
    }
    ctx->pos++;
    ASTNode* node = CreateASTNode(AST_FUNC_CALL, ctx->pool);
    node->func_call_node.func_name = name;
    while (*ctx->pos && *ctx->pos != ')') {
        ASTNode* arg = ParseExpression(ctx);
        if (arg) AddChild(node, arg);
        SkipWhitespace(ctx);
        if (*ctx->pos == ',') ctx->pos++;
    }
    if (*ctx->pos == ')') ctx->pos++;
    return node;
}

ASTNode* ParseStatement(ParserContext* ctx) {
    SkipWhitespace(ctx);
    if (!*ctx->pos) return 0;
    if (*ctx->pos == '{') return ParseBlock(ctx);
    
    ASTNode* custom = ParseCustomStatement(ctx);
    if (custom) return custom;
    
    char* save = ctx->pos;
    char* ident = ReadIdent(ctx);
    if (ident) {
        SkipWhitespace(ctx);
        if (*ctx->pos == '(') {
            ctx->pos = save;
            return ParseFuncCall(ctx);
        }
        ctx->pos = save;
    }
    
    ASTNode* kw = ParseKeyword(ctx);
    if (kw) return kw;
    
    ASTNode* node = ParseAssign(ctx);
    if (node) return node;
    node = ParseVarDecl(ctx);
    if (node) return node;
    return ParseExpression(ctx);
}

ASTNode* CodeToAST(char* code, FishDSL* dsl, void* pool) {
    ParserContext ctx;
    ctx.code = code;
    ctx.pos = code;
    ctx.dsl = dsl;
    ctx.pool = pool;
    ctx.root = 0;
    ctx.current = 0;
    
    ASTNode* root = CreateASTNode(AST_BLOCK, pool);
    while (*ctx.pos) {
        SkipWhitespace(&ctx);
        if (!*ctx.pos) break;
        ASTNode* stmt = ParseStatement(&ctx);
        if (stmt) AddChild(root, stmt);
        SkipWhitespace(&ctx);
        if (*ctx.pos == ';') ctx.pos++;
    }
    return root;
}

static void EmitBinOp(char op, u8** p) {
    switch (op) {
        case '+': EmitAddRaxRbx(p); break;
        case '-': EmitSubRaxRbx(p); break;
        case '*': EmitMulRaxRbx(p); break;
        case '/': EmitDivRaxRbx(p); break;
        case '%': EmitModRaxRbx(p); break;
        case '<': EmitCmpRaxRbx(p); EmitSetlRax(p); break;
        case '>': EmitCmpRaxRbx(p); EmitSetgRax(p); break;
        case 'E': EmitCmpRaxRbx(p); EmitSetzRax(p); break;
        case 'L': EmitCmpRaxRbx(p); EmitSetleRax(p); break;
        case 'G': EmitCmpRaxRbx(p); EmitSetgeRax(p); break;
    }
}

void GrammarToJIT(ASTNode* node, FishDSL* dsl) {
    if (!node) return;
    u8* p = 0;
    switch (node->type) {
        case AST_BLOCK: {
            ASTNode* child = node->children;
            while (child) {
                GrammarToJIT(child, dsl);
                child = child->next;
            }
            break;
        }
        case AST_KEYWORD: {
            if (node->keyword_ptr) {
                ((Keyword*)node->keyword_ptr)->ToJITDo();
            }
            break;
        }
        case AST_CUSTOM: {
            if (node->custom_node.rule && node->custom_node.rule->EmitCustom) {
                node->custom_node.rule->EmitCustom((void*)node, (void*)dsl, (void*)p);
            }
            break;
        }
        case AST_IF: {
            GrammarToJIT(node->if_node.cond, dsl);
            u8* end = 0;
            EmitJcc(&p, 0x0F, 0x84, 0, 0);
            u8* jmp_else = p - 6;
            GrammarToJIT(node->if_node.then_branch, dsl);
            if (node->if_node.else_branch) {
                EmitJmp(&p, 0, 0);
                u8* jmp_end = p - 5;
                u8* else_start = p;
                *(s32*)(jmp_else + 2) = (s32)((u64)else_start - (u64)(jmp_else + 6));
                GrammarToJIT(node->if_node.else_branch, dsl);
                u8* end_else = p;
                *(s32*)(jmp_end + 1) = (s32)((u64)end_else - (u64)(jmp_end + 5));
            } else {
                u8* end_then = p;
                *(s32*)(jmp_else + 2) = (s32)((u64)end_then - (u64)(jmp_else + 6));
            }
            break;
        }
        case AST_FOR: {
            GrammarToJIT(node->for_node.init, dsl);
            u8* loop_start = p;
            GrammarToJIT(node->for_node.cond, dsl);
            EmitJcc(&p, 0x0F, 0x84, 0, 0);
            u8* jmp_end = p - 6;
            GrammarToJIT(node->for_node.body, dsl);
            GrammarToJIT(node->for_node.inc, dsl);
            EmitJmp(&p, loop_start, p);
            u8* end_loop = p;
            *(s32*)(jmp_end + 2) = (s32)((u64)end_loop - (u64)(jmp_end + 6));
            break;
        }
        case AST_WHILE: {
            u8* loop_start = p;
            GrammarToJIT(node->for_node.cond, dsl);
            EmitJcc(&p, 0x0F, 0x84, 0, 0);
            u8* jmp_end = p - 6;
            GrammarToJIT(node->for_node.body, dsl);
            EmitJmp(&p, loop_start, p);
            u8* end_loop = p;
            *(s32*)(jmp_end + 2) = (s32)((u64)end_loop - (u64)(jmp_end + 6));
            break;
        }
        case AST_BINOP: {
            GrammarToJIT(node->binop_node.left, dsl);
            EmitPushRax(&p);
            GrammarToJIT(node->binop_node.right, dsl);
            EmitPopRbx(&p);
            EmitBinOp(node->binop_node.op, &p);
            break;
        }
        case AST_UNARY: {
            GrammarToJIT(node->unary_node.operand, dsl);
            if (node->unary_node.op == '-') EmitNegRax(&p);
            if (node->unary_node.op == '!') EmitNotRax(&p);
            if (node->unary_node.op == '~') EmitNotRax(&p);
            if (node->unary_node.op == '*') {
                EmitMovRaxPtrRax(&p);
            }
            break;
        }
        case AST_FUNC_CALL: {
            ASTNode* arg = node->children;
            while (arg) {
                GrammarToJIT(arg, dsl);
                EmitPushRax(&p);
                arg = arg->next;
            }
            char* name = node->func_call_node.func_name;
            u64 serial = 0;
            u64 func_count = GetFuncCount();
            for (u64 i = 0; i < func_count; i++) {
                if (StrIs(name, (char*)GetFuncPoint(i))) {
                    serial = i;
                    break;
                }
            }
            if (serial) {
                void* func = GetFuncPoint(serial);
                EmitCall(&p, func, p);
            }
            break;
        }
        case AST_RETURN: {
            GrammarToJIT(node->return_node.expr, dsl);
            EmitRet(&p);
            break;
        }
        case AST_NUMBER: {
            EmitMovRax(&p, node->num_value);
            break;
        }
        case AST_STRING: {
            EmitMovRax(&p, (u64)node->string_value);
            break;
        }
        case AST_VAR_DECL: {
            u64 serial = VarAdd((JITVar){0, TypeUnsigned|TypeLong, 0});
            if (serial != (u64)-1) {
                if (node->var_decl_node.init_value) {
                    GrammarToJIT(node->var_decl_node.init_value, dsl);
                    void* data = GetVarDataPoint(serial);
                    if (data) {
                        EmitMovPtrRax(&p, (u64)data);
                    }
                }
            }
            break;
        }
        case AST_ASSIGN: {
            GrammarToJIT(node->assign_node.value, dsl);
            if (node->assign_node.target && node->assign_node.target->type == AST_IDENT) {
                u64 serial = 0;
                for (u64 i = 0; i < VarEnd; i++) {
                    if (Vars[i].Type && Vars[i].DataPoint) {
                        if (StrIs((char*)Vars[i].DataPoint, node->assign_node.target->ident_name)) {
                            serial = i;
                            break;
                        }
                    }
                }
                if (serial) {
                    void* data = GetVarDataPoint(serial);
                    if (data) {
                        EmitMovPtrRax(&p, (u64)data);
                    }
                }
            }
            break;
        }
        case AST_IDENT: {
            u64 serial = 0;
            for (u64 i = 0; i < VarEnd; i++) {
                if (Vars[i].Type && Vars[i].DataPoint) {
                    if (StrIs((char*)Vars[i].DataPoint, node->ident_name)) {
                        serial = i;
                        break;
                    }
                }
            }
            if (serial) {
                void* data = GetVarDataPoint(serial);
                if (data) {
                    EmitMovRax(&p, (u64)data);
                }
            }
            break;
        }
        default: {
            break;
        }
    }
}