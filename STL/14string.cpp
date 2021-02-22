#include<iostream>
#include<string>

using namespace std;

void test01()
{
    string s1;
    cout<<"s1="<<s1<<endl;

    const char* str = "hello world";
    string s2(str);
    cout<<"s2="<<s2<<endl;

    string s3(s2);
    cout<<"s3="<<s3<<endl;

    string s4(10, 'a');
    s4.assign("cc c++", 5);
    cout<<"s4="<<s4<<endl;
}

void test02()
{
    //字符串添加
    string str1 = "hello world";
    str1 += " is good";
    string str2 = "c++ STL";
    str1 += str2;
    str1.append("the world is good");
    cout<<str1<<endl;
    //////字符串查找与替换
    int pos = str1.rfind("STL");
    str1.replace(0, pos, "aaaa");
    cout<<str1<<endl;
    /////字符串比较
    int ret = str1.compare("a");
    cout<<ret<<endl;

    ///字符串获取
    for(int i=0; i<str1.size(); i++)
    {
        cout<<str1[i];
    }
    cout<<endl;
    for(int i=0; i<str1.size(); i++)
    {
        cout<<str1.at(i);
    }
    cout<<endl;

    /////字符串插入和删除
    str1.insert(3,"WO");
    cout<<str1<<endl;
    str1.erase(3, 6);
    cout<<str1<<endl;
    /////string子串
    string str = "gaoleifx@hotmail.com";
    int pos1 = str.find("@");
    string  substr = str.substr(0, pos1);
    cout<<substr<<endl;


}


int main()
{
    test02();
    system("pause");
}