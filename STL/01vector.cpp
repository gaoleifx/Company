#include<iostream>
#include<vector>
using namespace std;
#include<algorithm>

//vector迭代器

void mypravl(int aa)
{
    cout<<aa<<endl;
}

void test01()
{
    vector<int> v;

    v.push_back(10);
    v.push_back(20);
    v.push_back(30);
    v.push_back(40);

    //第二种遍历方式
    // for (vector<int>::iterator it = v.begin(); it != v.end(); it++)
    // {
    //     cout<<*it<<endl;
    // }

    //第三种遍历方式
    for_each( v.begin(), v.end(), mypravl);
}

int main()
{
    test01();

    system("pause");
}

