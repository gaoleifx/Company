#include<iostream>
using namespace std;

class preson
{
public:
    preson();
    preson(double, string);
    preson(int, int, string);
    preson(preson &p);//拷贝构造函数
    ~preson()
    {
        cout<<"shwo xigou function!"<<endl;
    }
    void show();
private:
    int age;
    int year;
    string name;
};

preson::preson(int a, int b, string s):age(a), year(b), name(s)
{

}

preson::preson()
{
    age = 20;
    year = 1990;
    name = "lisi";
}

preson::preson(double a, string s)
{
    year = a;
    name = s;
}

preson::preson(preson &p)
{
    age = p.age;
    year = p.year;
    name = p.name;
}

void preson::show()
{
    cout<<"age="<<age<<endl;
    cout<<"name="<<name<<endl;
    cout<<"birth="<<year<<endl;
}

void test01()
{
    preson p(12, 1960, "xiaoming");
    p.show();
    preson p2;
    p2.show();
    preson p3(p2);
    p3.show();
}

int main()
{
    test01();
    system("pause");
}

