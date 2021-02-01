#include<iostream>

using namespace std;

template<typename T>
void myswap(T &a, T &b)
{
    T temp = a;
    a = b;
    b = temp;
}

int main()
{
    float a = 10.2;
    float b  = 20.6;
    myswap(a, b);
    cout<<a<<endl;
    cout<<b<<endl;

    system("pause");
}