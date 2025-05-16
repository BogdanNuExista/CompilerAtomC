struct S{
	int n;
	char text[16];
	};
	
struct S a;
struct S v[10];

void f(char text[],int i,char ch){
	text[i]=ch;
	}

int h(int x,int y){
	if(x>0&&x<y){
		f(v[x].text,y,'#');
		return 1;
		}
	return 0;
	}

struct Point {
    int x;
    int y;
};

void testErrors() {
    int a;
    double b;
    struct Point p;
    
    // Error: struct in condition
    if(p) {
        a = 5;
    }
    
    // Error: array as scalar in assignment

    //int arr[10];
    //a = arr;
    
    // Error: returning value from void function

    //return 5;
}

int testReturn() {
    // Error: missing return value
}

void testReturnVoid() {
    // Error: void function returning value

    //return 10;
}

int main() {
    int x;
    double y;
    
    // Error: incompatible assignment
    // x = "hello";
    
    // Error: struct used in arithmetic
    struct Point p;
    //x = p + 5;
    
    // Error: invalid indexing
    //x[5] = 10;
    
    return 0;
}