//
// Created by jiaxin on 2023/6/21 14:11.
//

#include "connectpool.h"
connectpool::connectpool() : m_CurConn(0), m_FreeConn(0) {}
connectpool::~connectpool()
{
    FreePool();
}

//没有这句编译链接失败
connectpool *connectpool::sqlpool = nullptr;
connectpool::FreeInstance connectpool::freeInstance;

//从连接池中获取一个空闲连接
MYSQL *connectpool::GetConnection()
{
    if (m_connList.empty()) {
        return nullptr;
    }

    m_ListState.wait();
    m_lock.lock();

    auto conn = m_connList.front();
    m_connList.pop_front();

    --m_FreeConn;
    ++m_CurConn;

    m_lock.unlock();
    return conn;
}

//归还一个连接到连接池中
bool connectpool::ReleaseConnection(MYSQL *conn)
{
    if (!conn) {
        return false;
    }

    m_lock.lock();
    m_connList.push_back(conn);

    ++m_FreeConn;
    --m_CurConn;

    m_lock.unlock();
    m_ListState.post();

    return true;
}
int connectpool::GetFreeConn() const
{
    return m_FreeConn;
}
void connectpool::FreePool()
{
    m_lock.lock();

    if (!m_connList.empty()) {
        for (auto &iterator: m_connList) {
            mysql_close(iterator);
            //解决mysql_init()报内存泄露的问题
            //如果是在类中使用MySQL，一般是把mysql_close和mysql_library_end()放在析构函数里
            mysql_server_end();
        }
        m_CurConn = 0;
        m_FreeConn = 0;
        //
        m_connList.clear();
    }

    m_lock.unlock();
}
connectpool *connectpool::GetInstance()
{
    if (!sqlpool) {
        sqlpool = new connectpool;
    }
    return sqlpool;
}
void connectpool::init(const string &url, const string &username,
                       const string &password, const string &dbname, int port,
                       int MaxConn, bool closelog)
{
    m_url = url;
    m_username = username;
    m_passwd = password;
    m_DBName = dbname;
    m_port = port;
    m_close_log = closelog;
    for (int i = 0; i < MaxConn; ++i) {
        MYSQL *con = nullptr;
        con = mysql_init(con);
        if (!con) {
            //            LOG_ERROR("MySQL Error");
            cout << "Mysql error!\n";
            exit(1);
        }
        con = mysql_real_connect(con, url.c_str(), username.c_str(),
                                 password.c_str(), dbname.c_str(), port,
                                 nullptr, 0);
        if (!con) {
            // log
            cout << "MySQL error!\n";
            exit(1);
        }
        m_connList.push_back(con);
        ++m_FreeConn;
    }

    // 初始信号量
    m_ListState = sem(m_FreeConn);
    m_MaxConn = m_FreeConn;
}

connectionRAII::connectionRAII(MYSQL **connRaii, connectpool *sqlpollRaii)
{
    *connRaii = sqlpollRaii->GetConnection();
    connRAII = *connRaii;
    sqlpollRAII = sqlpollRaii;
}

connectionRAII::~connectionRAII()
{
    sqlpollRAII->ReleaseConnection(connRAII);
}

connectpool::FreeInstance::~FreeInstance()
{
    //    Clang-Tidy: 'if' statement is unnecessary; deleting null pointer has no effect
    delete connectpool::sqlpool;
}
