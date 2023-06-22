//
// Created by jiaxin on 2023/6/21 14:11.
//

#ifndef TINYWEBSERVER_CONNECTPOOL_H
#define TINYWEBSERVER_CONNECTPOOL_H

#include <iostream>
#include <mysql/mysql.h>
#include "lock.h"
#include <list>
using std::cin;
using std::cout;
using std::endl;
using std::list;
using std::string;
class connectpool
{
public:
    MYSQL *GetConnection();
    bool ReleaseConnection(MYSQL *conn);
    int GetFreeConn() const;
    void FreePool();

    static connectpool *GetInstance();

    void init(const string &url, const string &username, const string &password,
              const string &dbname, int port, int MaxConn, bool closelog);

private:
    connectpool();
    ~connectpool();

    int m_MaxConn{};//最大连接数
    int m_CurConn;  //当前连接数
    int m_FreeConn; //空闲连接数
    locker m_lock;
    list<MYSQL *> m_connList;
    sem m_ListState;

    static connectpool *sqlpool;//单例模式

    class FreeInstance
    {
    public:
        ~FreeInstance();
    };
    static FreeInstance freeInstance;

public:
    string m_url;
    int m_port{};
    string m_username;
    string m_passwd;
    string m_DBName;//数据库名
    bool m_close_log{false};
};

class connectionRAII
{
public:
    connectionRAII(MYSQL **connRaii, connectpool *sqlpollRaii);
    virtual ~connectionRAII();

private:
    MYSQL *connRAII;
    connectpool *sqlpollRAII;
};

#endif//TINYWEBSERVER_CONNECTPOOL_H
