#include<iostream>

using namespace std;

int main()
{
    int a = 10;
    int& b = a;

    cout<<a<<" "<<b<<endl;

    a = 20;
    cout<<a<<" "<<b<<endl;

    //常量引用可以引用常量也可以引用变量， 但常量必须用引用常量才能引用
    const int c1 = 30;
    const int& c2 = c1;
    const int& c3 = a;

    //引用常量会随着变量变而变
    a = 150;
    //int &c c3 = c1;

    cout<<c1<<" "<<c2<<" "<<c3<<endl;//30 30 150

    system("pause");
}