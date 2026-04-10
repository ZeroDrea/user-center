
#ifndef HTTP_CONTEXT_H
#define HTTP_CONTEXT_H

#include "HttpRequest.h"
#include "utils/Buffer.h"

class HttpContext {
public:
    enum ParseState {
        kExpectRequestLine,
        kExpectHeaders,
        kExpectBody,
        kGotAll,
        kError
    };
    
    HttpContext() : state_(kExpectRequestLine) {}
    
    // 返回值：true 表示解析出一个完整请求（可能刚完成，也可能之前已完成）
    bool parse(Buffer* buf);
    
    bool gotAll() const { return state_ == kGotAll; }
    bool isError() const { return state_ == kError; }
    const HttpRequest& getRequest() const { return request_; }
    
    void reset() {
        state_ = kExpectRequestLine;
        request_ = HttpRequest();  // 需要实现拷贝或移动
    }
    
private:
    bool parseRequestLine(const char* begin, const char* end);
    bool parseHeader(const char* begin, const char* end);
    
    ParseState state_;
    HttpRequest request_;
};

#endif // HTTP_CONTEXT_H