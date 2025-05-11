#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>

#include "parser.h"
#include "ad.h"
#include "utils.h"  // Added for safeAlloc

Token *iTk;        // the iterator in the tokens list
Token *consumedTk; // the last consumed token
Symbol *owner = NULL; // current owner symbol (struct or fn)

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
    t->n = -1; // not an array by default
    
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
        if(consume(ID)){
            Token *tkName = consumedTk;
            // Look for struct symbol
            Symbol *s = findSymbol(tkName->text);
            if(!s) {
                tkerr("structura nedefinita: %s", tkName->text);
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
            t->n = consumedTk->i; // Set array size
        } else {
            t->n = 0; // Array without specified size
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
                if(t.n == 0) tkerr("a vector variable must have a specified dimension");
            }
            
            if(consume(SEMICOLON)){
                // Check for symbol redefinition
                Symbol *var = findSymbolInDomain(symTable, tkName->text);
                if(var) tkerr("symbol redefinition: %s", tkName->text);
                
                // Create new symbol
                var = newSymbol(tkName->text, SK_VAR);
                var->type = t;
                var->owner = owner;
                addSymbolToDomain(symTable, var);
                
                // Handle based on owner
                if(owner){
                    switch(owner->kind){
                    case SK_FN:
                        var->varIdx = symbolsLen(owner->fn.locals);
                        addSymbolToList(&owner->fn.locals, dupSymbol(var));
                        break;
                    case SK_STRUCT:
                        var->varIdx = typeSize(&owner->type);
                        addSymbolToList(&owner->structMembers, dupSymbol(var));
                        break;
                    case SK_VAR:  // Added to prevent warning
                    case SK_PARAM: // Added to prevent warning
                        // These cases shouldn't occur as owner
                        tkerr("invalid owner kind for variable %s", tkName->text);
                        break;
                    }
                } else {
                    var->varMem = safeAlloc(typeSize(&t));
                }
                
                return true;
            }
            tkerr("missing ; after variable declaration");
        }
        // Restore position for other rules
        iTk = start;
        return false;
    }
    return false;
}

// structDef: STRUCT ID LACC varDef* RACC SEMICOLON
bool structDef(){
    Token *start = iTk;
    Token *tkName;
    Symbol *oldOwner;
    
    if(consume(STRUCT)){
        if(consume(ID)){
            tkName = consumedTk;
            if(consume(LACC)){
                // Check for struct redefinition
                Symbol *s = findSymbolInDomain(symTable, tkName->text);
                if(s) tkerr("symbol redefinition: %s", tkName->text);
                
                // Create struct symbol
                s = newSymbol(tkName->text, SK_STRUCT);
                s->type.tb = TB_STRUCT;
                s->type.s = s;
                s->type.n = -1;
                addSymbolToDomain(symTable, s);
                
                // Save previous owner and set new owner
                oldOwner = owner;
                owner = s;
                pushDomain();
                
                // Parse struct members
                for(;;){
                    if(!varDef()) break;
                }
                
                if(consume(RACC)){
                    if(consume(SEMICOLON)){
                        // Restore owner and drop domain
                        owner = oldOwner;
                        dropDomain();
                        return true;
                    }
                    tkerr("missing ; after struct definition");
                }
                tkerr("missing } after struct body");
            } 
            // Not a struct definition
            iTk = start;
            return false;
        }
        tkerr("missing struct identifier");
    }
    return false;
}

// exprPrimary: ID ( LPAR ( expr ( COMMA expr )* )? RPAR )?
//            | INT | DOUBLE | CHAR | STRING | LPAR expr RPAR
bool exprPrimary();
bool expr(); 

// exprPostfix: exprPostfix LBRACKET expr RBRACKET
//           | exprPostfix DOT ID
//           | exprPrimary
bool exprPostfix(){
    if(exprPrimary()){
        for(;;){
            if(consume(LBRACKET)){
                if(expr()){
                    if(consume(RBRACKET)){
                        // continue loop
                    }else{
                        tkerr("missing ]");
                    }
                }else{
                    tkerr("missing expression after [");
                }
            }else if(consume(DOT)){
                if(consume(ID)){
                    // continue loop
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
bool exprUnary(){
    if(consume(SUB) || consume(NOT)){
        if(exprUnary()){
            return true;
        }
        tkerr("invalid unary expression");
    }
    if(exprPostfix()){
        return true;
    }
    return false;
}

// exprCast: LPAR typeBase arrayDecl? RPAR exprCast | exprUnary
bool exprCast(){
    if(consume(LPAR)){
        Token *start = iTk;
        Type t;
        if(typeBase(&t)){
            arrayDecl(&t); // optional
            if(consume(RPAR)){
                if(exprCast()){
                    return true;
                }
                tkerr("invalid cast expression");
            }
            tkerr("missing )");
        }else{
            // Restore position - not a cast
            iTk = start;
        }
    }
    if(exprUnary()){
        return true;
    }
    return false;
}

// Forward declarations for expression handling
bool exprMul();
bool exprAdd();
bool exprRel();
bool exprEq();
bool exprAnd();
bool exprOr();
bool exprAssign();

// exprMul: exprMul ( MUL | DIV ) exprCast | exprCast
bool exprMul(){
    if(exprCast()){
        for(;;){
            if(consume(MUL) || consume(DIV)){
                if(exprCast()){
                    // continue loop
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
bool exprAdd(){
    if(exprMul()){
        for(;;){
            if(consume(ADD) || consume(SUB)){
                if(exprMul()){
                    // continue loop
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
bool exprRel(){
    if(exprAdd()){
        for(;;){
            if(consume(LESS) || consume(LESSEQ) || consume(GREATER) || consume(GREATEREQ)){
                if(exprAdd()){
                    // continue loop
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
bool exprEq(){
    if(exprRel()){
        for(;;){
            if(consume(EQUAL) || consume(NOTEQ)){
                if(exprRel()){
                    // continue loop
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
bool exprAnd(){
    if(exprEq()){
        for(;;){
            if(consume(AND)){
                if(exprEq()){
                    // continue loop
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
bool exprOr(){
    if(exprAnd()){
        for(;;){
            if(consume(OR)){
                if(exprAnd()){
                    // continue loop
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
bool exprAssign(){
    Token *startTk = iTk;
    if(exprUnary()){
        if(consume(ASSIGN)){
            if(exprAssign()){
                return true;
            }
            tkerr("invalid assignment expression");
        }else{
            // Not an assignment, restore position
            iTk = startTk;
        }
    }
    if(exprOr()){
        return true;
    }
    return false;
}

// expr: exprAssign
bool expr(){
    if(exprAssign()){
        return true;
    }
    return false;
}

// exprPrimary: ID ( LPAR ( expr ( COMMA expr )* )? RPAR )?
//            | INT | DOUBLE | CHAR | STRING | LPAR expr RPAR
bool exprPrimary(){
    if(consume(ID)){
        if(consume(LPAR)){
            if(expr()){
                for(;;){
                    if(consume(COMMA)){
                        if(expr()){
                            // continue loop
                        }else{
                            tkerr("invalid expression after ,");
                        }
                    }else{
                        break;
                    }
                }
            }
            if(consume(RPAR)){
                return true;
            }
            tkerr("missing ) in function call");
        }
        return true;
    }
    if(consume(INT) || consume(DOUBLE) || consume(CHAR) || consume(STRING)){
        return true;
    }
    if(consume(LPAR)){
        if(expr()){
            if(consume(RPAR)){
                return true;
            }
            tkerr("missing ) in expression");
        }
        tkerr("invalid expression after (");
    }
    return false;
}

// Forward declaration
bool stm();

// stmCompound: LACC ( varDef | stm )* RACC
bool stmCompound(bool newDomain){
    if(consume(LACC)){
        if(newDomain) pushDomain();
        
        for(;;){
            if(varDef()){}
            else if(stm()){}
            else break;
        }
        
        if(consume(RACC)){
            if(newDomain) dropDomain();
            return true;
        }
        tkerr("missing } in compound statement");
    }
    return false;
}

// stm: stmCompound
//    | IF LPAR expr RPAR stm ( ELSE stm )?
//    | WHILE LPAR expr RPAR stm
//    | RETURN expr? SEMICOLON
//    | expr? SEMICOLON
bool stm(){
    if(stmCompound(true)){
        return true;
    }
    if(consume(IF)){
        if(consume(LPAR)){
            if(expr()){
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
            if(expr()){
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
        expr(); // optional
        if(consume(SEMICOLON)){
            return true;
        }
        tkerr("missing ;");
    }
    Token *start = iTk;
    if(expr()){ // optional
        // Expression found
    }
    if(consume(SEMICOLON)){
        return true;
    }
    if(iTk != start){
        tkerr("missing ;");
    }
    return false;
}

// fnParam: typeBase ID arrayDecl?
bool fnParam(){
    Type t;
    Token *tkName;
    
    if(typeBase(&t)){
        if(consume(ID)){
            tkName = consumedTk;
            
            if(arrayDecl(&t)) {
                t.n = 0; // Reset dimension for array parameters
            }
            
            // Check for parameter redefinition
            Symbol *param = findSymbolInDomain(symTable, tkName->text);
            if(param) tkerr("symbol redefinition: %s", tkName->text);
            
            // Create parameter symbol
            param = newSymbol(tkName->text, SK_PARAM);
            param->type = t;
            param->owner = owner;
            param->paramIdx = symbolsLen(owner->fn.params);
            
            // Add parameter to domain and function
            addSymbolToDomain(symTable, param);
            addSymbolToList(&owner->fn.params, dupSymbol(param));
            
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
    
    if(typeBase(&t) || (consume(VOID) && (t.tb = TB_VOID, true))){
        if(consume(ID)){
            tkName = consumedTk;
            
            if(consume(LPAR)){
                // Check for function redefinition
                Symbol *fn = findSymbolInDomain(symTable, tkName->text);
                if(fn) tkerr("symbol redefinition: %s", tkName->text);
                
                // Create function symbol
                fn = newSymbol(tkName->text, SK_FN);
                fn->type = t;
                addSymbolToDomain(symTable, fn);
                
                // Set owner and create function domain
                owner = fn;
                pushDomain();
                
                // Parse parameters
                if(fnParam()){
                    for(;;){
                        if(consume(COMMA)){
                            if(fnParam()){
                                // continue loop
                            }else{
                                tkerr("invalid parameter after ,");
                            }
                        }else{
                            break;
                        }
                    }
                }
                
                if(consume(RPAR)){
                    if(stmCompound(false)){ // Don't create a new domain
                        // Cleanup after function
                        dropDomain();
                        owner = NULL;
                        return true;
                    }
                    tkerr("missing function body");
                }
                tkerr("missing )");
            } else {
                // Not a function, restore position
                iTk = start;
                return false;
            }
        }
        // Restore position
        iTk = start;
    }
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
    tkerr("unexpected token at end of file");
    return false;
}

void parse(Token *tokens){
    // Initialize domain analysis
    pushDomain(); // Global domain
    owner = NULL;
    
    iTk = tokens;
    if(!unit()) tkerr("syntax error");
    
    // Display symbol table (optional)
    // showDomain(symTable, "global");
}