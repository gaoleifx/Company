#include<iostream>
#include<vector>
#include<list>
#include<io.h>
#include<cstring>

using namespace std;

void get_file(const string& path, const bool isFolder)
{
    vector<string> files;
    list<string> subFolders;
    subFolders.push_back(path);

    while(!subFolders.empty())
    {
        string current_folder(subFolders.back());

        if(*(current_folder.end() - 1) != '\\')
        {
            current_folder.append("\\*");
        }
        else
        {
            current_folder.append("*");
        }

        subFolders.pop_back();//delete path

        struct _finddata_t fileinfo;
        long handle = _findfirst(current_folder.c_str(), &fileinfo);

        cout<<current_folder<<endl;

        while(handle != -1)
        {
            if(isFolder && (!strcmp(fileinfo.name, ".") || !strcmp(fileinfo.name, "..")))
            {
                if(_findnext(handle, &fileinfo) != 0) break;
                continue;
            }
        }

        if(fileinfo.attrib & _A_SUBDIR)
        {
            if(isFolder)
            {
                string current_path(current_folder);
                current_path.pop_back();
                current_path.append(fileinfo.name);
                subFolders.push_back(current_path.c_str());
                cout<<subFolders<<endl;
            }
        }

        
    }

}

int main()
{
    string path = "E:\\HoudiniProjects\\newProject";
    get_file(path, true);
    system("pause");
    return 0;
}