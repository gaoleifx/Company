#include<bits/stdc++.h>
using namespace std;

//运算符重载-加号运算符重载

class preson
{
public:
    preson operator+(preson &p)//加号运算符重载
    {
        preson temp;
        temp.pre_a = this->pre_a + p.pre_a;
        temp.pre_b= this->pre_b + pre_b;
    }
    int pre_a;
    int pre_b;
};

void test01()
{
    preson pr01;
    pr01.pre_a = 10;
    pr01.pre_b = 20;
    preson pr02;
    pr02.pre_a = 10;
    pr02.pre_b = 20;

    preson pr03 = pr01 + pr02;
    cout<<pr03.pre_a<<endl;
    cout<<pr03.pre_b<<endl;
}

int main()
{
    test01();

    system("pause");
    return 0;
}

