//
// Created by jiaxin on 2023/6/25 19:35.
//

#ifndef TINYWEBSERVER_CONFIG_H
#define TINYWEBSERVER_CONFIG_H

#include <iostream>
using std::cin;
using std::cout;
using std::endl;
class config
{
public:
    config();

    void parse_arg(int argc, char *argv[]);

    int PORT;

    int ASYN_LOGWrite;

    int TRIGMode;

    int OPT_LINGER;

    int sql_num;

    int thread_num;

    int close_log;

    int actor_model;
};

#endif//TINYWEBSERVER_CONFIG_H
