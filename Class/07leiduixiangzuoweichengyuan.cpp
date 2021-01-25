#include<bits/stdc++.h>
using namespace std;
#include<string>

//类对象作为成员

class phone
{
public:
    string p_logo;
    //string p_user;

    phone(string a)
    {
        p_logo = a;
        cout<<"phone的构造函数"<<endl;
    }

};

class preson
{
public:
    string m_name;
    phone m_phone;//类对象作为成员

    preson(string a, string b):m_name(a), m_phone(b)
    {
        cout<<"preson的构造函数"<<endl;
    }

    void playGame()
    {
        cout<<m_name<<"使用"<<m_phone.p_logo<<"牌手机"<<endl;
    }
};

void test01()
{
    preson p("zhangsan", "pingguo");
    p.playGame();

}

int main()
{
    test01();

    system("pause");
    return 0;
}