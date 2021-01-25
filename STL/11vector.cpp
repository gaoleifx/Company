#include<iostream>
#include<vector>
using namespace std;

void test01()
{
    vector<int> c;
    char *p[] = {"a", "b", "c", "d", "e"};
    int len = sizeof(p)/sizeof(p[0]);
    for(int i = 0; i<=len; i++)
    {
        c.push_back(10+i);
    }
    
    for(int i = 0; i<len; i++)
    {
        cout<<c[i]<<endl;
        cout<<p[i]<<endl;
    }

    //c.pop_back();
    cout<<c[2]<<endl;
    
}

int main()
{
    test01();
    system("pause");
}