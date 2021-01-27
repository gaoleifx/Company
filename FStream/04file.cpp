#include<iostream>
#include<vector>

using namespace std;

int main()
{
    vector<string> files;
    string path = "E:/HoudiniProjects/newProject";
    files.push_back(path);
    string current_folder(files.back());
    //files.pop_back();
    cout<<current_folder<<endl;


    system("pause");
}