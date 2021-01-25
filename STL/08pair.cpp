#include<iostream>
using namespace std;

void test01()
{
    pair<string, int>p1 = make_pair("xiaoming", 20);
    cout<<p1.first<<endl;
    cout<<p1.second<<endl;
}

int main()
{
    test01();
    system("pause");
}