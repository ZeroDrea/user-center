#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <string>
#include <unordered_map>

class HttpRequest {
public:
    enum Method { kInvalid, kGet, kPost, kPut, kDelete, kHead, kPatch };
    
    void setMethod(Method method) { method_ = method; }
    void setPath(const std::string& path) { path_ = path; }
    void setVersion(const std::string& version) { version_ = version; }
    void setQuery(const std::string& query) { query_ = query; }
    void setBody(const std::string& body) { body_ = body; }
    void addHeader(const std::string& key, const std::string& value) {
        headers_[key] = value;
    }

    void getHeadAll() const;
    
    Method getMethod() const { return method_; }
    const std::string& getPath() const { return path_; }
    const std::string& getVersion() const { return version_; }
    const std::string& getQuery() const { return query_; }
    const std::string& getBody() const { return body_; }
    std::string getHeader(const std::string& key) const;
    int getContentLength() const;
    
private:
    Method method_ = kInvalid;
    std::string path_;
    std::string version_;
    std::string query_;
    std::string body_;
    std::unordered_map<std::string, std::string> headers_;
};

#endif // HTTP_REQUEST_H