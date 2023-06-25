//
// Created by jiaxin on 2023/6/25 16:18.
//

#include <iostream>
#include "http_conn.h"
using std::cin;
using std::cout;
using std::endl;
int main()
{
    int fd = epoll_create(5);
    http_conn httpConn;
    auto url = "localhost", username = "root", passwd = "123456";
    int port = 3306;
    const char *DBname = "DB1";
    auto sql_pool = connectpool::GetInstance();
    if (!sql_pool) {
        exit(1);
    }
    sql_pool->init(url, username, passwd, DBname, port, 10, false);
    sockaddr_in sockaddr{};
    httpConn.init(10, sockaddr, "./", 1, 0, "user", "123456", "DB1");
    httpConn.initmysql_result(sql_pool);
    return 0;
}
