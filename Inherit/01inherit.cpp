#include<iostream>
using namespace std;

//继承

class father
{
public:
    int pre_a = 1;
protected:
    int pre_b = 2;
private:
    int pre_c = 3;
};

class son:public father
{
public:
    int son_a = 4;
};

void test01()
{
    son son01;
    cout<<son01.pre_a<<endl;
    cout<<son01.son_a<<endl;
}

int main()
{
    test01();

    system("pause");
}