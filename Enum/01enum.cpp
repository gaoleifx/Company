#include<iostream>
using namespace std;

extern int a, b, c;

int main()
{
    //指针
    double *p;
    double arr[10] = {10, 12, 5, 8, 65, 9};
    p = arr;
    cout<<*p<<endl;
    cout<<*(p+3)<<endl;

    //枚举
    enum course {math, chinese=1, english=2, physics, chemistry}c;
    c = physics;

    cout<<c<<endl;
    system("pause");
}