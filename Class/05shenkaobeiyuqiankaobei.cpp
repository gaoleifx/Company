#include<bits/stdc++.h>
using namespace std;

//深拷贝与浅拷贝

class preson
{
public:
    int age;
    int *height;

    preson()
    {
        cout << "无参构造函数" << endl;
    }

    preson(int a, int b)
    {
        age = a;
        height = new int(b);
        cout << "有参构造函数" << endl;
        //cout<<"年龄是："<<
    }
    //重载
    preson(const preson &p)
    {
        age = p.age;
        //height = p.height;
        height = new int(*p.height);
    }

    ~preson()
    {
        if (height != NULL)
        {
            delete height;
            height = NULL;
        }
        cout << "析构函数" << endl;
    }
};

void test01()
{
    preson s1(10, 160);
    preson s2(s1);
    cout << "年龄是：" << s1.age << endl;
    cout << "身高是：" << *s1.height << endl;
}

int main()
{
    test01();

    system("pause");
    return 0;
}