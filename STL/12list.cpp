#include<iostream>
#include<vector>
#include<numeric>
#include<algorithm>
#include<list>
using namespace std;

typedef list<int> LISTINT;
typedef list<char> LISTHAR;

int main()
{
    //用LISTINT创建一个名为listone的list对象
    LISTINT listone;
    //声明i为迭代器
    LISTINT::iterator i;

    listone.push_front(2);
    listone.push_front(1);

    listone.push_back(3);
    listone.push_back(4);

    //从前向后显示listone中的数据
    cout<<"listone.begin()~~~~~~~~~listone.end():"<<endl;
    for(i = listone.begin(); i != listone.end(); i++)
    {
        cout<<*i<<" ";
    }
    cout<<endl;
    for(list<int>::iterator it = listone.begin(); it != listone.end(); it++)
    {
        cout<<*it<<" ";
    }
    cout<<endl;

    //声明ir为反向迭代器
    LISTINT::reverse_iterator ir;
    //从后向前显示listone中的数据
    cout<<"listone.rbegin()~~~~~~~~~~~~listone.rend()"<<endl;
    for( ir = listone.rbegin(); ir != listone.rend(); ir++)
    {
        cout<<*ir<<" ";
    }
    cout<<endl;

    //使用STL中的accumulate(累加)算法
    int result = accumulate(listone.begin(), listone.end(), 10);//10为累加的初始值
    cout<<"sum="<<result<<endl;
    cout<<"---------------------"<<endl;

    //定义一个listtwo容器
    LISTHAR listtwo;
    //声明j为迭代器
    LISTHAR::iterator j;

    listtwo.push_front('a');
    listtwo.push_front('b');

    listtwo.push_back('c');
    listtwo.push_back('d');

    //从前向后显示listTwo中的数据
    cout<<"lsittwo.begin()~~~~~~~~~~~listtwo.end():"<<endl;
    for( j = listtwo.begin(); j != listtwo.end(); j++)
    {
        cout<<char(*j)<<" ";
    }
    cout<<endl;
    
    //使用STL的max_element算法求listTwo中的最大元素并显示
    j=max_element(listtwo.begin(), listtwo.end());
    cout<<"The maximum element in listtwo is:"<<char(*j)<<endl;

    system("pause");
}