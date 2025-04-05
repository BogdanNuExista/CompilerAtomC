struct Pt {
    int x;
    int y;
};
struct Pt points[10];

double max(double a, double b) {
    if (a > b) return a;
    else return b;
}

int len(char s[]) {
    int i;
    i = 0;
    while (s[i]) i = i + 1;
    return i;
}

void testStruct() {
    struct Pt p;
    p.x = 5;
    p.y = 10;
    points[0] = p;
    points[1].x = 15;
    points[1].y = 20;
}

void testArray() {
    int arr[4];
    arr[0] = 1;
    arr[1] = arr[0] + 2;
    arr[2] = arr[1] * 3;
    arr[3] = arr[2] / 4;
}

void main() {
    int i;
    i = 10;
    while (i != 0) {
        puti(i);
        i = i / 2;
    }

    testStruct();
    testArray();
    testExpressions();
    testFunctionCalls();
    testNestedLoops();
}