//
// Created by jiaxin on 2023/6/21 14:11.
//

#ifndef TINYWEBSERVER_CONNECTPOOL_H
#define TINYWEBSERVER_CONNECTPOOL_H

#include <mysql/mysql.h>
#include <string>
#include "lock.h"
#include "log.h"
#include <list>
using std::list;
using std::string;

/**
 * 1. 数据库连接池，通过单例模式保证连接池对象唯一\n
 * 2. 通过List<MYSQL*>实现连接池\n
 * 3. 通过互斥锁和信号量确保线程安全
 */
class connectpool
{
public:
    //从连接池获取一个连接
    MYSQL *GetConnection();
    //释放一个连接，规范给连接池
    bool ReleaseConnection(MYSQL *conn);
    int GetFreeConn() const;
    //销毁整个连接池
    void FreePool();

    //单例模式
    static connectpool *GetInstance();

    //连接池初始化
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

    //单例模式嵌套类，delete动态资源
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

/**
 * 通过RAII机制获取数据库连接
 */
class connectionRAII
{
public:
    connectionRAII(MYSQL **connRaii, connectpool *sqlpollRaii);
    ~connectionRAII();

private:
    MYSQL *connRAII;
    connectpool *sqlpollRAII;
};

#endif//TINYWEBSERVER_CONNECTPOOL_H
