#include<iostream>
using namespace std;

//多态

class Ticket
{
public:
    virtual void buyTicket()=0;
    Ticket(){}
    virtual ~Ticket()
    {
        cout<<"Ticket 析构函数被调用"<<endl;
    }

};

class adult:public Ticket
{
public:
    virtual void buyTicket()
    {
        cout<<"adult buy ticket 500"<<endl;
    }

    adult(){}
    ~adult()
    {
        cout<<"audlt析构函数被调用"<<endl;
    }
};

class child:public Ticket
{
public:
    virtual void buyTicket()
    {
        cout<<"child buy ticket free"<<endl;
    }

    child(){}
    ~child()
    {
        cout<<"child析构函数被调用"<<endl;
    }
};

// void test01(Ticket &ti)
// {
//     ti.buyTicket();
//     //delete ti;
// }


int main()
{
    //adult ad;
    Ticket *ti = new adult();
    ti->buyTicket();
    delete ti;
    ti = new child();
    ti->buyTicket();
    delete ti;


    system("pause");
    return 0;
}
