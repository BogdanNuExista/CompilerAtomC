#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>

#include "parser.h"
#include "ad.h"

Token *iTk;        // the iterator in the tokens list
Token *consumedTk; // the last consumed token

void tkerr(const char *fmt,...){
    fprintf(stderr,"error in line %d: ",iTk->line);
    va_list va;
    va_start(va,fmt);
    vfprintf(stderr,fmt,va);
    va_end(va);
    fprintf(stderr,"\n");
    exit(EXIT_FAILURE);
}

bool consume(int code){
    if(iTk->code==code){
        consumedTk=iTk;
        iTk=iTk->next;
        return true;
    }
    return false;
}

// typeBase: TYPE_INT | TYPE_DOUBLE | TYPE_CHAR | STRUCT ID
bool typeBase(Type *t){
    // Initialize type with default values
    t->n = -1;  // not an array by default
    
    if(consume(TYPE_INT)){ 
        t->tb = TB_INT;
        return true;
    }
    if(consume(TYPE_DOUBLE)){
        t->tb = TB_DOUBLE;
        return true;
    }
    if(consume(TYPE_CHAR)){
        t->tb = TB_CHAR;
        return true;
    }
    if(consume(STRUCT)){
        Token *tkName;
        if(consume(ID)){
            tkName = consumedTk;
            // Look for the struct symbol in the symbol table
            Symbol *s = findSymbol(tkName->text);
            if(!s) {
                tkerr("undefined struct: %s", tkName->text);
            }
            if(s->kind != SK_STRUCT) {
                tkerr("%s is not a struct", tkName->text);
            }
            t->tb = TB_STRUCT;
            t->s = s;
            return true;
        }
        tkerr("missing struct identifier");
    }
    return false;
}

// arrayDecl: LBRACKET INT? RBRACKET
bool arrayDecl(Type *t){
    if(consume(LBRACKET)){
        if(consume(INT)) {
            // Get the array size from the INT token
            t->n = atoi(consumedTk->text);
        } else {
            // Array without size - will be determined at runtime
            t->n = 0;
        }
        if(consume(RBRACKET)){
            return true;
        }
        tkerr("missing ]");
    }
    return false;
}

// varDef: typeBase ID arrayDecl? SEMICOLON
bool varDef(){
    Token *start = iTk;
    Type t;
    
    if(typeBase(&t)){
        Token *tkName;
        if(consume(ID)){
            tkName = consumedTk;
            
            if(arrayDecl(&t)) {
                // It's an array
            }
            
            if(consume(SEMICOLON)){
                // Create and add symbol to current domain
                Symbol *s = findSymbolInDomain(symTable, tkName->text);
                if(s) {
                    tkerr("symbol redefinition: %s", tkName->text);
                }
                s = newSymbol(tkName->text, SK_VAR);
                s->type = t;
                s->owner = NULL; // Set owner appropriately based on context
                
                // For global variables, allocate memory
                if(symTable->parent == NULL) {
                    s->varMem = malloc(typeSize(&t));
                    if(!s->varMem) {
                        tkerr("not enough memory");
                    }
                } else {
                    // For local variables, set the index
                    if(symTable->parent->symbols && symTable->parent->symbols->kind == SK_FN) {
                        s->owner = symTable->parent->symbols;
                        s->varIdx = symbolsLen(s->owner->fn.locals);
                        addSymbolToList(&s->owner->fn.locals, dupSymbol(s));
                    }
                }
                
                addSymbolToDomain(symTable, s);
                return true;
            }
            tkerr("missing ;");
        }
        
        // Restore position for structDef
        iTk = start;
        return false;
    }
    return false;
}

// structDef: STRUCT ID LACC varDef* RACC SEMICOLON
bool structDef(){
    Token *start = iTk;
    Token *tkName;
    
    if(consume(STRUCT)){
        if(consume(ID)){
            tkName = consumedTk;
            // Check if this is a struct definition (with LACC)
            if(consume(LACC)){
                Symbol *s = findSymbolInDomain(symTable, tkName->text);
                if(s) {
                    tkerr("symbol redefinition: %s", tkName->text);
                }
                
                s = newSymbol(tkName->text, SK_STRUCT);
                s->type.tb = TB_STRUCT;
                s->type.s = s;
                s->type.n = -1;
                addSymbolToDomain(symTable, s);
                
                // Create a new domain for struct members
                Domain *oldDomain = symTable;
                pushDomain();
                
                for(;;){
                    if(!varDef()) break;
                }
                
                // Copy all symbols from the struct domain as struct members
                Symbol *sym;
                for(sym = symTable->symbols; sym; sym = sym->next) {
                    Symbol *newSym = dupSymbol(sym);
                    newSym->owner = s;
                    addSymbolToList(&s->structMembers, newSym);
                }
                
                // Drop the struct domain
                dropDomain();
                symTable = oldDomain;
                
                if(consume(RACC)){
                    if(consume(SEMICOLON)){
                        return true;
                    }
                    tkerr("missing ;");
                }
                tkerr("missing }");
            } 
            // If not a struct definition, restore position
            else {
                iTk = start;
                return false;
            }
        }
        tkerr("missing struct identifier");
    }
    return false;
}

// exprPrimary: ID ( LPAR ( expr ( COMMA expr )* )? RPAR )?
//            | INT | DOUBLE | CHAR | STRING | LPAR expr RPAR

bool exprPrimary();
bool expr(Type *t); 

// exprPostfix: exprPostfix LBRACKET expr RBRACKET
//           | exprPostfix DOT ID
//           | exprPrimary
bool exprPostfix(Type *t){ // ex: acces la elemente de array sau struct
    if(exprPrimary(t)){
        for(;;){
            if(consume(LBRACKET)){
                Type tIdx;
                if(expr(&tIdx)){
                    if(t->n < 0) {
                        tkerr("only arrays can be indexed");
                    }
                    if(tIdx.tb != TB_INT) {
                        tkerr("the index must be of type int");
                    }
                    
                    // Copy base type but remove array dimension
                    Type tElem = *t;
                    tElem.n = -1;
                    *t = tElem;
                    
                    if(consume(RBRACKET)){
                        // continue loop - it's a recursive rule
                    }else{
                        tkerr("missing ]");
                    }
                }else{
                    tkerr("missing expression after [");
                }
            }else if(consume(DOT)){
                if(t->tb != TB_STRUCT) {
                    tkerr("a struct is required before .");
                }
                
                if(consume(ID)){
                    Token *tkName = consumedTk;
                    Symbol *structSym = t->s;
                    Symbol *member = NULL;
                    
                    // Find member in struct
                    for(Symbol *m = structSym->structMembers; m; m = m->next) {
                        if(strcmp(m->name, tkName->text) == 0) {
                            member = m;
                            break;
                        }
                    }
                    
                    if(!member) {
                        tkerr("struct %s does not have a member %s", structSym->name, tkName->text);
                    }
                    
                    // The result type is the member's type
                    *t = member->type;
                    
                    // continue loop - it's a recursive rule
                }else{
                    tkerr("missing identifier after .");
                }
            }else{
                break;
            }
        }
        return true;
    }
    return false;
}

// exprUnary: ( SUB | NOT ) exprUnary | exprPostfix
bool exprUnary(Type *t){
    if(consume(SUB) || consume(NOT)){
        Token *op = consumedTk;
        if(exprUnary(t)){
            // Type checking for operators
            if(op->code == SUB) {
                if(t->tb != TB_INT && t->tb != TB_DOUBLE) {
                    tkerr("unary - cannot be applied to this type");
                }
            } else if(op->code == NOT) {
                if(t->tb != TB_INT) {
                    tkerr("unary ! cannot be applied to this type");
                }
            }
            return true;
        }
        tkerr("invalid unary expression");
    }
    if(exprPostfix(t)){
        return true;
    }
    return false;
}

// exprCast: LPAR typeBase arrayDecl? RPAR exprCast | exprUnary
bool exprCast(Type *t){
    if(consume(LPAR)){
        Token *start = iTk;
        Type tCast;
        if(typeBase(&tCast)){
            if(arrayDecl(&tCast)) {
                // It's an array cast
            }
            if(consume(RPAR)){
                Type tExp;
                if(exprCast(&tExp)){
                    // Check if cast is valid
                    if(tCast.n >= 0 || tExp.n >= 0) {
                        tkerr("cannot cast arrays");
                    }
                    
                    // Check cast compatibility
                    if(tCast.tb == TB_STRUCT || tExp.tb == TB_STRUCT) {
                        tkerr("cannot cast to/from struct");
                    }
                    
                    *t = tCast;
                    return true;
                }
                tkerr("invalid cast expression");
            }
            tkerr("missing )");
        }else{
            // Restore token position - this is not a cast but an expression in parentheses
            iTk = start;
        }
    }
    if(exprUnary(t)){
        return true;
    }
    return false;
}

// forward declarations for expression handling
bool exprMul(Type *t);
bool exprAdd(Type *t);
bool exprRel(Type *t);
bool exprEq(Type *t);
bool exprAnd(Type *t);
bool exprOr(Type *t);
bool exprAssign(Type *t);

// exprMul: exprMul ( MUL | DIV ) exprCast | exprCast
bool exprMul(Type *t){
    if(exprCast(t)){
        for(;;){
            if(consume(MUL) || consume(DIV)){
                //Token *op = consumedTk;
                Type tRight;
                if(exprCast(&tRight)){
                    // Type checking for multiplication/division
                    if(t->n >= 0 || tRight.n >= 0) {
                        tkerr("cannot apply * or / to arrays");
                    }
                    if(t->tb == TB_STRUCT || tRight.tb == TB_STRUCT) {
                        tkerr("cannot apply * or / to structs");
                    }
                    
                    // Result type promotion
                    if(t->tb == TB_DOUBLE || tRight.tb == TB_DOUBLE) {
                        t->tb = TB_DOUBLE;
                    } else {
                        t->tb = TB_INT;
                    }
                    
                    // continue the loop - it's a recursive rule
                }else{
                    tkerr("invalid multiplication expression");
                }
            }else{
                break;
            }
        }
        return true;
    }
    return false;
}

// exprAdd: exprAdd ( ADD | SUB ) exprMul | exprMul
bool exprAdd(Type *t){
    if(exprMul(t)){
        for(;;){
            if(consume(ADD) || consume(SUB)){
                //Token *op = consumedTk;
                Type tRight;
                if(exprMul(&tRight)){
                    // Type checking for addition/subtraction
                    if(t->n >= 0 || tRight.n >= 0) {
                        tkerr("cannot apply + or - to arrays");
                    }
                    if(t->tb == TB_STRUCT || tRight.tb == TB_STRUCT) {
                        tkerr("cannot apply + or - to structs");
                    }
                    
                    // Result type promotion
                    if(t->tb == TB_DOUBLE || tRight.tb == TB_DOUBLE) {
                        t->tb = TB_DOUBLE;
                    } else {
                        t->tb = TB_INT;
                    }
                    
                    // continue the loop - it's a recursive rule
                }else{
                    tkerr("invalid addition expression");
                }
            }else{
                break;
            }
        }
        return true;
    }
    return false;
}

// exprRel: exprRel ( LESS | LESSEQ | GREATER | GREATEREQ ) exprAdd | exprAdd
bool exprRel(Type *t){
    if(exprAdd(t)){
        for(;;){
            if(consume(LESS) || consume(LESSEQ) || consume(GREATER) || consume(GREATEREQ)){
                Type tRight;
                if(exprAdd(&tRight)){
                    // Type checking for relational operators
                    if(t->n >= 0 || tRight.n >= 0) {
                        tkerr("cannot apply relational operators to arrays");
                    }
                    if(t->tb == TB_STRUCT || tRight.tb == TB_STRUCT) {
                        tkerr("cannot apply relational operators to structs");
                    }
                    
                    // Result is always int (boolean)
                    t->tb = TB_INT;
                    t->n = -1;
                    
                    // continue the loop - it's a recursive rule
                }else{
                    tkerr("invalid relational expression");
                }
            }else{
                break;
            }
        }
        return true;
    }
    return false;
}

// exprEq: exprEq ( EQUAL | NOTEQ ) exprRel | exprRel
bool exprEq(Type *t){
    if(exprRel(t)){
        for(;;){
            if(consume(EQUAL) || consume(NOTEQ)){
                Type tRight;
                if(exprRel(&tRight)){
                    // Type checking for equality operators
                    if(t->n >= 0 || tRight.n >= 0) {
                        tkerr("cannot apply equality operators to arrays");
                    }
                    if(t->tb == TB_STRUCT || tRight.tb == TB_STRUCT) {
                        tkerr("cannot apply equality operators to structs");
                    }
                    
                    // Result is always int (boolean)
                    t->tb = TB_INT;
                    t->n = -1;
                    
                    // continue the loop - it's a recursive rule
                }else{
                    tkerr("invalid equality expression");
                }
            }else{
                break;
            }
        }
        return true;
    }
    return false;
}

// exprAnd: exprAnd AND exprEq | exprEq
bool exprAnd(Type *t){
    if(exprEq(t)){
        for(;;){
            if(consume(AND)){
                Type tRight;
                if(exprEq(&tRight)){
                    // Type checking for AND operator
                    if(t->tb != TB_INT || tRight.tb != TB_INT) {
                        tkerr("&& requires int operands");
                    }
                    
                    // Result is always int (boolean)
                    t->tb = TB_INT;
                    t->n = -1;
                    
                    // continue the loop - it's a recursive rule
                }else{
                    tkerr("invalid AND expression");
                }
            }else{
                break;
            }
        }
        return true;
    }
    return false;
}

// exprOr: exprOr OR exprAnd | exprAnd
bool exprOr(Type *t){
    if(exprAnd(t)){
        for(;;){
            if(consume(OR)){
                Type tRight;
                if(exprAnd(&tRight)){
                    // Type checking for OR operator
                    if(t->tb != TB_INT || tRight.tb != TB_INT) {
                        tkerr("|| requires int operands");
                    }
                    
                    // Result is always int (boolean)
                    t->tb = TB_INT;
                    t->n = -1;
                    
                    // continue the loop - it's a recursive rule
                }else{
                    tkerr("invalid OR expression");
                }
            }else{
                break;
            }
        }
        return true;
    }
    return false;
}

// exprAssign: exprUnary ASSIGN exprAssign | exprOr
bool exprAssign(Type *t){
    Token *startTk = iTk;
    Type tLeft;
    if(exprUnary(&tLeft)){
        if(consume(ASSIGN)){
            Type tRight;
            if(exprAssign(&tRight)){
                // Type checking for assignment
                if(tLeft.n >= 0) {
                    tkerr("cannot assign to an array");
                }
                
                // Check for type compatibility
                if(tLeft.tb == TB_STRUCT && tRight.tb == TB_STRUCT) {
                    if(tLeft.s != tRight.s) {
                        tkerr("cannot assign different struct types");
                    }
                } else if(tLeft.tb == TB_STRUCT || tRight.tb == TB_STRUCT) {
                    tkerr("cannot assign to/from a struct and a non-struct");
                }
                
                *t = tLeft;
                return true;
            }
            tkerr("invalid assignment expression");
        }else{
            // If not an assignment, restore position and try exprOr
            iTk = startTk;
        }
    }
    if(exprOr(t)){
        return true;
    }
    return false;
}

// expr: exprAssign
bool expr(Type *t){
    if(exprAssign(t)){
        return true;
    }
    return false;
}

// exprPrimary: ID ( LPAR ( expr ( COMMA expr )* )? RPAR )?
//            | INT | DOUBLE | CHAR | STRING | LPAR expr RPAR
bool exprPrimary(Type *t){
    if(consume(ID)){
        Token *tkName = consumedTk;
        Symbol *s = findSymbol(tkName->text);
        if(!s) {
            tkerr("undefined symbol: %s", tkName->text);
        }
        
        if(consume(LPAR)){
            // Function call
            if(s->kind != SK_FN) {
                tkerr("%s is not a function", tkName->text);
            }
            
            // Check function arguments
            Symbol *param = s->fn.params;
            if(expr(t)){
                // Check parameter type
                if(!param) {
                    tkerr("too many arguments in call to %s", tkName->text);
                }
                // Type checking for first argument would go here
                
                param = param->next;
                
                for(;;){
                    if(consume(COMMA)){
                        if(!param) {
                            tkerr("too many arguments in call to %s", tkName->text);
                        }
                        
                        Type argType;
                        if(expr(&argType)){
                            // Type checking for argument would go here
                            
                            param = param->next;
                            // continue the loop
                        }else{
                            tkerr("invalid expression after ,");
                        }
                    }else{
                        break;
                    }
                }
            }
            
            if(param) {
                tkerr("too few arguments in call to %s", tkName->text);
            }
            
            if(consume(RPAR)){
                *t = s->type;
                return true;
            }
            tkerr("missing )");
        }
        
        // Variable reference
        if(s->kind != SK_VAR && s->kind != SK_PARAM) {
            tkerr("%s is not a variable", tkName->text);
        }
        
        *t = s->type;
        return true;
    }
    
    if(consume(INT)){
        t->tb = TB_INT;
        t->n = -1;
        return true;
    }
    
    if(consume(DOUBLE)){
        t->tb = TB_DOUBLE;
        t->n = -1;
        return true;
    }
    
    if(consume(CHAR)){
        t->tb = TB_CHAR;
        t->n = -1;
        return true;
    }
    
    if(consume(STRING)){
        t->tb = TB_CHAR;
        t->n = 0;  // String is an array of chars
        return true;
    }
    
    if(consume(LPAR)){
        if(expr(t)){
            if(consume(RPAR)){
                return true;
            }
            tkerr("missing )");
        }
        tkerr("invalid expression after (");
    }
    return false;
}

// stm: stmCompound
//    | IF LPAR expr RPAR stm ( ELSE stm )?
//    | WHILE LPAR expr RPAR stm
//    | RETURN expr? SEMICOLON
//    | expr? SEMICOLON
bool stm();

// stmCompound: LACC ( varDef | stm )* RACC
bool stmCompound(){
    if(consume(LACC)){
        // Create a new domain for the compound statement
        pushDomain();
        
        for(;;){
            if(varDef()){}
            else if(stm()){}
            else break;
        }
        
        if(consume(RACC)){
            // Drop the compound statement domain
            dropDomain();
            return true;
        }
        tkerr("missing }");
    }
    return false;
}

// stm: stmCompound
//    | IF LPAR expr RPAR stm ( ELSE stm )?
//    | WHILE LPAR expr RPAR stm
//    | RETURN expr? SEMICOLON
//    | expr? SEMICOLON
bool stm(){
    if(stmCompound()){
        return true;
    }
    if(consume(IF)){
        if(consume(LPAR)){
            Type tCond;
            if(expr(&tCond)){
                // Check if condition is valid (should be convertible to boolean)
                if(tCond.n >= 0) {
                    tkerr("an array cannot be used as a condition");
                }
                if(tCond.tb == TB_STRUCT) {
                    tkerr("a struct cannot be used as a condition");
                }
                
                if(consume(RPAR)){
                    if(stm()){
                        if(consume(ELSE)){
                            if(stm()){
                                return true;
                            }
                            tkerr("missing statement after else");
                        }
                        return true;
                    }
                    tkerr("missing statement after if");
                }
                tkerr("missing )");
            }
            tkerr("missing expression after (");
        }
        tkerr("missing (");
    }
    if(consume(WHILE)){
        if(consume(LPAR)){
            Type tCond;
            if(expr(&tCond)){
                // Check if condition is valid (should be convertible to boolean)
                if(tCond.n >= 0) {
                    tkerr("an array cannot be used as a condition");
                }
                if(tCond.tb == TB_STRUCT) {
                    tkerr("a struct cannot be used as a condition");
                }
                
                if(consume(RPAR)){
                    if(stm()){
                        return true;
                    }
                    tkerr("missing statement after while");
                }
                tkerr("missing )");
            }
            tkerr("missing expression after (");
        }
        tkerr("missing (");
    }
    if(consume(RETURN)){
        // Get current function from the symbol table
        Symbol *crtFn = NULL;
        for(Domain *d = symTable; d; d = d->parent) {
            if(d->parent && d->parent->symbols && d->parent->symbols->kind == SK_FN) {
                crtFn = d->parent->symbols;
                break;
            }
        }
        
        if(!crtFn) {
            tkerr("return outside a function");
        }
        
        Type tRet;
        if(expr(&tRet)){ 
            // Check return type compatibility with function return type
            if(crtFn->type.tb == TB_VOID) {
                tkerr("a void function cannot return a value");
            }
            
            if(tRet.n >= 0 || crtFn->type.n >= 0) {
                tkerr("cannot return an array");
            }
            
            if(tRet.tb == TB_STRUCT || crtFn->type.tb == TB_STRUCT) {
                if(tRet.tb != crtFn->type.tb || tRet.s != crtFn->type.s) {
                    tkerr("return value type doesn't match function return type");
                }
            } else {
                // Allow type promotion and conversion
                if((tRet.tb == TB_INT && crtFn->type.tb == TB_DOUBLE) ||
                   (tRet.tb == TB_CHAR && (crtFn->type.tb == TB_INT || crtFn->type.tb == TB_DOUBLE))) {
                    // These conversions are allowed
                } else if(tRet.tb != crtFn->type.tb) {
                    tkerr("return value type doesn't match function return type");
                }
            }
        } else {
            // No return value
            if(crtFn->type.tb != TB_VOID) {
                tkerr("function must return a value");
            }
        }
        
        if(consume(SEMICOLON)){
            return true;
        }
        tkerr("missing ;");
    }
    Token *start = iTk;
    Type tExpr;
    if(expr(&tExpr)){ // optional
        // Expression is present, must end with semicolon
    }
    if(consume(SEMICOLON)){
        return true;
    }
    // If we tried to parse an expression but didn't find a semicolon, give an error
    if(iTk != start){
        tkerr("missing ;");
    }
    return false;
}

// fnParam: typeBase ID arrayDecl?
bool fnParam(Symbol *fn){
    Type t;
    Token *tkName;
    
    if(typeBase(&t)){
        if(consume(ID)){
            tkName = consumedTk;
            
            if(arrayDecl(&t)) {
                // It's an array parameter
            }
            
            // Check for parameter redefinition
            for(Symbol *p = fn->fn.params; p; p = p->next) {
                if(strcmp(p->name, tkName->text) == 0) {
                    tkerr("redefinition of parameter: %s", tkName->text);
                }
            }
            
            // Add the parameter to the function
            addFnParam(fn, tkName->text, t);
            
            return true;
        }
        tkerr("missing parameter identifier");
    }
    return false;
}

// fnDef: ( typeBase | VOID ) ID LPAR ( fnParam ( COMMA fnParam )* )? RPAR stmCompound
bool fnDef(){
    Token *start = iTk;
    Type t;
    Token *tkName;
    //bool isVoid = false;
    
    if(typeBase(&t)){
        // Function has a return type
    } else if(consume(VOID)){
        //isVoid = true;
        t.tb = TB_VOID;
        t.n = -1;
    } else {
        return false;
    }
    
    if(consume(ID)){
        tkName = consumedTk;
        
        if(consume(LPAR)){
            // Check for function redefinition
            Symbol *s = findSymbolInDomain(symTable, tkName->text);
            if(s) {
                tkerr("symbol redefinition: %s", tkName->text);
            }
            
            // Create function symbol
            s = newSymbol(tkName->text, SK_FN);
            s->type = t;
            addSymbolToDomain(symTable, s);
            
            // Set current function for parameter handling
            Symbol *currentFn = s;
            
            if(fnParam(currentFn)){
                for(;;){
                    if(consume(COMMA)){
                        if(fnParam(currentFn)){
                            // continue loop
                        }else{
                            tkerr("invalid function parameter after ,");
                        }
                    }else{
                        break;
                    }
                }
            }
            
            if(consume(RPAR)){
                // Create a new domain for the function body
                pushDomain();
                
                // Add parameters as local variables
                for(Symbol *param = currentFn->fn.params; param; param = param->next) {
                    Symbol *localParam = dupSymbol(param);
                    localParam->kind = SK_PARAM;
                    localParam->owner = currentFn;
                    addSymbolToDomain(symTable, localParam);
                }
                
                if(stmCompound()){
                    // We're already done with the function domain from stmCompound
                    return true;
                }
                tkerr("missing function body");
            }
            tkerr("missing )");
        }
        // Restore position - this might be a variable definition
        iTk = start;
    }
    // Restore position - this might be a variable definition
    iTk = start;
    return false;
}

// unit: ( structDef | fnDef | varDef )* END
bool unit(){
    for(;;){
        if(structDef()){}
        else if(fnDef()){}
        else if(varDef()){}
        else break;
    }
    if(consume(END)){
        return true;
    }
    tkerr("syntax error at the end of file");
    return false;
}

void parse(Token *tokens){
    iTk=tokens;
    if(!unit())tkerr("syntax error");
}