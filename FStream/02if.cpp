#include<iostream>
//包含头文件
#include<fstream>
#include<string>
using namespace std;

void test01()
{
    //创建流对象
    ifstream ifs;

    //指定打开文件
    ifs.open("C:\\Workspace\\test.txt", ios::in);
    if (!ifs.is_open())
    {
        cout<<"file open error"<<endl;
    }

    //1.读文件
    // char buf[1024] = {0};
    // while( ifs.getline(buf, sizeof(buf)))
    // {
    //     cout<<buf<<endl;
    // }
    //2.读文件
    char buf[1024] = {0};
    while( ifs >> buf)
    {
        cout<<buf<<endl;
    }
    //3.读文件
    // string buf;
    // while( getline(ifs, buf))
    // {
    //     cout<<buf<<endl;
    // }
    //关闭文件
    ifs.close();

}

int main()
{
    test01();

    system("pause");
    return 0;
}