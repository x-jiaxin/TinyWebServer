//
// Created by jiaxin on 2023/6/22 16:14.
//

#include "log.h"

int main()
{
    //    Log::getInstance()->init("/home/jiaxin/Code/TinyWebServer/log/a.log", 0,
    //                             2000, 800000, 800);
    Log::getInstance()->init("log", 0, 2000, 800000, 800);

    LOG_ERROR("error test")
    LOG_DEBUG("debug test")
    LOG_INFO("info test")
    LOG_WARN("warn test")
    return 0;
}
