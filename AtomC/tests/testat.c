struct S {
    int x;
    double y;
};

int globalArray[10];
struct S globalStruct;

// Function declarations for testing
void voidFn() {
    // Test case #4: VOID function cannot return a value
    // return 5;
}

double convFn(int x) {
    // Test case #6: Return value must be convertible to function return type

    
    // return "string";  // Error: string cannot convert to double
    return 5;  // OK: int can convert to double
}

void testConditions() {
    int i;
    int arr[5];
    struct S s;
    
    // Test case #1: IF condition must be scalar
    // if(s) { i = 1; }  // Error: struct in condition
    
    // Test case #33: Arrays cannot be used in conditions
    // if(arr) { i = 2; }  // Error: array in condition
    
    // Test case #2: WHILE condition must be scalar
    // while(s) { i = 3; }  // Error: struct in condition
}

void testReturns() {
    struct S s;
    int arr[5];
    
    // Test case #3: RETURN expression must be scalar
    // return s;  // Error: struct in return
    // return arr;  // Error: array in return
}

void testAssignments() {
    int i;
    int j;
    int arr[5];
    struct S s;
    
    // Test case #7: Assignment destination must be left-value
    // 5 = i;  // Error: 5 is not a left-value
    
    // Test case #8: Assignment destination cannot be constant
    // (i + 5) = 10;  // Error: expression is not modifiable
    
    // Test case #9: Assignment destination must be scalar
    // arr = i;  // Error: array as assignment destination
    // s = i;    // Error: struct as assignment destination
    
    // Test case #10: Assignment source must be scalar
    // i = arr;  // Error: array as assignment source
    // i = s;    // Error: struct as assignment source
    
    // Test case #11: Assignment source must be convertible to destination type
    // i = "hello";  // Error: string cannot convert to int
}

void testLogicalOps() {
    int i;
    int j;
    struct S s;
    int arr[5];

    i = i || j; 
    i = i || 6;
    
    // Test case #12: || requires scalar operands
    // i = s || j;  // Error: struct in logical OR
    // i = arr || j;  // Error: array in logical OR
    
    // Test case #13: && requires scalar operands
    // i = s && j;  // Error: struct in logical AND
    // i = arr && j;  // Error: array in logical AND
    
    // Test case #14: == and != require scalar operands
    // i = (s == s);  // Error: struct in equality
    // i = (arr != arr);  // Error: array in equality
    
    // Test case #15: <, <=, >, >= require scalar operands 
    // i = (s < s);  // Error: struct in relational
    // i = (arr > arr);  // Error: array in relational
}

void testArithmeticOps() {
    int i;
    struct S s;
    int arr[5];
    
    i = i && i; // OK: logical AND with scalars
    i = i && 5;

    i = i+6;
    i=arr[2]+2;
    // Test case #16: + and - require scalar operands
    // i = s + i;  // Error: struct in addition
    // i = arr - i;  // Error: array in subtraction
    
    // Test case #34: Arrays cannot be used directly in arithmetic
    // i = arr + arr;  // Error: array in arithmetic
    
    // Test case #17: * and / require scalar operands
    // i = s * i;  // Error: struct in multiplication
    // i = i / arr;  // Error: array in division
}

void testCasts() {
    int i;
    struct S s;
    int arr[5];
    struct S s2;

    i = (int)i;
    i = (int)5;
    
    // Test case #18: Cast cannot convert to struct
    // s = (struct S)i;  // Error: cannot cast to struct
    
    // Test case #19: Cast cannot convert struct
    // i = (int)s;  // Error: cannot cast struct
    
    // Test case #20: Array can only be converted to another array
    // i = (int)arr;  // Error: array to scalar conversion
    
    // Test case #21: Scalar can only be converted to another scalar
    // arr = (int[5])i;  // Error: scalar to array conversion
}

void testUnary() {
    int i;
    struct S s;
    int arr[5];
    
    // Test case #22: Unary - or ! must have scalar operand
    // i = -s;  // Error: unary - on struct
    // i = !arr;  // Error: unary ! on array
}

void testArrayOps() {
    int i;
    struct S s;
    
    // Test case #23: Only arrays can be indexed
    // i = i[0];  // Error: indexing non-array
    // i = s[0];  // Error: indexing non-array
    
    // Test case #24: Array index must be convertible to int
    // i = globalArray[s];  // Error: struct as index
}

void testStructOps() {
    int i;
    
    // Test case #25: Field selection requires struct
    // i.x = 5;  // Error: field select on non-struct
    
    // Test case #26: Field must exist in struct
    // globalStruct.z = 10;  // Error: no field z in struct S
}

void testFunctionCalls() {

    testStructOps();
    convFn(5);
    
    // Test case #27: ID must exist in symbol table
    // nonExistentFn();  // Error: undefined id
    
    // Test case #28: Only functions can be called
    // globalArray();  // Error: only functions can be called
    
    // Test case #29: Functions can only be called
    // int x = voidFn;  // Error: function can only be called
    
    // Test case #30: Function calls must have correct number of arguments (not too many)
    // voidFn(1, 2);  // Error: too many arguments
    
    // Test case #31: Function calls must have correct number of arguments (not too few)
    // convFn();  // Error: too few arguments
    
    // Test case #32: Function argument types must be convertible to parameter types
    // convFn(globalStruct);  // Error: struct not convertible to int
}

int main() {
    return 0;
}