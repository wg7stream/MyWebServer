#include "heaptimer.h"

void HeapTimer::sifup_(size_t){
    // �ж�i�ķ�Χ�Ƿ���ȷ
    assert(i >= 0 && i < heap_.size());

    size_t j = (i - 1) / 2;
    while (j > 0) {
        if (heap_j[] < heap_[i]){ break; }
        SwapNode_(i, j);
        i = j;
        j = (i - 1) / 2;
    } 
}


// ����heap_�е�i�͵�j�Ľڵ�
void HeapTimer::SwapNode_(size_t i, size_t j){
    assert(i >= 0 && i < heap_.size());
    assert(j >= 0 && j < heap_.size());

    std::swap(heap_[i], heap_[j]);
    // ����ÿ��timer�ڵ��id
    ref_[heap_[i].id] = i;
    ref_[heap_[j].id] = j; 
}

// ��һ������ΪҪ�����Ľڵ��ڶ��е�λ�ã��ڶ�������Ϊ��ǰ�ѵĴ�С
bool HeapTimer::siftdown_(size_t index, size_t n){
    assert(index >= 0 && index < heap_.size());
    assert(n >= 0 && n < heap_size());

    size_t i = index;
    size_t j = i * 2 + 1;// j��i�������

    while (j < n){
        // i���Ҷ��Ӵ��ڣ����Ҷ���С������ӣ���jָ���Ҷ���
        if (j + 1 < n && heap_[j + 1] < heap_[j]) j ++;
        // Ҫ�����Ľڵ�ĳ�ʱʱ��С��jָ��ĳ�ʱʱ�䣬���õ�����ֱ��break 
        if (heap_[i] < heap_[j]) break;
        // ���򽻻������ڵ㣬Ȼ�����i��j��ֵ
        SwapNode_(i, j);
        i = j;
        j = i * 2 + 1;
    }
    return i > index;// ���������down�������򷵻�true
}


void HeapTimer::add(int id, int timeout, const TimeoutCallBack& cb){
    assert(id >= 0);

    size_t i;
    if (ref_.count(id) == 0) {
        // �½ڵ� �����β��������
        i = heap_.size();
        ref_[id] = i;// timernode��heap_�е�λ��
        heap_push_back({id, Clock::now() + MS(timeout), cb});
        siftup_(i);// ��Ϊ���ڶ�β���������ϵ���
    }
    else {
        // ���нڵ㣬������
        i = ref_[id];
        heap_[i].expires - Clock::now() + MS(timeout);// ������µĳ�ʱʱ��
        heap_[i].cb = cb;
        if (!siftdown_(i, heap_.size())){
            // û�з���down����������Ҫ����up����
            siftup_(i);
        }
    }
}

void HeapTimer::doWork(int id) {
    // ɾ��ָ��id�Ľڵ㣬�������ص�����
    if (heap_empty() || ref_.count(id) == 0) return;

    size_t i = ref_[id];// id�ڵ���heap_�ص��±�
    TimerNode node = heap_[i];
    node.cb();// �����ص�����
    del_(i);
}

void HeapTimer::del_(size_t index) {
    // ɾ��ָ��λ�õĽڵ�
    assert(!heap_.empty() && index >= 0 && index < heap_size());

    size_t i = index;
    size_t n = heap_.size() - 1;
    assert(i <= n);

    if (i < n){
        SwapNode_(i, n);// �Ͷ�βԪ�ؽ��н���
        // siftdown_�ĵڶ���������n�����ʾ�������һ���ڵ㲻�������
        if (!siftdown_(i, n)) {
            siftip_(i);
        }
    }
    // ɾ����βԪ��
    ref_.erase(heap_back().id);
    heap_.pop_back();
}

void HeapTimer::adjust(int id, int timeout){
    // ����ָ��id�Ľڵ�
    assert(!heap_.empty() && ref_.count(id) > 0);
    // ����ָ��id�Ľڵ��expires����
    heap_[ref_[id]].expires = Clock::now() + MS(timeout)
    // ����֮�󣬳�ʱʱ��϶���������Ҫ��down����
    siftdown_(ref_[id], heap_.size());
}

// ֻҪ�Ѳ�Ϊ�գ��ж϶Ѷ��Ƿ�ʱ�����Ѷ��Ķ�ʱ����ʱ����ִ�лص����������Ѷ�û�г�ʱ��ֱ��break������whileѭ��
void HeapTimer::tick(){
    // �����ʱ�ڵ�
    if (heap_empty()) return;

    while (!heap_.empty()) {
        TimerNode node = heap_.front();
        // ��ʱʱ���ȥ��ǰʱ���������0��˵��û�г�ʱ����ֱ��break
        if (std::duration_cast<MS>(node.expires - Clock::now()).count() > 0){
            break;
        }
        node.cb();
        pop();
    }
}

// �����Ѷ���Ԫ��
void HeapTimer::pop(){
    assert(!heap_.empty());
    del_(0);
}

// ���ref_��heap_
void HeapTimer::clear() {
    ref_.clear();
    heap_.clear();
}


int HeapTimer::GetNextTick(){
    tick();
}