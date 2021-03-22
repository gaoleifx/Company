#include<iostream>
using namespace std;

class A
{
public:
    virtual void func()=0;
};

class B:public A
{
public: 
    void func()
    {
        cout<<"B:func() is called!"<<endl;
    }
};

class C: public A, public B
{
public:
    void func()
    {
        cout<<"C:func() is called!"<<endl;
    }
};

int main()
{
    C c1;
    c1.B::func();
    system("pause");
}