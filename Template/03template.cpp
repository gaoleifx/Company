#include<iostream>

using namespace std;

template<class S>
void myswap(S& a, S& b)
{
    S temp = a;
    a = b;
    b = temp;
}

template<class T>
void mysort(T& array, int len)
{
    for(int i = 0; i<len; i++)
    {
        int max = i;
        for(int j = i+1; j<len; j++)
        {
            if(array[j] < array[max])
            {
                max = j;
            }
        }
        if(max != i)
        {
            myswap(array[max], array[i]);
        }
    }
}

template<class T>
void myprint(T arr[], int len)
{
        
    for(int i = 0; i<len; i++)
    {
        cout<<arr[i]<<" ";

    }
    cout<<endl;
}

void test01()
{
    char charArr[] = "bajegoizn";
    int len = sizeof(charArr) / sizeof(char);
    mysort(charArr, len);
    myprint(charArr, len);

}

void test02()
{
    int intArr[] = {4,2,9,3,6,7,2,5};
    int len = sizeof(intArr) / sizeof(intArr[0]);
    mysort(intArr, len);
    myprint(intArr, len);
}



int main()
{
   //test01();
    test02();
    system("pause");
}