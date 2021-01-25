#include<iostream>
using namespace std;

//多态
//动态多态满足条件
//1.有继承关系
//2.子类重写父类得虚函数

//动态多态使用
//父类的指针或引用， 执行子类对象

class animal
{
public:
    virtual void speak()
    {
        cout<<"animal is speaking"<<endl;
    }
};

class cat:public animal
{
public:
    void speak()
    {
        cout<<"cat is speaking"<<endl;
    }
};

class dog:public animal
{
public:
    void speak()
    {
        cout<<"dog is speaking"<<endl;
    }
};

void doSpeak(animal &p)
{
    p.speak();

}


int main()
{
    cat ca01;
    doSpeak(ca01);

    dog do01;
    doSpeak(do01);

    system("pause");
    return 0;
}