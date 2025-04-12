#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>

#include "parser.h"

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
bool typeBase(){
    if(consume(TYPE_INT)){ 
        return true;
    }
    if(consume(TYPE_DOUBLE)){
        return true;
    }
    if(consume(TYPE_CHAR)){
        return true;
    }
    if(consume(STRUCT)){
        if(consume(ID)){
            return true;
        }
        tkerr("missing struct identifier");
    }
    return false;
}

// arrayDecl: LBRACKET INT? RBRACKET
bool arrayDecl(){
    if(consume(LBRACKET)){
        consume(INT); // INT is optional
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
    
    if(typeBase()){
        if(consume(ID)){
            arrayDecl(); // optional
            if(consume(SEMICOLON)){
                return true;
            }
            tkerr("missing ; after variable declaration");
        }
        else{
            if(iTk->code == LBRACKET) {
                tkerr("missing identifier before array declaration");
            }

            // pentru ca structDef sa fie corect, ne intoarcem la inceputul lui varDef
            iTk = start;
            return false;
        }
    }
    return false;
}

// structDef: STRUCT ID LACC varDef* RACC SEMICOLON
bool structDef(){
    Token *start = iTk; 
    
    if(consume(STRUCT)){
        if(consume(ID)){
            // Check if this is a struct definition (with LACC)
            if(consume(LACC)){
                for(;;){
                    if(!varDef()) break;
                }
                if(consume(RACC)){
                    if(consume(SEMICOLON)){
                        return true;
                    }
                    tkerr("missing ; after struct definition");
                }
                tkerr("missing } after struct body");
            } 
            // Daca nu are LACC poate fi doar declarare
            else {

                Token *lookAhead = iTk;
                
                // Verificam daca e un ID sau un tip de date, daca nu e atunci e o struct
                if(lookAhead->code == TYPE_INT || 
                   lookAhead->code == TYPE_DOUBLE || 
                   lookAhead->code == TYPE_CHAR ||
                   lookAhead->code == STRUCT) {
                    tkerr("missing { from struct definition");
                }

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

bool expr(); 

// exprPostfix: exprPostfix LBRACKET expr RBRACKET
//           | exprPostfix DOT ID
//           | exprPrimary
bool exprPostfix(){ // ex: acces la elemente de array sau struct
    if(exprPrimary()){
        for(;;){
            if(consume(LBRACKET)){
                if(expr()){
                    if(consume(RBRACKET)){
                        // continue loop - it's a recursive rule
                    }else{
                        tkerr("missing ]");
                    }
                }else{
                    tkerr("missing expression after [");
                }
            }else if(consume(DOT)){
                if(consume(ID)){
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
        if(typeBase()){
            arrayDecl(); // optional
            if(consume(RPAR)){
                if(exprCast()){
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
    if(exprUnary()){
        return true;
    }
    return false;
}

// forward declarations for expression handling
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
bool exprAdd(){
    if(exprMul()){
        for(;;){
            if(consume(ADD) || consume(SUB)){
                if(exprMul()){
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
bool exprRel(){
    if(exprAdd()){
        for(;;){
            if(consume(LESS)){
                if(exprAdd()){
                    // continue the loop
                }else{
                    tkerr("invalid relational expression after '<'");
                }
            }else if(consume(LESSEQ)){
                if(exprAdd()){
                    // continue the loop
                }else{
                    tkerr("invalid relational expression after '<='");
                }
            }else if(consume(GREATER)){
                if(exprAdd()){
                    // continue the loop
                }else{
                    tkerr("invalid relational expression after '>'");
                }
            }else if(consume(GREATEREQ)){
                if(exprAdd()){
                    // continue the loop
                }else{
                    tkerr("invalid relational expression after '>='");
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
            if(consume(EQUAL)){
                if(exprRel()){
                    // continue the loop - it's a recursive rule
                }else{
                    tkerr("invalid equality expression after '=='");
                }
            }else if(consume(NOTEQ)){
                if(exprRel()){
                    // continue the loop - it's a recursive rule
                }else{
                    tkerr("invalid equality expression after '!='");
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
bool exprOr(){
    if(exprAnd()){
        for(;;){
            if(consume(OR)){
                if(exprAnd()){
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
bool exprAssign(){
    Token *startTk = iTk;
    if(exprUnary()){
        if(consume(ASSIGN)){
            if(exprAssign()){
                return true;
            }
            tkerr("invalid assignment expression");
        }else{
            // If not an assignment, restore position and try exprOr
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
                            // continue the loop
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

// stm: stmCompound
//    | IF LPAR expr RPAR stm ( ELSE stm )?
//    | WHILE LPAR expr RPAR stm
//    | RETURN expr? SEMICOLON
//    | expr? SEMICOLON
bool stm();

// stmCompound: LACC ( varDef | stm )* RACC
bool stmCompound(){
    if(consume(LACC)){
        for(;;){
            if(varDef()){}
            else if(stm()){}
            else break;
        }
        if(consume(RACC)){
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
    if(stmCompound()){
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
bool fnParam(){
    if(typeBase()){
        if(consume(ID)){
            arrayDecl(); // optional
            return true;
        }
        tkerr("missing parameter identifier");
    }
    return false;
}

// fnDef: ( typeBase | VOID ) ID LPAR ( fnParam ( COMMA fnParam )* )? RPAR stmCompound
bool fnDef(){
    Token *start = iTk;
    if(typeBase() || consume(VOID)){
        if(consume(ID)){
            if(consume(LPAR)){
                if(fnParam()){
                    for(;;){
                        if(consume(COMMA)){
                            if(fnParam()){
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
                    if(stmCompound()){
                        return true;
                    }
                    tkerr("missing function body");
                }
                tkerr("missing )");
            } else {
                // verificam daca ar fi o declarare de variabila
                if(iTk->code == LBRACKET) {
                    // This is likely a variable definition
                    iTk = start;
                    return false;
                }
                tkerr("missing ( after function name");
            }
        } else {
            tkerr("missing function name");
        }
        // Restore position if we couldn't match a function definition
        iTk = start;
    }
    return false;
}

// unit: ( structDef | fnDef | varDef )* END
// unit: ( structDef | fnDef | varDef )* END
bool unit(){
    for(;;){
        if(structDef()){}
        else if(fnDef()){}
        else if(varDef()){}
        else {
            // daca nu suntem la final afisam un mesaj de eroare in functie de tipul tokenului
            if(iTk->code != END) {
                if(iTk->code == STRUCT) {
                    tkerr("invalid struct definition or declaration");
                } else if(iTk->code == TYPE_INT || iTk->code == TYPE_DOUBLE ||
                         iTk->code == TYPE_CHAR || iTk->code == VOID) {
                    tkerr("invalid variable or function definition");
                } else {
                    tkerr("unexpected token in global scope");
                }
            }
            break;
        }
    }
    if(consume(END)){
        return true;
    }
    tkerr("unexpected token at end of file");
    return false;
}

void parse(Token *tokens){
    iTk=tokens;
    if(!unit())tkerr("syntax error");
}