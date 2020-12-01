#ifndef HEAP_TIMER_H
#define HERP_TIMER_H

#include <functional>
#include <unordered_map>
#include <queue>
#include <time.h>
// assert宏的原型定义在<assert.h>中，其作用是如果它的条件返回错误，则终止程序执行。
#include <assert.h> //静态断言的头文件
#include <algorithm>

/*
    std::chrono是C++11引入的日期时间处理库
    包含3种
*/
#include <chrono> // 是一个time library
#include <arpa/inet.h> // 网络编程常用的头文件


typedef std::function<void()> TimeoutCallBack;
typedef std::chrono::high_resolution_clock Clock;
typedef std::chrono::milliseconds MS;// 毫秒
typedef Clock::time_point TimeStamp;// 时间戳

// 定时器节点
struct TimerNode{
    int id;
    TimeStamp expires;// 过期时间(几点就超时了)
    TimeoutCallBack cb;// 回调函数
    // 重载小于号
    bool operator<(const TimerNode& t) {
        return expire < t.expire;
    }
};

class HeapTimer{
public:
    HeapTimer() {heap_reserve(64); }
    ~HeapTimer() {}

    void adjust(int id, int new Expires);

    void add(int id, int timeOut, const TimeoutCallBack& cb);

    void doWork(int id);

    void clear();

    void tick();

    void pop();

    int GetNextTick();

private:
    void del_(sise_t i);

    void siftup_(size_t i);

    bool siftdown_(size_t index, size_t n);

    void SwapNode_(size_t i, size_t j);

    std::vector<TimerNode> heap_;// vector存放所有定时器

    std::unordered_map<int, size_t> ref_;// 存放每个定时器在heap_中的位置
};


#endif //HEAP_TIMER_H

















