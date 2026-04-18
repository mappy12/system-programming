#include <iostream>
#include <queue>
#include <optional>
#include <thread>
#include <chrono>
#include <pthread.h>
#include <vector>
#include <random>
#include <functional>

template <typename T>
class mt_queue {
private:
    std::queue<T> buffer;
    size_t max_size;

    pthread_mutex_t mutex;
    pthread_cond_t not_full;
    pthread_cond_t not_empty;

    bool is_finished;

public:
    explicit mt_queue(size_t max_sz) : max_size(max_sz), is_finished(false) {
        pthread_mutex_init(&mutex, nullptr);
        pthread_cond_init(&not_full, nullptr);
        pthread_cond_init(&not_empty, nullptr);
    }

    ~mt_queue() {
        pthread_mutex_destroy(&mutex);
        pthread_cond_destroy(&not_full);
        pthread_cond_destroy(&not_empty);
    }

    mt_queue(const mt_queue&) = delete;
    mt_queue& operator=(const mt_queue&) = delete;
    mt_queue(mt_queue&&) = delete;
    mt_queue& operator=(mt_queue&&) = delete;

    void enqueue(const T& v) {
        pthread_mutex_lock(&mutex);

        while (buffer.size() >= max_size && !is_finished) {
            pthread_cond_wait(&not_full, &mutex);
        }

        if (is_finished) {
            pthread_mutex_unlock(&mutex);
            return;
        }

        buffer.push(v);

        pthread_cond_signal(&not_empty);

        pthread_mutex_unlock(&mutex);
    }

    std::optional<T> dequeue() {
        pthread_mutex_lock(&mutex);

        while (buffer.empty() && !is_finished) {
            pthread_cond_wait(&not_empty, &mutex);
        }

        if (buffer.empty()) {
            pthread_mutex_unlock(&mutex);
            return std::nullopt;
        }

        T val = buffer.front();
        buffer.pop();

        pthread_cond_signal(&not_full);

        pthread_mutex_unlock(&mutex);
        return val;
    }

    bool full() const {
        pthread_mutex_lock(&const_cast<pthread_mutex_t&>(mutex));
        bool res = (buffer.size() >= max_size);
        pthread_mutex_unlock(&const_cast<pthread_mutex_t&>(mutex));
        return res;
    }

    bool empty() const {
        pthread_mutex_lock(&const_cast<pthread_mutex_t&>(mutex));
        bool res = buffer.empty();
        pthread_mutex_unlock(&const_cast<pthread_mutex_t&>(mutex));
        return res;
    }

    std::optional<T> try_dequeue() {
        pthread_mutex_lock(&mutex);
        std::optional<T> res;
        if (!buffer.empty()) {
            res = buffer.front();
            buffer.pop();
            pthread_cond_signal(&not_full);
        }
        pthread_mutex_unlock(&mutex);
        return res;
    }

    bool try_enqueue(const T& v) {
        pthread_mutex_lock(&mutex);
        bool success = false;
        if (buffer.size() < max_size) {
            buffer.push(v);
            pthread_cond_signal(&not_empty);
            success = true;
        }
        pthread_mutex_unlock(&mutex);
        return success;
    }

    void finish() {
        pthread_mutex_lock(&mutex);
        is_finished = true;

        pthread_cond_broadcast(&not_full);
        pthread_cond_broadcast(&not_empty);
        pthread_mutex_unlock(&mutex);
    }
};

void producer_thread(mt_queue<int>& queue, int id, int count) {
    for (int i = 0; i < count; ++i) {
        int val = id * 1000 + i;
        queue.enqueue(val);
        std::cout << "[P" << id << "] Produced: " << val << std::endl;
    }
    std::cout << "[Producer " << id << "] Finished producing." << std::endl;
}

void consumer_thread(mt_queue<int>& queue, int id) {
    while (true) {
        auto val_opt = queue.dequeue();
        if (!val_opt.has_value()) {
            break;
        }
        std::cout << "[C" << id << "] Consumed: " << val_opt.value() << std::endl;

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::cout << "[Consumer " << id << "] Finished consuming." << std::endl;
}

int main() {
    const int MAX_QUEUE_SIZE = 5;
    const int NUM_PRODUCERS = 3;
    const int NUM_CONSUMERS = 4;
    const int ITEMS_PER_PRODUCER = 10;

    mt_queue<int> queue(MAX_QUEUE_SIZE);

    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;

    std::cout << "Starting " << NUM_PRODUCERS << " producers and " << NUM_CONSUMERS << " consumers..." << std::endl;

    for (int i = 0; i < NUM_CONSUMERS; ++i) {
        consumers.emplace_back(consumer_thread, std::ref(queue), i);
    }

    for (int i = 0; i < NUM_PRODUCERS; ++i) {
        producers.emplace_back(producer_thread, std::ref(queue), i, ITEMS_PER_PRODUCER);
    }

    for (auto& t : producers) {
        t.join();
    }
    std::cout << "All producers finished. Signaling completion..." << std::endl;

    queue.finish();

    for (auto& t : consumers) {
        t.join();
    }
    std::cout << "All consumers finished. Program exiting." << std::endl;

    return 0;
}