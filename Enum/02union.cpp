#include<iostream>
#include<string>
#include<bits/stdc++.h>
#include<stdio.h>
using namespace std;

int main()
{
        union
    {
        int i;
        struct
        {
            char first;
            char second;
        }half;  
    }number;

    number.i = 0x4241;
    sprintf("%c%c\n", number.half.first, number.half.second);
    //printf("%f\n", x.t);
    system("pause");
} 