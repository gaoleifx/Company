#include<iostream>
#include<vector>
using namespace std;

//容器嵌套容器

class father
{
public:
    virtual father(string &name)
    {
        this->father_name = name;
    }

    string father_name;
};

class child:public father
{
    virtual child(string &name)
    {
        this->son_name = name;
    }
    
    string son_name;
};

void test01()
{
    vector<vector<father>> v;
}