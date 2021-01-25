#include<iostream>
using namespace std;

class Box
{
public:
    Box(){};
    Box(double a)
    {
        length = new double(a);

    }

    ~Box()
    {
        if(length != NULL)
        {
            delete length;
            length = NULL;

        }
        //if()
    }
public:
    double *length;
};

void test01()
{
    Box box1(10);
    Box box2;
    box2 = box1;
    cout<<*box2.length<<endl;
}

int main()
{
    test01();
    system("pause");
}