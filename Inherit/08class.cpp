#include<iostream>
using namespace std;

class base
{
public:
    int n;
};

class D1:virtual public base
{
public:
    int x;
};

class D2:virtual public base
{
public:
    int y;
};

class S:public D1, public D2
{
public:
    int z;
};

int main()
{
    S s1;
    D1 aa;
    aa.n = 20;
    cout<<s1.D1::n<<endl;
    system("pause");
}