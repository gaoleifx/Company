#include<bits/stdc++.h>
using namespace std;

//左移运算符重载

class preson
{
public:
    //不能利用成员函数重载左移运算符
    
    preson()
    {
        pre_a = 10;
        pre_b = 10;
    }

    int pre_a;
    int pre_b;
};

//只能利用全局函数重载左移运算符
ostream& operator<<(ostream &cout, preson &p)
{
    cout<<"pre_a:"<<p.pre_a<<"pre_b:"<<p.pre_b;
    return cout;
}

void test01()
{
    preson pr01;
    cout<<pr01<<endl;
}

int main()
{
    test01();

    system("pause");
    return 0;
}