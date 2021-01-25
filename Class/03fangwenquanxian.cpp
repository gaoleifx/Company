#include<bits/stdc++.h>
#include<iostream>
using namespace std;

//访问权限

class fun
{
public:
    string house;
protected:
    string name;
private:
    string password;

public:
    void info()
    {
        //house = "zhangsan";
        name = "xiaoming";
        password = "123456";

        cout<<house <<name <<password <<endl;
    }
};

int main()
{
    fun pre1;
    pre1.house = "wangwu";
    //pre1.name = "uuu";
    pre1.info();

    system("pause");
    return 0;
}