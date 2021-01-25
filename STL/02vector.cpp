#include<iostream>
#include<vector>
using namespace std;
#include<string>

class preson
{
public:
    preson(string name, int age)
    {
        this->pr_name = name;
        this->pr_age = age;
    }

    string pr_name;
    int pr_age;

};

void test01()
{
    vector<preson> v;

    preson pr01("zhangsan", 10);
    preson pr02("lisi"    , 10);
    preson pr03("wangwun" , 30);
    preson pr04("zhaoliu" , 60);
    preson pr05("caoqi"   , 90);


    v.push_back(pr01);
    v.push_back(pr02);
    v.push_back(pr03);
    v.push_back(pr04);
    v.push_back(pr05);
    
    for(vector<preson>::iterator it = v.begin(); it != v.end(); it++)
    {
       //cout<<(*it).pr_name<<(*it).pr_age<<endl;
       cout<<"the name is:"<<it->pr_name<<" the age is:"<<it->pr_age<<endl;
    }
}

int main()
{
    test01();

    system("pause");
}