#include "llhttp.h"
#include <iostream>
#include <string>

int main() {
    llhttp_t parser;
    llhttp_settings_t settings;

    llhttp_settings_init(&settings);
    llhttp_init(&parser, HTTP_REQUEST, &settings);

    std::string request = "GET /hello HTTP/1.1\r\nHost: example.com\r\n\r\n";
    enum llhttp_errno err = llhttp_execute(&parser, request.data(), request.size());

    if (err == HPE_OK) {
        std::cout << "HTTP 请求解析成功！" << std::endl;
    } else {
        std::cout << "解析失败: " << llhttp_get_error_reason(&parser) << std::endl;
    }
    return 0;
}