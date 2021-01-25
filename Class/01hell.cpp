#include<bits/stdc++.h>
using namespace std;

//类的基本书写方式

float pi = 3.1415926;

class caPar
{
public:

    float radius;

    float test()
    {
        return 2 * pi * radius;
    }
};

int main()
{
    caPar s1;
    s1.radius = 10;

    cout<<s1.test()<<endl;
    system("pause");
}