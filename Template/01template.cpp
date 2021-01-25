#include<iostream>
using namespace std;

//template

template<typename T>

void aa(T& a, T& b)
{
    T temp = a;
    a = b;
    b = temp;
}

void test01()
{
    int a = 10;
    int b = 20;
    aa<int>(a, b);

    cout<<"a:"<<a<<endl;
    cout<<"b:"<<b<<endl;
}

int main()
{
    test01();

    system("pause");
    return 0;
}