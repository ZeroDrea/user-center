#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H
#include <string>
#include <unordered_map>

class HttpResponse {
public:
    void setStatusCode(int code);
    void setStatusMessage(const std::string& msg) { statusMessage_ = msg; }
    void setBody(const std::string& body) { body_ = body; }
    void setContentType(const std::string& type) { setHeader("Content-Type", type); }
    void setHeader(const std::string& key, const std::string& value);
    std::string serialize() const;
private:
    int statusCode_ = 200;
    std::string statusMessage_ = "OK";
    std::string body_;
    std::unordered_map<std::string, std::string> headers_;
};

#endif // HTTP_RESPONSE_H