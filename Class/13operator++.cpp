#include<bits/stdc++.h>
using namespace std;
//#include<directive>
//operator++重载

class preson
{
    //friend ostream &operator<<(ostream &cout, preson &p);
    friend int test01();
public:
    preson()
    {
        pre_age = 10;
    }

    preson& operator++()
    {
        pre_age++;
        return *this;
    }

    preson operator++(int)
    {   
        preson temp = *this;
        pre_age++;
        return temp;
    }

private:
    int pre_age;
};

// ostream &operator<<(ostream &cout, preson &p)
// {
//     cout<<p.pre_age;
// }


int test01()
{
    preson pr01;
    cout<<pr01.pre_age<<endl;
    ++pr01;
    //pr01++;
    cout<<pr01.pre_age<<endl;
}

// void test02()
// {
//     preson pr02;
//     cout<<pr02++ << endl;
//     cout<<pr02 << endl;
    
// }

int main()
{
    test01();
    //test02();

    system("pause");
    return 0;
}