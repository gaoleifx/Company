#include<iostream>
#include<string.h>
#include<algorithm>
#include<stdio.h>

using namespace std;

void test01()
{
    string st1;
    string str2("12345678956447895325647125689554123");
    string str3(str2, 3);

    cout<<"str2:"<<str2<<endl;
    cout<<"str3:"<<str3<<"\n"<<"size:"<<str3.length()<<endl;
    cout<<"max_size:"<<str3.capacity()<<endl;

}


void test02()
{
    string aa1("abcd");
    string aa2("ABecd");

    cout<<aa1.compare(2, 3, aa2, 2, 3)<<endl;//return -1(aa1's cd compare aa2's ec)
    cout<<aa1.compare(2, 3, aa2, 3, 4)<<endl;//return 0(aa1's cd compare aa2's cd)
}

void test03()
{
    string s1("abc");
    string s2("def");
    s1.append(s2);
    cout<<s1<<endl;//return abcdef
    string s3 = s1 + s2;
    cout<<s3<<endl;//return abcdefdef
    string s4 = "ABC";
    s4 += s3.c_str();
    cout<<s4<<endl;//return ABCabcdefdef
}

//遍历
void test04()
{
    string s1 = "abcdef456fe";
    for(int i = 0; i<s1.size(); i++)
    {
        cout<<s1[i]<<" ";
    }
    cout<<endl;

    string::iterator its;
    for(its = s1.begin(); its < s1.end(); its++)
    {
        cout<<*its<<" ";//remind *its,因为迭代器是指针
    }
    cout<<endl;

    //反向遍历
    string::reverse_iterator ita;
    for(ita = s1.rbegin(); ita != s1.rend(); ita++)
    {
        cout<<*ita<<" ";
    }
    cout<<endl;
}

//删除 erase()
void test05()
{
    string s1 = "sfefsfegeisle123";
    s1.erase(s1.begin()+1, s1.end()-3);
    string::iterator iter = s1.begin();
    while( iter != s1.end() )
    {
        cout<<*iter;
        *iter++;
    }
    cout<<endl;
}

//replace
void test06()
{
    string s1("hello world!");
    s1.replace(s1.size()-1, 1, 2, '.');////将当前字符串从pos索引开始的n个字符，替换成n1个字符c
    cout<<s1<<endl;
    s1.replace(6, 10, "gril");
    cout<<s1<<endl;
    s1.replace(s1.begin(), s1.begin()+5, "boy");
    cout<<s1<<endl;
}

//通过STL的transform算法配合的toupper和tolower来实现该功能
void test07()
{
    string str1("ABCDEFG");
    string result;

    //transform(str1.begin(), str1.end(), ::tolower);
    //cout<<str1<<endl;
}

//find
void test08()
{
    string str1 = "ABCDEFG";

    cout<<str1.find("B", 2)<<endl;
    
}
//sort
void test09()
{
    string s = "sefsba";
    sort(s.begin(), s.end());
    cout<<s<<endl;
}

//string的分割/截取字符串：strtok() & substr()
void test10()
{
    char str[16] = "abc,def;e!s";
    char *split = ",;!";
    char *p2 = strtok(str, split);
    //printf("%s\n", p2);
    while( p2 != NULL)
    {
        cout<<p2<<endl;
        p2 = strtok(NULL, split);
    }
}

int main()
{
    test10();
    system("pause");
}