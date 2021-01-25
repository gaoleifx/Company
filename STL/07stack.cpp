#include<iostream>
#include<stack>
using namespace std;

//stack容器

void test01()
{
    stack<int> st;
    for( int i = 0; i<10; i++)
    {
        st.push(i+10);
    }

    cout<<st.size()<<endl;

    while(!st.empty())
    {
        cout<<st.top()<<endl;
        st.pop();
    }
}

int main()
{
    test01();
    system("pause");
}