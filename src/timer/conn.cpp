//
// Created by jiaxin on 2023/6/25 13:26.
//

#include <iostream>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cassert>
using std::cin;
using std::cout;
using std::endl;

int timeout_connect(const char *ip, int port, int time)
{
    int ret = 0;
    sockaddr_in address{};
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address);
    address.sin_port = htons(port);

    int sockfd = socket(PF_INET, SOCK_STREAM, 0);

    timeval timeout{};
    timeout.tv_sec = time;
    timeout.tv_usec = 0;
    socklen_t len = sizeof(timeout);
    ret = setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, len);
    assert(ret != -1);
    ret = connect(sockfd, (sockaddr *)&address, sizeof(address));
    if (ret == -1) {
        if (errno == EINPROGRESS) {
            printf("connecting timeout!\n");
            return -1;
        }
        else {
            printf("error occur!\n");
            return -1;
        }
    }
    return sockfd;
}
int main()
{
    /*const char *ip = "www.baidu.com";
    int port = 8888;
    int sockfd = timeout_connect(ip, port, 10);
    if (sockfd == -1) {
        return -1;
    }*/
    auto cur_time = time(nullptr);
    return 0;
}
