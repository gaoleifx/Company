#include<bits/stdc++.h>
using namespace std;

//友元类
//类外实现成员函数=
class building;
class goodgay
{
public:
    goodgay();//类外实现构造函数

    void vist01();
    void vist02();

    building *bu01;
    
};

class building
{
    //friend class goodgay;//友元类
    friend void goodgay::vist01();
public:
    building();//类外实现构造函数
public:
    string bu_sittingroom;
private:
    string bu_bedroom;

};
//类外实现成员函数
building::building()
{
    bu_bedroom = "卧室";
    bu_sittingroom = "客厅";
}

goodgay::goodgay()
{
    bu01 = new building;
}

void goodgay::vist01()
{
    cout<<"vist01正在访问"<<bu01->bu_sittingroom<<endl;
    cout<<"vist01正在访问"<<bu01->bu_bedroom<<endl;
    //cout<<"hello world"<<endl;
}
void goodgay::vist02()
{
    cout<<"vist01正在访问"<<bu01->bu_sittingroom<<endl;
}

void test01()
{
    goodgay pr01;
    pr01.vist01();
    pr01.vist02();
}

int main()
{
    test01();

    system("pause");
    return 0;
}