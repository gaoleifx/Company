#include<iostream>
using namespace std;
#include<set>

class myCompar
{
public:
    bool operator()(int a, int b)
    {
       return a > b;
    }
};

void test01()
{
    set<int, myCompar>s1;
    for(int i = 0; i<10; i++)
    {
        s1.insert(10+i);
    }

    for(set<int, myCompar>:: iterator it = s1.begin(); it != s1.end(); it++)
    {
        cout<<*it<<" ";
    }

    cout<<endl;

}


int main()
{
    test01();
    system("pause");

}