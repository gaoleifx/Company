#include<bits/stdc++.h>
using namespace std;

//全局函数做友元

class bedroom
{
    friend void test01();
public:

    bedroom()
    {
        this->my_sittingRoom = "客厅";
        this->my_bedRoom = "卧室";
    }
public:
    string my_sittingRoom;
private:
    string my_bedRoom;
};

void test01()
{
    bedroom pr1;
    //pr1.my_sittingRoom = a;
    //pr1.my_bedRoom = b;
    cout<<"you are visting:"<<pr1.my_sittingRoom<<endl;
    cout<<"you are visting:"<<pr1.my_bedRoom<<endl;
}

int main()
{
    test01();

    system("pause");
    return 0;
}