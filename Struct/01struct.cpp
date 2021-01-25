#include<iostream>
using namespace std;

struct preson
{
    int year;
    int age;
    string name;
}p1 = {1920, 20, "zhangsan"};

struct box
{
    float length;
    float width;
    float height;
}box[2] = {{10, 20, 30}, {50, 60, 70}};

struct preson p2 = {1860, 60, "lisi"};

int main()
{
    cout<<p1.age<<endl;
    cout<<box[0].length<<endl;
    cout<<p2.year<<endl;
    system("pause");
}