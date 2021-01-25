#include<iostream>
using namespace std;

class preson
{
public:
    preson()
    {
        cout<<"preson's gouzao function!"<<endl;
    }
    ~preson()
    {
        cout<<"preson's xigou function~"<<endl;
    }
    virtual void doSpeak(){};

};

class adult:public preson
{
public:
    adult()
    {
        cout<<"adult's gouzhao function!"<<endl;
    }
    ~adult()
    {
        cout<<"adult's xigou function!"<<endl;
    }
    virtual void doSpeak()
    {
        cout<<"adult is speaking!"<<endl;
    }
};

class child:public preson
{
public:
    child()
    {
        cout<<"child's gouzao function!"<<endl;
    }
    ~child()
    {
        cout<<"child's xigou function!"<<endl;
    }
    virtual void doSpeak()
    {
        cout<<"child is speaking!"<<endl;
    }
};

void test01()
{
    preson *pr = new adult();
    pr->doSpeak();
    delete pr;
    pr = new child();
    pr->doSpeak();
    delete pr;
}

int main()
{
    test01();

    system("pause");
}