#include<iostream>
#include<vector>
#include<list>
#include<io.h>
#include<cstring>

using namespace std;

vector<string> getFiles(const string &path, const bool isFolder)
{
    vector<string> files;
    list<string> subFolders;
    subFolders.push_back(path);

    while(!subFolders.empty())
    {
       string current_folder(subFolders.back());

       if(*(current_folder.end() - 1) != '/')
       {
           current_folder.append("/*");
       } 
       else{
           current_folder.append("*");
       }

       subFolders.pop_back();

       struct _finddata_t fileinfo;
       long handle = _findfirst(current_folder.c_str(), &fileinfo);

        while(handle != -1)
        {
            if(isFolder && (!strcmp(fileinfo.name, ".") || !strcmp(fileinfo.name, "..")))
            {
                if(_findnext(handle, &fileinfo) != 0) break;
                continue;
            }

            if(fileinfo.attrib & _A_SUBDIR)
            {
                if(isFolder)
                {
                    string path(current_folder);
                    path.pop_back();

                    path.append(fileinfo.name);
                    subFolders.push_back(path.c_str());
                }
            }
            else
            {
                string file_path;
                file_path.assign(current_folder.c_str()).pop_back();
                file_path.append(fileinfo.name);
                files.push_back(file_path);
            }

            if(_findnext(handle, &fileinfo) != 0) break;
        }

        _findclose(handle);
    }

    return files;
}

int main()
{
    string path = "E:/xmind";
    vector<string> out;
    out = getFiles(path, true);
    for(int i = 0; i<out.size(); i++)
    {
        cout<<out[i]<<endl;
    }
    system("pause");
    return 0;
}