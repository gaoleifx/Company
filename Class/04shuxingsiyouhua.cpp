#include<iostream>
#include<bits/stdc++.h>
using namespace std;

//成员属性私有化

class student
{
private:
    string name = "zhangsan";
    int age = 18;
    string lover = "xiaosi";

public:
    //设置名字可读可写
    void setName(string inName)
    {
        name = inName;

    }

    string disName()
    {
        return name;
    }
    //设置年龄只读
    int setAge()
    {   
        age = 20;
        return age;
    }
    //设置情人只写
    void disLover(string inLover)
    {
        lover = inLover;
    }
public:
    void info()
    {
        cout<<name<<age<<lover<<endl;
    }
};

int main()
{
    student st1;
    st1.setName("xiaoming");
    st1.setAge();
    st1.info();
    
    system("pause");

}