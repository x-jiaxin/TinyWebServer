//
// Created by jiaxin on 2023/6/25 19:35.
//

#include <getopt.h>
#include "config.h"
config::config()
{
    PORT = 8888;

    /*日志写入方式默认同步*/
    ASYN_LOGWrite = 0;

    /*Listenfd LT  connfd LT*/
    TRIGMode = 1;

    /*保持连接*/
    OPT_LINGER = 0;

    /*连接池数量*/
    sql_num = 8;

    /*线程池数量*/
    thread_num = 8;

    /*关闭日志*/
    close_log = 0;

    /*并发模型，默认proactor*/
    actor_model = 0;
}
void config::parse_arg(int argc, char **argv)
{
    /*"-p  8888 -l 1 -m 1 -o 0 -s 8 -t 8 -c 0 -a 1"*/
    int opt;
    const char *str = "p:l:m:o:s:t:c:a:";
    while ((opt = getopt(argc, argv, str)) != -1)
    {
        switch (opt)
        {
            case 'p':
            {
                PORT = atoi(optarg);
                break;
            }
            case 'l':
            {
                ASYN_LOGWrite = atoi(optarg);
                break;
            }
            case 'm':
            {
                /*0 1 2 3*/
                TRIGMode = atoi(optarg);
                break;
            }
            case 'o':
            {
                OPT_LINGER = atoi(optarg);
                break;
            }
            case 's':
            {
                sql_num = atoi(optarg);
                break;
            }
            case 't':
            {
                thread_num = atoi(optarg);
                break;
            }
            case 'c':
            {
                close_log = atoi(optarg);
                break;
            }
            case 'a':
            {
                /*reactor 1*/
                actor_model = atoi(optarg);
                break;
            }
            default:
                break;
        }
    }
}
