#include<iostream>
#include<vector>
using namespace std;

//容器嵌套

void test01()
{
    vector<vector<int>> v;

    //创建小容器
    vector<int>v1;
    vector<int>v2;
    vector<int>v3;
    vector<int>v4;
    vector<int>v5;

    for(int i=0; i<5; i++)
    {
        v1.push_back(i+1);
        v2.push_back(i+2);
        v3.push_back(i+3);
        v4.push_back(i+4);
        v5.push_back(i+5);
    }

    //将小容器放入大容器
    v.push_back(v1);
    v.push_back(v2);
    v.push_back(v3);
    v.push_back(v4);
    v.push_back(v5);

    //遍历容器
    for(vector<vector<int>>::iterator it = v.begin(); it != v.end(); it++)
    {
        //*it现在还是一个容器，需要在遍历一次
        for(vector<int>::iterator vit = (*it).begin(); vit != (*it).end(); vit++)
        {
            cout<<*vit<<" ";
        }
        cout<<endl;
    }
}

int main()
{
    test01();

    system("pause");
}