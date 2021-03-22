#include<iostream>
using namespace std;

class ani
{
public:
    ani(){cout<<"ani() called."<<endl;}
    virtual ~ani(){cout<<"~ani() called."<<endl;}
};

class dog:public ani
{
public:
    dog(){cout<<"dog() called."<<endl;}
    ~dog(){cout<<"~dog() called."<<endl;}
};

int main()
{
    dog d1;
    system("pause");
    return 0;
}