#ifndef NET_BUFFER_H
#define NET_BUFFER_H

#include <vector>
#include <string>
/**
 * 高性能网络缓冲区，用于非阻塞 I/O 的数据收发。
 * 
 * 内部内存布局：
 * [prependable] [readable] [writable]
 * 0            r          w         size
 * 
 * - prependable: 预留空间，用于在头部快速写入（如消息长度）
 * - readable:    已接收待应用层读取的数据
 * - writable:    空闲区域，可供 recv 写入
 * 
 * 通过移动 readIndex_ 和 writeIndex_ 来避免频繁的内存拷贝。
 */
class Buffer {
public:
    // 默认 prepend 区域大小（字节），用于在消息头部插入数据
    static constexpr size_t kCheapPrepend = 8;
    // 初始缓冲区大小（不含 prepend）
    static constexpr size_t kInitialSize = 1024;

    /**
     * 构造函数。
     * @param initialSize 初始可写空间大小（不包括 prepend 区域）
     */
    explicit Buffer(size_t initialSize = kInitialSize);

    // 禁止拷贝，允许移动（移动语义默认即可）
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer(Buffer&&) = default;
    Buffer& operator=(Buffer&&) = default;

    // ------------------------- 基本属性查询 -------------------------
    
    /** 可读字节数（即尚未被应用取走的数据长度） */
    size_t readableBytes() const { return writeIndex_ - readIndex_; }

    /** 可写字节数（当前空闲区域长度） */
    size_t writableBytes() const { return buffer_.size() - writeIndex_; }

    /** prepend 区域大小（即 readIndex_ 前面的空间） */
    size_t prependableBytes() const { return readIndex_; }

    /** 返回可读数据的起始指针（const 版本） */
    const char* peek() const { return begin() + readIndex_; }

    /** 返回可写区域的起始指针（非 const） */
    char* beginWrite() { return begin() + writeIndex_; }

    /** 返回可写区域的起始指针（const 版本） */
    const char* beginWrite() const { return begin() + writeIndex_; }

public:  // 暂不开放
    // ------------------------- 数据消费（取走数据） -------------------------

    /**
     * 从缓冲区取走 len 字节（移动 readIndex_）
     * 前提：len <= readableBytes()
     */
    void retrieve(size_t len);
    void retrieveAll();

   
    /** 取走取走 len 字节，并返回字符串 */
    std::string retrieveAsString(size_t len);
    std::string retrieveAllAsString();

    // ------------------------- 数据写入（追加） -------------------------

    /** 确保至少有 len 字节的可写空间（必要时扩容或移动数据） */
    void ensureWritableBytes(size_t len);

    /** 追加数据（从指针） */
    void append(const char* data, size_t len);

    /** 追加数据（从 std::string） */
    void append(const std::string& str);

    // ------------------------- 前置写入（在头部插入） -------------------------

    /**
     * 在现有可读数据的前面插入数据（利用 prepend 区域）。
     * 前提：len <= prependableBytes()
     * 常用于先构造 body，再回头写入 total length。
     */
    void prepend(const char* data, size_t len);

    /** 前置写入（std::string 版本） */
    void prepend(const std::string& str);

    // ------------------------- 辅助查找（针对常见协议） -------------------------

    /**
     * 查找 "\r\n" 在可读数据中的位置（偏移量，从 peek() 开始计算）。
     * @return 找到的偏移，未找到返回 std::string::npos
     */
    size_t findCRLF() const;

    /**
     * 从指定偏移 start 开始查找 "\r\n"。
     * @param start 起始偏移（相对于 peek()）
     */
    size_t findCRLF(size_t start) const;

public:
    // ------------------------- 高性能 I/O 辅助方法 -------------------------

    /**
     * 从文件描述符 fd 读取数据，自动处理非阻塞 EAGAIN。
     * 内部使用 readv 分散读：先填充 buffer 内部可写区域，多余数据读入栈上临时缓冲。
     * @param fd 已设置非阻塞的文件描述符
     * @param savedErrno 保存 errno（因为调用者可能覆盖）
     * @return 读取的字节数，-1 表示错误（errno 保存在 savedErrno）
     */
    ssize_t readFd(int fd, int* savedErrno);

    /**
     * 向文件描述符 fd 写入数据（尽可能多发）。
     * 内部使用 writev 分散写：将 buffer 中的可读数据 + 用户数据一起发送。
     * @param fd 非阻塞 fd
     * @param savedErrno 保存 errno
     * @return 写入的字节数，-1 表示错误
     */
    ssize_t writeFd(int fd, int* savedErrno);

    // ------------------------- 内存收缩（可选） -------------------------

    /** 收缩内部 额外预留的空间，释放多余内存（在连接空闲时调用） */
    void shrink(size_t reserve = kInitialSize);

private:
    // 返回 vector 内部数组起始指针（非 const）
    char* begin() { return buffer_.data(); }
    // const 版本
    const char* begin() const { return buffer_.data(); }

    /**
     * 确保至少有 len 字节可写空间的核心实现。
     * 策略：
     * 1. 如果剩余可写 + prepend 区域总和足够，则移动可读数据到前端（腾出连续空间）。
     * 2. 否则，直接扩容 buffer_。
     */
    void makeSpace(size_t len);

    std::vector<char> buffer_;   // 底层存储
    size_t readIndex_;           // 下一个待读取字节的索引
    size_t writeIndex_;          // 下一个待写入字节的索引
};

#endif // NET_BUFFER_H