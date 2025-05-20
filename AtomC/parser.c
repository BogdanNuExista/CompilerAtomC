#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>

#include "parser.h"
#include "ad.h"
#include "at.h"    // Added for type analysis
#include "utils.h"

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

// Forward declarations with return type tracking
bool exprPrimary(Ret *r);
bool expr(Ret *r); 

// exprPostfix: exprPostfix LBRACKET expr RBRACKET
//           | exprPostfix DOT ID
//           | exprPrimary
bool exprPostfix(Ret *r){
    if(exprPrimary(r)){
        for(;;){
            if(consume(LBRACKET)){
                if(r->type.n < 0) {
                    tkerr("only an array can be indexed");
                }
                
                Ret idx;
                if(expr(&idx)) {
                    Type tInt = {TB_INT, NULL, -1};
                    if(!convTo(&idx.type, &tInt)) {
                        tkerr("the index is not convertible to int");
                    }
                    
                    // Result is element type (remove array dimension)
                    r->type.n = -1;
                    r->lval = true;
                    r->ct = false;
                    
                    if(consume(RBRACKET)){
                        // continue loop - expression is recursive
                    } else {
                        tkerr("missing ]");
                    }
                } else {
                    tkerr("missing expression after [");
                }
            } else if(consume(DOT)){
                if(r->type.tb != TB_STRUCT) {
                    tkerr("a field can only be selected from a struct");
                }
                
                if(consume(ID)){
                    Token *tkName = consumedTk;
                    Symbol *s = findSymbolInList(r->type.s->structMembers, tkName->text);
                    
                    if(!s) {
                        tkerr("the structure %s does not have a field %s", 
                               r->type.s->name, tkName->text);
                    }
                    
                    // Result is the field's type
                    *r = (Ret){s->type, true, s->type.n >= 0};
                } else {
                    tkerr("missing identifier after .");
                }
            } else {
                break;
            }
        }
        return true;
    }
    return false;
}

// exprUnary: ( SUB | NOT ) exprUnary | exprPostfix
bool exprUnary(Ret *r){
    if(consume(SUB) || consume(NOT)){
        Token *op = consumedTk;
        
        if(exprUnary(r)){
            if(!canBeScalar(r)) {
                tkerr("unary - or ! must have a scalar operand");
            }
            
            // For NOT operator, result is always int
            if(op->code == NOT) {
                r->type.tb = TB_INT;
                r->type.n = -1;
                r->type.s = NULL;
            }
            
            r->lval = false;
            r->ct = true;
            return true;
        }
        tkerr("invalid unary expression");
    }
    if(exprPostfix(r)){
        return true;
    }
    return false;
}

// exprCast: LPAR typeBase arrayDecl? RPAR exprCast | exprUnary
// exprCast: LPAR typeBase arrayDecl? RPAR exprCast | exprUnary
bool exprCast(Ret *r){
    if(consume(LPAR)){
        Token *start = iTk;
        Type t;
        
        if(typeBase(&t)){
            arrayDecl(&t); // optional
            
            if(consume(RPAR)){
                Ret op;
                // CHANGE THIS LINE: Use exprUnary instead of recursive exprCast call
                if(exprUnary(&op)){
                    // Allow struct-to-struct casts of the same type
                    if(t.tb == TB_STRUCT && op.type.tb == TB_STRUCT) {
                        if(t.s != op.type.s) {
                            tkerr("cannot cast between different struct types");
                        }
                        // Same struct type cast is allowed - don't report an error here
                    } 
                    // Don't allow casting between struct and non-struct
                    else if(t.tb == TB_STRUCT) {
                        tkerr("cannot convert to a struct type");
                    }
                    else if(op.type.tb == TB_STRUCT) {
                        tkerr("cannot convert a struct");
                    }
                    
                    // Array conversion validation
                    if(op.type.n >= 0 && t.n < 0) {
                        tkerr("an array can be converted only to another array");
                    }
                    if(op.type.n < 0 && t.n >= 0) {
                        tkerr("a scalar can be converted only to another scalar");
                    }
                    
                    *r = (Ret){t, false, true};
                    return true;
                }
                tkerr("invalid expression after cast");
            }
            tkerr("missing )");
        }else{
            // Restore position - not a cast but an expression in parentheses
            iTk = start;
        }
    }
    if(exprUnary(r)){
        return true;
    }
    return false;
}

// exprMul: exprMul ( MUL | DIV ) exprCast | exprCast
bool exprMul(Ret *r){
    if(exprCast(r)){
        for(;;){
            if(consume(MUL) || consume(DIV)){
                Ret right;
                
                if(exprCast(&right)){
                    Type tDst;
                    if(!arithTypeTo(&r->type, &right.type, &tDst)) {
                        tkerr("invalid operand type for * or /");
                    }
                    
                    *r = (Ret){tDst, false, true};
                } else {
                    tkerr("invalid multiplication expression");
                }
            } else {
                break;
            }
        }
        return true;
    }
    return false;
}

// exprAdd: exprAdd ( ADD | SUB ) exprMul | exprMul
bool exprAdd(Ret *r){
    if(exprMul(r)){
        for(;;){
            if(consume(ADD) || consume(SUB)){
                Ret right;
                
                if(exprMul(&right)){
                    Type tDst;
                    if(!arithTypeTo(&r->type, &right.type, &tDst)) {
                        tkerr("invalid operand type for + or -");
                    }
                    
                    *r = (Ret){tDst, false, true};
                } else {
                    tkerr("invalid addition expression");
                }
            } else {
                break;
            }
        }
        return true;
    }
    return false;
}

// exprRel: exprRel ( LESS | LESSEQ | GREATER | GREATEREQ ) exprAdd | exprAdd
bool exprRel(Ret *r){
    if(exprAdd(r)){
        for(;;){
            if(consume(LESS) || consume(LESSEQ) || consume(GREATER) || consume(GREATEREQ)){
                Ret right;
                
                if(exprAdd(&right)){
                    Type tDst;
                    if(!arithTypeTo(&r->type, &right.type, &tDst)) {
                        tkerr("invalid operand type for <, <=, >, >=");
                    }
                    
                    // Result is always an int (boolean)
                    *r = (Ret){{TB_INT, NULL, -1}, false, true};
                } else {
                    tkerr("invalid relational expression");
                }
            } else {
                break;
            }
        }
        return true;
    }
    return false;
}

// exprEq: exprEq ( EQUAL | NOTEQ ) exprRel | exprRel
bool exprEq(Ret *r){
    if(exprRel(r)){
        for(;;){
            if(consume(EQUAL) || consume(NOTEQ)){
                Ret right;
                
                if(exprRel(&right)){
                    Type tDst;
                    if(!arithTypeTo(&r->type, &right.type, &tDst)) {
                        tkerr("invalid operand type for == or !=");
                    }
                    
                    // Result is always an int (boolean)
                    *r = (Ret){{TB_INT, NULL, -1}, false, true};
                } else {
                    tkerr("invalid equality expression");
                }
            } else {
                break;
            }
        }
        return true;
    }
    return false;
}

// exprAnd: exprAnd AND exprEq | exprEq
bool exprAnd(Ret *r){
    if(exprEq(r)){
        for(;;){
            if(consume(AND)){
                Ret right;
                
                if(exprEq(&right)){
                    Type tDst;
                    if(!arithTypeTo(&r->type, &right.type, &tDst)) {
                        tkerr("invalid operand type for &&");
                    }
                    
                    // Result is always an int (boolean)
                    *r = (Ret){{TB_INT, NULL, -1}, false, true};
                } else {
                    tkerr("invalid AND expression");
                }
            } else {
                break;
            }
        }
        return true;
    }
    return false;
}

// exprOr: exprOr OR exprAnd | exprAnd
bool exprOr(Ret *r){
    if(exprAnd(r)){
        for(;;){
            if(consume(OR)){
                Ret right;
                
                if(exprAnd(&right)){
                    Type tDst;
                    if(!arithTypeTo(&r->type, &right.type, &tDst)) {
                        tkerr("invalid operand type for ||");
                    }
                    
                    // Result is always an int (boolean)
                    *r = (Ret){{TB_INT, NULL, -1}, false, true};
                } else {
                    tkerr("invalid OR expression");
                }
            } else {
                break;
            }
        }
        return true;
    }
    return false;
}

// exprAssign: exprUnary ASSIGN exprAssign | exprOr
bool exprAssign(Ret *r){
    Token *startTk = iTk;
    Ret rDst;
    
    if(exprUnary(&rDst)){
        if(consume(ASSIGN)){
            if(exprAssign(r)){
                // Check if destination is a valid lvalue
                if(!rDst.lval) {
                    tkerr("the assign destination must be a left-value");
                }
                if(rDst.ct) {
                    tkerr("the assign destination cannot be constant");
                }
                
                // Check if both operands are scalar
                if(!canBeScalar(&rDst)) {
                    tkerr("the assign destination must be scalar");
                }
                if(!canBeScalar(r)) {
                    tkerr("the assign source must be scalar");
                }
                
                // Check type compatibility
                if(!convTo(&r->type, &rDst.type)) {
                    tkerr("the assign source cannot be converted to destination");
                }
                
                // Assignment result is the destination type
                r->type = rDst.type;
                r->lval = false;
                r->ct = false;
                return true;
            }
            tkerr("invalid assignment expression");
        } else {
            // Not an assignment, restore position and try exprOr
            iTk = startTk;
        }
    }
    if(exprOr(r)){
        return true;
    }
    return false;
}

// expr: exprAssign
bool expr(Ret *r){
    if(exprAssign(r)){
        return true;
    }
    return false;
}

// exprPrimary: ID ( LPAR ( expr ( COMMA expr )* )? RPAR )?
//            | INT | DOUBLE | CHAR | STRING | LPAR expr RPAR
bool exprPrimary(Ret *r){
    if(consume(ID)){
        Token *tkName = consumedTk;
        Symbol *s = findSymbol(tkName->text);
        
        if(!s) {
            tkerr("undefined id: %s", tkName->text);
        }
        
        if(consume(LPAR)){
            // Function call
            if(s->kind != SK_FN) {
                tkerr("only a function can be called");
            }
            
            // Check function arguments
            Ret rArg;
            Symbol *param = s->fn.params;
            
            if(expr(&rArg)){
                if(!param) {
                    tkerr("too many arguments in function call");
                }
                
                // Check parameter type compatibility
                if(!convTo(&rArg.type, &param->type)) {
                    tkerr("in call, cannot convert the argument type to the parameter type");
                }
                
                param = param->next;
                
                for(;;){
                    if(consume(COMMA)){
                        if(!param) {
                            tkerr("too many arguments in function call");
                        }
                        
                        if(expr(&rArg)){
                            // Check parameter type compatibility
                            if(!convTo(&rArg.type, &param->type)) {
                                tkerr("in call, cannot convert the argument type to the parameter type");
                            }
                            
                            param = param->next;
                        } else {
                            tkerr("invalid expression after ,");
                        }
                    } else {
                        break;
                    }
                }
            }
            
            if(param) {
                tkerr("too few arguments in function call");
            }
            
            if(consume(RPAR)){
                // Result is the function's return type
                *r = (Ret){s->type, false, true};
                return true;
            }
            tkerr("missing ) in function call");
        } else {
            // Variable reference
            if(s->kind == SK_FN) {
                tkerr("a function can only be called");
            }
            
            // Result is the variable's type
            *r = (Ret){s->type, true, s->type.n >= 0};
            return true;
        }
    }
    
    if(consume(INT)){
        *r = (Ret){{TB_INT, NULL, -1}, false, true};
        return true;
    }
    
    if(consume(DOUBLE)){
        *r = (Ret){{TB_DOUBLE, NULL, -1}, false, true};
        return true;
    }
    
    if(consume(CHAR)){
        *r = (Ret){{TB_CHAR, NULL, -1}, false, true};
        return true;
    }
    
    if(consume(STRING)){
        *r = (Ret){{TB_CHAR, NULL, 0}, false, true};
        return true;
    }
    
    if(consume(LPAR)){
        // First, check if this could be a cast by peeking ahead
        Token *savedTk = iTk;
        
        // Try to parse as a type
        Type t;
        //bool isCast = false;
        
        if(typeBase(&t)){
            arrayDecl(&t); // optional
            if(consume(RPAR)){
                // It's a cast expression
                Ret op;
                if(exprUnary(&op)){
                    // Handle the cast
                    // Allow struct-to-struct casts of the same type
                    if(t.tb == TB_STRUCT && op.type.tb == TB_STRUCT) {
                        if(t.s != op.type.s) {
                            tkerr("cannot cast between different struct types");
                        }
                        // Same struct type cast is allowed
                    } 
                    else if(t.tb == TB_STRUCT) {
                        tkerr("cannot convert to a struct type");
                    }
                    else if(op.type.tb == TB_STRUCT) {
                        tkerr("cannot convert a struct");
                    }
                    
                    // Array conversion validation
                    if(op.type.n >= 0 && t.n < 0) {
                        tkerr("an array can be converted only to another array");
                    }
                    if(op.type.n < 0 && t.n >= 0) {
                        tkerr("a scalar can be converted only to another scalar");
                    }
                    
                    *r = (Ret){t, false, true};
                    return true;
                }
                tkerr("invalid expression after cast");
            }
        }
        
        // Restore position - not a cast, try as normal parenthesized expression
        iTk = savedTk;
        
        if(expr(r)){
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
    Ret rCond, rExpr;
    
    if(stmCompound(true)){
        return true;
    }
    if(consume(IF)){
        if(consume(LPAR)){
            if(expr(&rCond)){
                // Check if condition is scalar
                if(!canBeScalar(&rCond)) {
                    tkerr("the if condition must be a scalar value");
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
            if(expr(&rCond)){
                // Check if condition is scalar
                if(!canBeScalar(&rCond)) {
                    tkerr("the while condition must be a scalar value");
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
        // Validate return statement
        if(expr(&rExpr)){ 
            // Check return value against function return type
            if(owner->type.tb == TB_VOID) {
                tkerr("a void function cannot return a value");
            }
            
            if(!canBeScalar(&rExpr)) {
                tkerr("the return value must be a scalar value");
            }
            
            if(!convTo(&rExpr.type, &owner->type)) {
                tkerr("cannot convert the return expression type to the function return type");
            }
        } else {
            // No return value provided
            if(owner->type.tb != TB_VOID) {
                tkerr("a non-void function must return a value");
            }
        }
        
        if(consume(SEMICOLON)){
            return true;
        }
        tkerr("missing ;");
    }
    Token *start = iTk;
    if(expr(&rExpr)){ 
        // Expression statement
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
}