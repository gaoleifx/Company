#include<iostream>
using namespace std;

//重载
class shape
{   
    //friend
    public:
        void setLenght(int a)
        {
            length = a;
        }
        void setWidth(int a)
        {
            width = a;
        }
        void setHeight(int a)
        {
            height = a;
        }
        
        double setVolume()
        {
            return length * width * height;
        }

        shape operator+(const shape& b)
        {
            shape box;
            box.length = this->length + b.length;
            box.width = this->width + b.width;
            box.height = this->height + b.height;

            return box;
        }

    protected:
        int length;
        int width;
        int height;
};

void test01()
{
    double volume = 0;

    shape box1;
    box1.setLenght(10);
    box1.setWidth(10);
    box1.setHeight(10);
    //box1.setVoleme();

    shape box2;
    box2.setLenght(10);
    box2.setWidth(10);
    box2.setHeight(10);
    //box2.setVoleme();

    shape box3;
    box3 = box1 + box2;

    cout<<box1.setVolume()<<endl;
    cout<<box2.setVolume()<<endl;
    cout<<box3.setVolume()<<endl;


    
}

int main()
{
    test01();

    system("pause");
}