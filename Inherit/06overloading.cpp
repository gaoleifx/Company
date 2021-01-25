#include<iostream>
#include<vector>
using namespace std;

class box
{
public:

    void setInfo(int a, int b, int c)
    {
        length = a;
        width = b;
        height = c;
    }

    double setVolume()
    {
        return length * width * height;
    }

    box& operator=(const box& b)
    {
        //box box1;
        this->length = b.length;
        this->width = b.width;
        this->height = b.height;
        this->volume = b.volume;
        return *this;
    }

    bool operator==(const box& c)
    {
        if(this->length == c.length && this->width == c.width && this->height == c.height)
        {
            return true;
        }
        return false;
    }

    box& operator>(const box& d)
    {   
        //box box1;
        if(this->volume > d.volume)
        {
            return *this;
        }
      
    }

    box& operator<(const box& e)
    {
        //box box2;
        if(this->volume < e.volume)
        {
            return *this;
        }
    }

    friend void test01();

protected:
    double length;
    double width;
    double height;
    double volume = length * width * height;
};

// box::operator==(const box&)
// {

// }


void test01()
{
    box box1;
    box1.setInfo(10, 20, 30);
    box box2;
    box2 = box1;
    cout<<box2.length<<"\n"<<box2.width<<"\n"<<box2.height<<endl;
    if(box1 == box2)
    {
        cout<<"the same box!"<<endl;
    }
    else
    {
        cout<<"the different box!"<<endl;
    }

    double volume1 = box1.setVolume();
    double volume2 = box2.setVolume();

    box box3;
    box3 = box2 = box1;
    double volume3 = box3.setVolume();

    cout<<"box2's volume is:"<<volume2<<endl;
    cout<<"box3's volume is:"<<volume3<<endl;



    
}

int main()
{
    test01();
    system("pause");
}