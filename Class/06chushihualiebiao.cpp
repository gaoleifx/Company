#include<bits/stdc++.h>
using namespace std;

//初始化列表

class preson
{
public:
    int age;
    int height;
    int id;

    preson(int a, int b, int c):age(a), height(b), id(c)//初始化列表书写格式
    {

    }
};

void test01()
{
    preson p1(30, 20, 26);
    cout<<p1.age<<endl;
    cout<<p1.height<<endl;
    cout<<p1.id<<endl;
}

int main()
{
    test01();

    system("pause");
    return 0;
}