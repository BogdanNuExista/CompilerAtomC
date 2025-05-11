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
    
    // Initialize domain analysis first
    pushDomain(); // Create global domain
    
    // Then initialize virtual machine
    vmInit();

    printf("virtual machine initialized\n");
    char *src = loadFile(argv[1]); // Load the input file
    Token *tokens = tokenize(src); // Generate tokens

    // Optional: display tokens
    //showTokens(tokens);
    
    // Parse and perform domain analysis
    // Use the parser API properly
    parse(tokens);
    
    // Display symbol table
    showDomain(symTable, "global");
    
    printf("Input is syntactically and semantically correct\n");

    free(src); // Free allocated memory
    
    return 0;
}