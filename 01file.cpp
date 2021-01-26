#include<iostream>
#include<io.h>

using namespace std;

void fileSearch()
{
    string path = "E:\\HoudiniProjects\\newProject";
    struct _finddata_t fileinfo;
    long handleFile = 0;
    string filePath;
    handleFile = _findfirst(filePath.assign(path).c_str(), &fileinfo);
    if(handleFile == -1)
        return;
    printf("%s\n", fileinfo.name);
    do
    {
        printf("%s\n", fileinfo.name);

    }while(!_findnext(handleFile, &fileinfo));
    _findclose(handleFile);
}


int main()
{
    fileSearch();
    system("pause");
    return 0;    
}