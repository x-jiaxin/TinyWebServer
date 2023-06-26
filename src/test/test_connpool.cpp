//
// Created by jiaxin on 2023/6/21 13:39.
//

#include <iostream>
#include "connectpool.h"
using std::cin;
using std::cout;
using std::endl;
int main()
{
    auto url = "localhost", username = "root", passwd = "123456";
    int port = 3306;
    const char *DBname = "DB1";
    auto sql_pool = connectpool::GetInstance();
    if (!sql_pool)
    {
        exit(1);
    }
    sql_pool->init(url, username, passwd, DBname, port, 10, false);
    {
        MYSQL *conn;
        connectionRAII conn_raii(&conn, sql_pool);
        cout << conn->db << endl;
    }

    return EXIT_SUCCESS;
}
