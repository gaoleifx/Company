#include<iostream>
#include<vector>
#include<io.h>
#include<cstring>
#include<fstream>

using namespace std;

void getFiles(const string folderPath, vector<string> &allPath, bool isFolder)
{
    string path;
    path = folderPath + "\\*";
    struct _finddata_t fileInfo;

    long handle = _findfirst(path.c_str(), &fileInfo);

    if(handle == -1)
    {
        //return 0;
        //cout<<"the folder path is empty!"<<endl;
    }
    do
    {
        if(fileInfo.attrib & _A_SUBDIR)
        {
           
            if(strcmp(fileInfo.name, ".") != 0 && strcmp(fileInfo.name, "..") != 0)
            {
                string current_path = folderPath + "\\" + fileInfo.name;
                getFiles(current_path, allPath, true );
                //string newPath = "\""+current_path+"\",";
                string newPath = current_path;
                allPath.push_back(newPath);
                //cout<<current_path<<endl;
            }

        }

    } while (_findnext(handle, &fileInfo) == 0);
    _findclose(handle);
}

int main()
{
    ofstream ofs;

    string path = "C:\\Program Files\\Side Effects Software\\Houdini 18.5.408\\toolkit\\include";
    string writeInPath = "E:\\HoudiniProjects\\newProject";
    ofs.open(writeInPath+"\\"+"test.txt", ios::out);

    vector<string> p1;
    getFiles(path, p1, true);
    vector<string>::iterator ita;
    for(ita = p1.begin(); ita != p1.end(); ita++)
    {
        ofs<<*ita<<endl;
        cout<<"write in:"<<*ita<<endl;
    }
    //cout<<endl;

    system("pause");
}