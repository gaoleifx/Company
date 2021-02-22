#include<iostream>
#include<vector>

using namespace std;

void printVector(vector<int>& v)
{
    vector<int>::iterator it;
    for(it = v.begin(); it<v.end(); it++)
    {
        cout<<*it<<" ";
    }
    cout<<endl;
}

void test01()
{
    vector<int> v1;
    for( int i=0; i<20; i++)
    {
        v1.push_back(i);
    }
    printVector(v1);

    vector<int> v2(v1.begin(), v1.end());
    printVector(v2);

    vector<int> v3(10, 2);
    printVector(v3);

    vector<int> v4(v3);
    printVector(v4);
}


int main()
{
    test01();
    system("pause");
}