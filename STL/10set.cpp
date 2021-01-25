#include<iostream>
#include<set>
using namespace std;

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

//创建一个仿函数
class comparePreson
{
public:
    bool operator()(const preson& pr1, const preson& pr2)
    {
        return pr1.pr_age>pr2.pr_age;
    }
};

void test01()
{
    set<preson, comparePreson> s;

    preson pr01("zhangfei", 20);
    preson pr02("guanyu", 29);
    preson pr03("liubei", 25);
    preson pr04("zhaoyun", 30);

    s.insert(pr01);
    s.insert(pr02);
    s.insert(pr03);
    s.insert(pr04);

    for(set<preson, comparePreson>::iterator it = s.begin(); it != s.end(); it++)
    {
        cout<<"the name is:"<<it->pr_name<<"the age is:"<<it->pr_age<<endl;
    }
}

int main()
{
    test01();

    system("pause");
}