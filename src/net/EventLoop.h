#include "EpollPoller.h"

class EventLoop {
public:
    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);
    void loop(); // 主循环
private:
    EpollPoller poller_;  // 每个循环一个poller，单 Reactor 时只有一个
};