#include<bits/stdc++.h>
using namespace std;
#include<string>

//函数分类及调用

class preson
{
public:
    preson()
    {
        cout<<"无参构造函数"<<endl;
    }

    preson(int a)
    {
        age = a;
        cout<<"有参构造函数"<<endl;
    }

    preson(const preson &p)
    {
        age = p.age;
        cout<<"拷贝构造函数"<<endl;
    }

    ~preson()
    {
        cout<<"析构函数"<<endl;
    }
    int age;

};

void test01()
{
    //括号法
    preson p1;//无参构造调用
    preson p2(10);//有参构造调用
    preson p3(p2);//拷贝构造调用
    cout<<p2.age<<endl;
    //显示法
    preson p11;
    preson p12 = preson(20);
    preson p13 = preson(p12);
    cout<<p12.age<<endl;
    //隐式转换法
    preson p14 = 10;//相当于写了preson p4 = preson(10);有参构造
    preson p15 = p14;//拷贝构造
}

int main()
{
    test01();

    system("pause");
    return 0;
}