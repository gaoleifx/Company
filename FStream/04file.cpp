#include<iostream>
#include<string>

using namespace std;

int main()
{
    string path = "abacsef";
    path = path.replace(path.begin(), path.begin()+5, "E");
    cout<<path<<endl;

    system("pause");
}