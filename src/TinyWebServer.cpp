//
// Created by jiaxin on 2023/6/25 16:18.
//

#include <server.h>
#include <config.h>

int main(int argc, char *argv[])
{
    //需要修改的数据库信息,登录名,密码,库名
    string user = "root";
    string passwd = "123456";
    string databasename = "DB1";

    //命令行解析
    config config;
    config.parse_arg(argc, argv);

    server my_webserver;

    //初始化
    my_webserver.init(config.PORT, user, passwd, databasename,
                      config.ASYN_LOGWrite, config.OPT_LINGER, config.TRIGMode,
                      config.sql_num, config.thread_num, config.close_log,
                      config.actor_model);

    //日志
    my_webserver.log_write();

    //数据库
    my_webserver.sql_pool();

    //线程池
    my_webserver.thread_pool();

    //触发模式
    my_webserver.trig_mode();

    //监听
    my_webserver.eventListen();

    //运行
    my_webserver.eventLoop();

    return 0;
}
