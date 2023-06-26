//
// Created by jiaxin on 2023/6/25 19:35.
//

#include <getopt.h>
#include "config.h"
config::config()
{
    PORT = 8888;

    /*日志写入方式默认同步*/
    LOGWrite = 0;

    /*Listenfd LT + connfd LT*/
    TRIGMode = 0;

    /*listenfd触发模式，默认LT*/
    LISTENTrigmode = 0;

    /*connfd触发模式，默认LT*/
    CONNTrigmode = 0;

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
    /*"-p  1 -l 2 -m 3 -o 4 -s 5 -t 6 -c 7 -a 8"*/
    int opt;
    const char *str = "p:l:m:o:s:t:c:a:";
    while ((opt = getopt(argc, argv, str)) != -1)
    {
        switch (opt)
        {
            case 'p': {
                PORT = atoi(optarg);
                break;
            }
            case 'l': {
                LOGWrite = atoi(optarg);
                break;
            }
            case 'm': {
                TRIGMode = atoi(optarg);
                break;
            }
            case 'o': {
                OPT_LINGER = atoi(optarg);
                break;
            }
            case 's': {
                sql_num = atoi(optarg);
                break;
            }
            case 't': {
                thread_num = atoi(optarg);
                break;
            }
            case 'c': {
                close_log = atoi(optarg);
                break;
            }
            case 'a': {
                actor_model = atoi(optarg);
                break;
            }
            default:
                break;
        }
    }
}
