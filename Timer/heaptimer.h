#ifndef HEAP_TIMER_H
#define HERP_TIMER_H

#include <functional>
#include <unordered_map>
#include <queue>
#include <time.h>
// assert���ԭ�Ͷ�����<assert.h>�У�����������������������ش�������ֹ����ִ�С�
#include <assert.h> //��̬���Ե�ͷ�ļ�
#include <algorithm>

/*
    std::chrono��C++11���������ʱ�䴦���
    ����3��
*/
#include <chrono> // ��һ��time library
#include <arpa/inet.h> // �����̳��õ�ͷ�ļ�


typedef std::function<void()> TimeoutCallBack;
typedef std::chrono::high_resolution_clock Clock;
typedef std::chrono::milliseconds MS;// ����
typedef Clock::time_point TimeStamp;// ʱ���

// ��ʱ���ڵ�
struct TimerNode{
    int id;
    TimeStamp expires;// ����ʱ��(����ͳ�ʱ��)
    TimeoutCallBack cb;// �ص�����
    // ����С�ں�
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

    std::vector<TimerNode> heap_;// vector������ж�ʱ��

    std::unordered_map<int, size_t> ref_;// ���ÿ����ʱ����heap_�е�λ��
};


#endif //HEAP_TIMER_H

















