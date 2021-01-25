#include<bits/stdc++.h>
using namespace std;

class preson
{
public:
    preson(int age)
    {
        //解决名字冲突
        this->age = age;
    }
    int age;

    void info()
    {
        cout<<age<<endl;
    }

};

void test01()
{
    preson p1(20);
    //cout<<p1.m_age<<endl;
    p1.info();
    cout<<"打印中文"<<endl;
}

int main()
{
    test01();

    system("pause");
    return 0;
}