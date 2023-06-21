//
// Created by jiaxin on 2023/6/21 13:39.
//

#include <iostream>
#include "threadpool.h"
#include "connectpool.h"
using std::cin;
using std::cout;
using std::endl;
int main()
{
    threadpool *p;
    cout << "main\n";
    p = new threadpool();
    cout << "new\n";
    delete p;
    connectpool p1;
    return EXIT_SUCCESS;
}
