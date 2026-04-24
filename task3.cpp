#include <iostream>
#include <queue>
#include <optional>
#include <pthread.h>
#include <cstring>
#include <vector>

const int MAX_QUEUE_SIZE = 5;
const int NUM_PRODUCERS = 3;
const int NUM_CONSUMERS = 1;
const int ITEMS_PER_PRODUCER = 10;

pthread_mutex_t cout_mutex = PTHREAD_MUTEX_INITIALIZER;

template <typename T>
class mt_queue {
private:
    std::queue<T> q_;
    size_t max_size;

    mutable pthread_mutex_t mutex;
    pthread_cond_t not_full;
    pthread_cond_t not_empty;

    bool is_finished;

    static void check_pthread(int rc, const char* msg) {
        if (rc != 0) {
            throw std::runtime_error(std::string(msg));
        }
    }

public:
    explicit mt_queue(size_t max_sz) : max_size(max_sz), is_finished(false) {
        check_pthread(pthread_mutex_init(&mutex, nullptr), "Mutex init failed");
        check_pthread(pthread_cond_init(&not_full, nullptr), "Cond not_full init failed");
        check_pthread(pthread_cond_init(&not_empty, nullptr), "Cond not_empty init failed");
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
        check_pthread(pthread_mutex_lock(&mutex), "Enqueue lock failed");

        while (q_.size() >= max_size && !is_finished) {
            check_pthread(pthread_cond_wait(&not_full, &mutex), "Enqueue wait failed");
        }

        if (is_finished) {
            pthread_mutex_unlock(&mutex);
            return;
        }

        q_.push(v);

        check_pthread(pthread_cond_signal(&not_empty), "Enqueue signal failed");
        check_pthread(pthread_mutex_unlock(&mutex), "Enqueue unlock failed");
    }

   T dequeue() {
        check_pthread(pthread_mutex_lock(&mutex), "Dequeue lock failed");

        while (q_.empty() && !is_finished) {
            check_pthread(pthread_cond_wait(&not_empty, &mutex), "Dequeue wait failed");
        }

        if (q_.empty()) {
            pthread_mutex_unlock(&mutex);
            throw std::runtime_error("Queue is finished and empty");
        }

        T val = q_.front();
        q_.pop();

        check_pthread(pthread_cond_signal(&not_full), "Dequeue signal failed");
        check_pthread(pthread_mutex_unlock(&mutex), "Dequeue unlock failed");

        return val;
    }

    bool full() const {
        check_pthread(pthread_mutex_lock(&mutex), "Full lock failed");
        const bool res = (q_.size() >= max_size);
        pthread_mutex_unlock(&mutex);
        return res;
    }

    bool empty() const {
        check_pthread(pthread_mutex_lock(&mutex), "Empty lock failed");
        const bool res = q_.empty();
        pthread_mutex_unlock(&mutex);
        return res;
    }

    std::optional<T> try_dequeue() {
        check_pthread(pthread_mutex_lock(&mutex), "Try dequeue lock failed");
        std::optional<T> res;
        if (!q_.empty()) {
            res = q_.front();
            q_.pop();
            pthread_cond_signal(&not_full);
        }
        pthread_mutex_unlock(&mutex);
        return res;
    }

    bool try_enqueue(const T& v) {
        check_pthread(pthread_mutex_lock(&mutex), "Try enqueue lock failed");
        bool success = false;

        if (q_.size() < max_size) {
            q_.push(v);
            pthread_cond_signal(&not_empty);
            success = true;
        }

        pthread_mutex_unlock(&mutex);
        return success;
    }

    void finish() {
        check_pthread(pthread_mutex_lock(&mutex), "Finish lock failed");

        is_finished = true;

        pthread_cond_broadcast(&not_full);
        pthread_cond_broadcast(&not_empty);
        pthread_mutex_unlock(&mutex);
    }
};

struct ProducerArgs {
    mt_queue<int>* queue;
    int id;
    int count;
};

struct ConsumerArgs {
    mt_queue<int>* queue;
    int id;
};

void* producer_thread(void* arg) {
    ProducerArgs* args = static_cast<ProducerArgs*>(arg);

    try {
        for (int i = 0; i < args->count; ++i) {
            int val = args->id * 1000 + i;
            pthread_mutex_lock(&cout_mutex);
            std::cout << "[P" << args->id << "] Produced: " << val << std::endl;
            pthread_mutex_unlock(&cout_mutex);
            args->queue->enqueue(val);
        }
        pthread_mutex_lock(&cout_mutex);
        std::cout << "[Producer " << args->id << "] Finished." << std::endl;
        pthread_mutex_unlock(&cout_mutex);
    } catch (const std::exception& e) {
        std::cerr << "Producer error: " << e.what() << std::endl;
    }
    return nullptr;
}

void* consumer_thread(void* arg) {
    ConsumerArgs* args = static_cast<ConsumerArgs*>(arg);
    try {
        while (true) {
            int val = args->queue->dequeue();
            pthread_mutex_lock(&cout_mutex);
            std::cout << "[C" << args->id << "] Consumed: " << val << std::endl;
            pthread_mutex_unlock(&cout_mutex);
        }
    } catch (const std::runtime_error& e) {
        pthread_mutex_lock(&cout_mutex);
        std::cout << "[Consumer " << args->id << "] Finished (Queue closed)." << std::endl;
        pthread_mutex_unlock(&cout_mutex);
    } catch (const std::exception& e) {
        std::cerr << "Consumer error: " << e.what() << std::endl;
    }
    return nullptr;
}

int main() {
    mt_queue<int> queue(MAX_QUEUE_SIZE);

    std::vector<pthread_t> producers(NUM_PRODUCERS);
    std::vector<pthread_t> consumers(NUM_CONSUMERS);

    std::vector<ProducerArgs> prod_args(NUM_PRODUCERS);
    std::vector<ConsumerArgs> cons_args(NUM_CONSUMERS);

    std::cout << "Starting " << NUM_PRODUCERS << " producers and " << NUM_CONSUMERS << " consumers..." << std::endl;

    for (int i = 0; i < NUM_CONSUMERS; ++i) {
        cons_args[i].queue = &queue;
        cons_args[i].id = i;
        pthread_create(&consumers[i], nullptr, consumer_thread, &cons_args[i]);
    }

    for (int i = 0; i < NUM_PRODUCERS; ++i) {
        prod_args[i].queue = &queue;
        prod_args[i].id = i;
        prod_args[i].count = ITEMS_PER_PRODUCER;
        pthread_create(&producers[i], nullptr, producer_thread, &prod_args[i]);
    }

    for (int i = 0; i < NUM_PRODUCERS; ++i) {
        pthread_join(producers[i], nullptr);
    }

    std::cout << "All producers finished. Signaling completion..." << std::endl;

    queue.finish();

    for (int i = 0; i < NUM_CONSUMERS; ++i) {
        pthread_join(consumers[i], nullptr);
    }

    std::cout << "All consumers finished. Exit." << std::endl;

    return 0;
}
