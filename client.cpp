#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

using namespace std;

int main() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    connect(fd, (sockaddr*)&addr, sizeof(addr));

    string msg = "Hello TCP Server!!\n";
    
    for (int i = 0; i < 50; i++) {
        ::write(fd, msg.c_str(), msg.size());
        sleep(5);
    }
    

    close(fd);
    return 0;
}