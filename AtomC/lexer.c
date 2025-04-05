#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "lexer.h"
#include "utils.h"

Token *tokens;    // single linked list of tokens
Token *lastTk;    // the last token in list

int line = 1;     // the current line in the input file

Token *addTk(int code) {
    Token *tk = safeAlloc(sizeof(Token));
    tk->code = code;
    tk->line = line;
    tk->next = NULL;
    if(lastTk) {
        lastTk->next = tk;
    } else {
        tokens = tk;
    }
    lastTk = tk;
    return tk;
}

char *extract(const char *begin, const char *end) {
    int length = end - begin;
    char *str = safeAlloc(length + 1);
    memcpy(str, begin, length);
    str[length] = '\0';
    return str;
}

Token *tokenize(const char *pch) {
    const char *start;
    for(;;) {
        switch(*pch) {
            case ' ': case '\t': pch++; break;
            case '\r':
                if(pch[1] == '\n') pch++;
                
            case '\n':
                line++;
                pch++;
                break;
            case '\0':
                addTk(END);
                return tokens;
            case ',':
                addTk(COMMA);
                pch++;
                break;
            case ';':
                addTk(SEMICOLON);
                pch++;
                break;
            case '(':
                addTk(LPAR);
                pch++;
                break;
            case ')':
                addTk(RPAR);
                pch++;
                break;
            case '{':
                addTk(LACC);
                pch++;
                break;
            case '}':
                addTk(RACC);
                pch++;
                break;
            case '[':
                addTk(LBRACKET);
                pch++;
                break;
            case ']':
                addTk(RBRACKET);
                pch++;
                break;
            case '+':
                addTk(ADD);
                pch++;
                break;
            case '-':
                addTk(SUB);
                pch++;
                break;
            case '*':
                addTk(MUL);
                pch++;
                break;
            case '.':
                addTk(DOT);
                pch++;
                break;
            case '&':
                if(pch[1] == '&') {
                    addTk(AND);
                    pch += 2;
                } else {
                    err("Invalid &");
                }
                break;
            case '|':
                if(pch[1] == '|') {
                    addTk(OR);
                    pch += 2;
                } else {
                    err("Invalid |");
                }
                break;
            case '=':
                if(pch[1] == '=') {
                    addTk(EQUAL);
                    pch += 2;
                } else {
                    addTk(ASSIGN);
                    pch++;
                }
                break;
            case '!':
                if(pch[1] == '=') {
                    addTk(NOTEQ);
                    pch += 2;
                } else {
                    err("Invalid !");
                }
                break;
            case '<':
                if(pch[1] == '=') {
                    addTk(LESSEQ);
                    pch += 2;
                } else {
                    addTk(LESS);
                    pch++;
                }
                break;
            case '>':
                if(pch[1] == '=') {
                    addTk(GREATEREQ);
                    pch += 2;
                } else {
                    addTk(GREATER);
                    pch++;
                }
                break;
            case '/':
                if(pch[1] == '/') {
                    while(*pch != '\n' && *pch != '\0') pch++;
                } else {
                    addTk(DIV);
                    pch++;
                }
                break;
            case '\"': { // String literal 
                start = ++pch;
                while(*pch != '\"' && *pch != '\0') pch++;
                if(*pch == '\0') err("Unclosed string");
                char *str = extract(start, pch);
                Token *tk = addTk(STRING);
                tk->text = str;
                pch++;
                break;
            }
            case '\'': { // Char literal
                if(pch[1] == '\0' || pch[2] != '\'') err("Invalid char literal");
                Token *tk = addTk(CHAR);
                tk->c = pch[1];
                pch += 3;
                break;
            }
            default:
                if(isalpha(*pch) || *pch == '_') { // Identifiers/keywords
                    start = pch++;
                    while(isalnum(*pch) || *pch == '_') pch++;
                    char *text = extract(start, pch);
                    
                    // Keyword checks
                    if(strcmp(text, "char") == 0) addTk(TYPE_CHAR);
                    else if(strcmp(text, "double") == 0) addTk(TYPE_DOUBLE);
                    else if(strcmp(text, "else") == 0) addTk(ELSE);
                    else if(strcmp(text, "if") == 0) addTk(IF);
                    else if(strcmp(text, "int") == 0) addTk(TYPE_INT);
                    else if(strcmp(text, "return") == 0) addTk(RETURN);
                    else if(strcmp(text, "struct") == 0) addTk(STRUCT);
                    else if(strcmp(text, "void") == 0) addTk(VOID);
                    else if(strcmp(text, "while") == 0) addTk(WHILE);
                    else {
                        Token *tk = addTk(ID);
                        tk->text = text;
                    }
                }
                else if(isdigit(*pch)) { // Numbers
                    start = pch;
                    int isDouble = 0;
                    while(isdigit(*pch) || *pch == '.' || 
                          tolower(*pch) == 'e' || *pch == '+' || *pch == '-') {
                        if(*pch == '.' || tolower(*pch) == 'e' || tolower(*pch)=='E') isDouble = 1;
                            pch++;
                            
                        // if it contains a letter that is not e or E => error
                        if(isalpha(*pch) && tolower(*pch) != 'e') {
                            err("Invalid number format: %.*s", (int)(pch - start), start);
                        }

                        // check if a + or - has before it an e or E 
                        if ((*pch == '+' || *pch == '-') && tolower(*(pch - 1)) != 'e') {
                            err("Invalid number format: %.*s", (int)(pch - start), start);
                        }

                        // check if e or E has before it a number
                        if ((*pch == 'e' || *pch == 'E') && !isdigit(*(pch - 1))) {
                            err("Invalid number format: %.*s", (int)(pch - start), start);
                        }
                        
                    }
                    
                    // not a valid number if it ends with . or e or E or + or -
                    if (*(pch - 1) == '.' || tolower(*(pch - 1)) == 'e' || *(pch - 1) == '+' || *(pch - 1) == '-') {
                        err("Invalid number format: %.*s", (int)(pch - start), start);
                    }

                    char *numStr = extract(start, pch);

                    Token *tk = addTk(isDouble ? DOUBLE : INT);
                    if(isDouble) tk->d = atof(numStr);
                    else tk->i = atoi(numStr);
                    free(numStr);
                }
                else {
                    err("Invalid character: %c (ASCII %d)", *pch, *pch);
                }
        }
    }
}

void showTokens(const Token *tokens) {
    const char *tokenNames[] = {
        "ID", "TYPE_CHAR", "TYPE_DOUBLE", "ELSE", "IF", "TYPE_INT", "RETURN", 
        "STRUCT", "VOID", "WHILE", "COMMA", "SEMICOLON", "LPAR", "RPAR", 
        "LBRACKET", "RBRACKET", "LACC", "RACC", "END", "ADD", "SUB", "MUL", 
        "DIV", "DOT", "AND", "OR", "NOT", "ASSIGN", "EQUAL", "NOTEQ", "LESS", 
        "LESSEQ", "GREATER", "GREATEREQ", "INT", "DOUBLE", "CHAR", "STRING"
    };

    for(const Token *tk = tokens; tk; tk = tk->next) {
        printf("%d\t%s", tk->line, tokenNames[tk->code]);
        switch(tk->code) {
            case ID: case STRING:
                printf(":%s", tk->text);
                break;
            case INT:
                printf(":%d", tk->i);
                break;
            case DOUBLE:
                printf(":%g", tk->d);
                break;
            case CHAR:
                printf(":%c", tk->c);
                break;
        }
        printf("\n");
    }
}