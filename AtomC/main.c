#include "lexer.h"
#include "utils.h"
#include "parser.h"
#include "ad.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: %s <input_file>\n", argv[0]);
        return 1;
    }
    char *src = loadFile(argv[1]); // Load the input file
    Token *tokens = tokenize(src); // Generate tokens

    showTokens(tokens);
    
    parse(tokens); // Parse the tokens
    
    printf("Input is syntactically correct\n");

    free(src); // Free allocated memory

    return 0;
}