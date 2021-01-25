#include<iostream>
using namespace std;

class shape
{
    public:
        void setLenght(int L)
        {
            length = L;
        }
        void setWidth(int W)
        {
            width = W;
        }
        void setHeight(int H)
        {
            height = H;
        }

    protected:
        int length;
        int width;
        int height;
};

class box:public shape
{
    public:
        int getVolume()
        {
            return length * width * height;
        }
};

int test01()
{
    box box1;
    box1.setLenght(10);
    box1.setWidth(10);
    box1.setHeight(10);

    int a = box1.getVolume();
    cout<<a<<endl;
}

int main()
{
    test01();

    system("pause");
}