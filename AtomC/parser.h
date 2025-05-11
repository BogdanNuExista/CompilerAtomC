#pragma once

#include "lexer.h"
#include "ad.h"
#include "stdbool.h"

// Token iterator used by parser
extern Token *iTk;        // the iterator in the tokens list
extern Token *consumedTk; // the last consumed token

// Error reporting function
void tkerr(const char *fmt,...);

// Parser entry point function
void parse(Token *tokens);

// Unit parsing function
bool unit();