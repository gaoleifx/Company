#include<iostream>
using namespace std;

//overloading

class Box
{
public:
    void setLenth(double a)
    {
        length = a;
    }
    void setWidth(double b)
    {
        width = b;
    }

    void setHeight(double c)
    {
        height = c;
    }

    double getvolume()
    {
        return length * width * height;
    }

    Box operator+( const Box& a)
    {
        Box box;
        box.length = this->length + a.length;
        box.width = this->width + a.width;
        box.height = this->height + a.height;
        return box;
        //return box.getvolume();
    }

    //Box operator>( const Box& b);
       
protected:
    double length;
    double width;
    double height;   
};

bool operator>(Box& box1, Box& box2)
{
    if(box1.getvolume() > box2.getvolume())
    {
        return true;
    }
    return false;
}

void test01()
{
    Box box1;
    box1.setLenth(10);
    box1.setWidth(12);
    box1.setHeight(10);
    box1.getvolume();

    Box box2;
    box2.setLenth(10);
    box2.setWidth(12);
    box2.setHeight(10);
    box2.getvolume();

    Box box3;
    box3 = box1 + box2;
    box3.getvolume();

    cout<<"box3's volume is:"<<box3.getvolume()<<endl;

    if(box1.getvolume() > box2.getvolume())
    {
        cout<<"box1's volume is bigger"<<endl;
    }

    cout<<"box2's volume is bigger"<<endl;

}

int main()
{
    test01();
    system("pause");
}