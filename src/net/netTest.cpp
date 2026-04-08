#include "net/Buffer.h"

#include "net/netTest.h"
#include <iostream>

#include <iostream>
#include <cassert>
#include <cstring>
#include <unistd.h>

using namespace std;

void printStatus(const Buffer& buf, const char* desc) {
    printf("[%s] readable=%zu, writable=%zu, prepend=%zu\n",
        desc,
        buf.readableBytes(),
        buf.writableBytes(),
        buf.prependableBytes()
    );
}

void testBasic() {
    cout << "===== 测试1：基础读写 =====" << endl;

    Buffer buf;
    printStatus(buf, "init");

    // 写入
    string msg = "hello buffer";
    buf.append(msg.c_str(), msg.size());
    printStatus(buf, "after append");

    assert(buf.readableBytes() == msg.size());

    // 读取
    string s = buf.retrieveAsString(buf.readableBytes());
    cout << "读取内容：" << s << endl;
    assert(s == msg);

    printStatus(buf, "after retrieveAll");
    cout << "测试1通过\n" << endl;
}

void testPrepend() {
    cout << "===== 测试2：prepend 头部插入 =====" << endl;

    Buffer buf;
    buf.append("world", 5);

    // 头部插入 hello
    buf.prepend("hello ", 6);

    string all = buf.retrieveAllAsString();
    cout << "拼接结果：" << all << endl;
    assert(all == "hello world");

    cout << "测试2通过\n" << endl;
}


void testExpand() {
    cout << "===== 测试3：自动扩容 =====" << endl;

    Buffer buf(10); // 初始可写 10 字节
    printStatus(buf, "init 10");

    // 写入 20 字节，触发扩容
    string s(20, 'x');
    buf.append(s);

    printStatus(buf, "after append 20");
    assert(buf.readableBytes() == 20);

    cout << "测试3通过\n" << endl;
}


void testShrink() {
    cout << "===== 测试4：shrink 内存收缩 =====" << endl;

    Buffer buf;
    for (int i = 0; i < 100; ++i) {
        buf.append("a", 1);
    }

    printf("收缩前 capacity: %zu\n", buf.writableBytes() + buf.readableBytes() + buf.prependableBytes());

    buf.shrink(50);

    printf("收缩后 capacity: %zu\n", buf.writableBytes() + buf.readableBytes() + buf.prependableBytes());

    assert(buf.readableBytes() == 100);
    cout << "测试4通过\n" << endl;
}

void testFindCRLF() {
    cout << "===== 测试5：find CRLF =====" << endl;

    Buffer buf;
    buf.append("hello\r\nworld");

    size_t pos = buf.findCRLF();
    cout << "CRLF 位置：" << pos << endl;
    assert(pos == 5);

    cout << "测试5通过\n" << endl;
}

void testReadWriteFd() {
    cout << "===== 测试6：readFd / writeFd 模拟 =====" << endl;

    int pipefd[2];
    int ret = pipe(pipefd);
    if (ret != 0) {
        return;
    }
    Buffer sendBuf;
    sendBuf.append("test io buffer");

    // 写入管道
    int err;
    sendBuf.writeFd(pipefd[1], &err);

    // 从管道读取
    Buffer recvBuf;
    recvBuf.readFd(pipefd[0], &err);

    string res = recvBuf.retrieveAllAsString();
    cout << "IO 结果：" << res << endl;
    assert(res == "test io buffer");

    close(pipefd[0]);
    close(pipefd[1]);

    cout << "测试6通过\n" << endl;
}

void TestBuffer() {
    testBasic();
    testPrepend();
    testExpand();
    testShrink();
    testFindCRLF();
    testReadWriteFd();
    cout << "所有测试都通过......\n";
}
void netTest() {
    std::cout << "net测试......" << std::endl;
}