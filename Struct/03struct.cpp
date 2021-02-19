#include<iostream>

using namespace std;

struct
{
    char a;
    short b;
    int c;
}box1;

struct 
{
    char a;
    short b;
    int c;
}box2;


int main()
{
    printf("%ld\n", sizeof(box1));

    typeof(box1) box1_01;//应该等于struct box1 box1_01
    printf("%ld\n", sizeof(box1_01));

    typeof(box1) *prt1 = &box1_01;
    prt1->b = 444;
    printf("%d\n", prt1->b);

    system("pause");
}