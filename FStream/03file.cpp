#include<iostream>
#include<io.h>
#include<vector>
#include<stdio.h>
#include<cstring>


using namespace std;

void getFiles(const string path, vector<string> &pathList,  bool isFolder)
{
    struct _finddata_t fileInfo;
    string newpath = path + "\\*";
    string to_search = path + "\\*.fbx";

    long handle = _findfirst(to_search.c_str(), &fileInfo);
    //cout<<fileInfo.name<<endl;

    if(handle == -1)
    {
        cout<<"the path is aviliable!"<<endl;
        exit(-1);
    }
    do
    {
        if(fileInfo.attrib & _A_SUBDIR)
        {
            if(strcmp(fileInfo.name, ".") != 0 && strcmp(fileInfo.name, "..") != 0)
            {
                string currentPath = path + "\\" + fileInfo.name;
                getFiles(currentPath, pathList, isFolder);
            }
            
        }
        else
            {
                //cout<<fileInfo.attrib<<endl;
                pathList.push_back(fileInfo.name);
            }
    }while(_findnext(handle, &fileInfo) == 0);

    _findclose(handle);
    
}


int main()
{
    string path = "E:\\Wood_Oak##02";
    vector<string> fileList;
    getFiles(path, fileList, true);

    vector<string>::iterator ita;
    for(ita = fileList.begin(); ita != fileList.end(); ita++)
    {
        cout<<*ita<<endl;
    }
    system("pause");
}