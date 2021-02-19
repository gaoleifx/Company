#include<iostream>

using namespace std;

struct mystuff
{
    string name;
    string jobs[5];
    int age;
    float sub;

}preson1;

int main()
{
    struct mystuff preson1 = {"xiaoming", {"xiwan", "xizao"}, 20, 10.2};
    struct mystuff preson2 = preson1;

    cout<<preson2.jobs[1]<<endl;
    system("pause");

}