#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <vector>
#include <cerrno>
#include <fcntl.h>
using namespace std;

int main() {
    // 1. 创建socket
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        cout << "create socket fail\n";
        return -1;
    }

    int flag = fcntl(listen_fd, F_GETFL, 0);        // 获取当前属性
    fcntl(listen_fd, F_SETFL, flag | O_NONBLOCK);

    
    // 2. 绑定
    string ip = "127.0.0.1";
    uint16_t port = 8080;
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip.c_str());
    if (bind(listen_fd, (const sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        cout << "bind fail\n";
        return -1;
    }

    // 3. 监听
    if (listen(listen_fd, 3) < 0) {
        cout << "listen fail\n";
        return -1;
    }

    cout << "等待客户端连接...\n";
    // 4. 提取

    int client_fd = -1;
    while (true) {
        sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);
        memset(&client_addr, 0, sizeof(client_addr));
        client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &len);
        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                cout << "暂无客户端连接，继续等待...\n";
                sleep(3);
                continue;
            } else {
                cout << "accept fail\n";
                return -1;
            }
        }

        int client_flag = fcntl(client_fd, F_GETFL, 0);
        fcntl(client_fd, F_SETFL, client_flag | O_NONBLOCK);
        cout << "有新的客户端连接活过来 ip: " << inet_ntoa(client_addr.sin_addr) 
            << ":" << ntohs(client_addr.sin_port) << endl;
        vector<char> buf(64);
        while (true) { // 非阻塞读
            int n = ::read(client_fd, buf.data(), buf.size() - 1);
            if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    cout << "暂无数据，继续等待...\n";
                    sleep(2);
                    continue;;
                } else {
                    cout << "read fail\n";
                    close(client_fd);
                    break;
                }
            }
    
            if (n == 0) {
                cout << "客户端正常断开\n";
                close(client_fd);
                break;
            }

            buf[n] = '\0';
            cout << "收到消息：" << buf.data() << endl;
        }


    }
    
    close(listen_fd);
}