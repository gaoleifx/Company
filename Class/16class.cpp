#include<iostream>
using namespace std;

class shape
{
public:
    shape();
    shape(float a, float b, float c)
    {
        length = a;
        width = b;
        height = c;
    }
    void showinof()
    {   
        cout<<"aa->length"<<endl;
    }

private:
    float length;
    float width;
    float height;
};

shape::shape()
{
    length = 10;
    width = 20;
    height = 50;
}

void test01()
{
    shape s1(10, 20, 30);
    shape *s2;
    s2->showinof();

}


int main()
{
    test01();
    system("pause");
}