int x;
char y;
double z;
double p[100];

struct S1{
	int i;
	double d[2];
	char x;
	};
struct S1 p1;
struct S1 vp[10];


struct S2 {
    struct S1 s1member;
    int data;
};

struct S2 s2var;

double sum(double x[5],int n){
	double r;
	int i;
	r=0;
	i=0;
	while(i<n){
		double r;
		n=x[i];
		r=r+n;
		i=i+1;
		}
	{ // test pentru block nested
		int blockVar;
        blockVar = 100;
        {
            int deeperVar;
            deeperVar = blockVar + i;
        }
	}

	return r;
	}
	
void f(struct S1 p){
	puti(p.i);
	}

void testVoid() {
    // Empty function
}

int testParams(int a, char b, double c, struct S1 d) {
    return a;
}

void test(){
	int x;
	{
		int y;
	}
	int y;
}