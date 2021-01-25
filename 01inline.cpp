#include<iostream>
#include<algorithm>
#include<vector>
using namespace std;
#define day 7

int main()
{
    int array[] = {11, 22, 33, 44, 55};
    for(int x : array)
    {
        cout<<x<<" ";
    }
    cout<<endl;

    for(auto x : array)
    {
        cout<<x<<" ";
    }
    cout<<endl;

    for(int i = 0; i<6; i++){
        switch(array[i])
        {
            case 11:
                cout<<"a"<<endl;
                break;
            case 22:
                cout<<"b"<<endl;
                break;
            case 33:
                cout<<"c"<<endl;
                break;
            default:
                cout<<"default"<<endl;
        }
    }

    // #ifdef day
    //     cout<<"the define day"<<endl;

    system("pause");

}

