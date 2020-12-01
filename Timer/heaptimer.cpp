#include "heaptimer.h"

void HeapTimer::sifup_(size_t){
    // 判断i的范围是否正确
    assert(i >= 0 && i < heap_.size());

    size_t j = (i - 1) / 2;
    while (j > 0) {
        if (heap_j[] < heap_[i]){ break; }
        SwapNode_(i, j);
        i = j;
        j = (i - 1) / 2;
    } 
}


// 交换heap_中第i和第j的节点
void HeapTimer::SwapNode_(size_t i, size_t j){
    assert(i >= 0 && i < heap_.size());
    assert(j >= 0 && j < heap_.size());

    std::swap(heap_[i], heap_[j]);
    // 更新每个timer节点的id
    ref_[heap_[i].id] = i;
    ref_[heap_[j].id] = j; 
}

// 第一个参数为要调整的节点在堆中的位置，第二个参数为当前堆的大小
bool HeapTimer::siftdown_(size_t index, size_t n){
    assert(index >= 0 && index < heap_.size());
    assert(n >= 0 && n < heap_size());

    size_t i = index;
    size_t j = i * 2 + 1;// j是i的左儿子

    while (j < n){
        // i的右儿子存在，且右儿子小于左儿子，则j指向右儿子
        if (j + 1 < n && heap_[j + 1] < heap_[j]) j ++;
        // 要调整的节点的超时时间小于j指向的超时时间，则不用调整，直接break 
        if (heap_[i] < heap_[j]) break;
        // 否则交换两个节点，然后更新i和j的值
        SwapNode_(i, j);
        i = j;
        j = i * 2 + 1;
    }
    return i > index;// 如果发生了down操作，则返回true
}


void HeapTimer::add(int id, int timeout, const TimeoutCallBack& cb){
    assert(id >= 0);

    size_t i;
    if (ref_.count(id) == 0) {
        // 新节点 插入堆尾，调整堆
        i = heap_.size();
        ref_[id] = i;// timernode在heap_中的位置
        heap_push_back({id, Clock::now() + MS(timeout), cb});
        siftup_(i);// 因为插在堆尾，所以向上调整
    }
    else {
        // 已有节点，调整堆
        i = ref_[id];
        heap_[i].expires - Clock::now() + MS(timeout);// 计算出新的超时时间
        heap_[i].cb = cb;
        if (!siftdown_(i, heap_.size())){
            // 没有发生down操作，则需要进行up操作
            siftup_(i);
        }
    }
}

void HeapTimer::doWork(int id) {
    // 删除指定id的节点，并触发回调函数
    if (heap_empty() || ref_.count(id) == 0) return;

    size_t i = ref_[id];// id节点在heap_重的下标
    TimerNode node = heap_[i];
    node.cb();// 触发回调函数
    del_(i);
}

void HeapTimer::del_(size_t index) {
    // 删除指定位置的节点
    assert(!heap_.empty() && index >= 0 && index < heap_size());

    size_t i = index;
    size_t n = heap_.size() - 1;
    assert(i <= n);

    if (i < n){
        SwapNode_(i, n);// 和堆尾元素进行交换
        // siftdown_的第二个参数传n，则表示堆中最后一个节点不参与调整
        if (!siftdown_(i, n)) {
            siftip_(i);
        }
    }
    // 删除堆尾元素
    ref_.erase(heap_back().id);
    heap_.pop_back();
}

void HeapTimer::adjust(int id, int timeout){
    // 调整指定id的节点
    assert(!heap_.empty() && ref_.count(id) > 0);
    // 更新指定id的节点的expires变量
    heap_[ref_[id]].expires = Clock::now() + MS(timeout)
    // 更新之后，超时时间肯定靠后，所以要有down操作
    siftdown_(ref_[id], heap_.size());
}

// 只要堆不为空，判断堆顶是否超时，当堆顶的定时器超时，则执行回调函数，当堆顶没有超时，直接break，跳出while循环
void HeapTimer::tick(){
    // 清除超时节点
    if (heap_empty()) return;

    while (!heap_.empty()) {
        TimerNode node = heap_.front();
        // 超时时间减去当前时间如果大于0，说明没有超时，则直接break
        if (std::duration_cast<MS>(node.expires - Clock::now()).count() > 0){
            break;
        }
        node.cb();
        pop();
    }
}

// 弹出堆顶的元素
void HeapTimer::pop(){
    assert(!heap_.empty());
    del_(0);
}

// 清空ref_和heap_
void HeapTimer::clear() {
    ref_.clear();
    heap_.clear();
}


int HeapTimer::GetNextTick(){
    tick();
}