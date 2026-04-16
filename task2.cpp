#include <iostream>
#include <vector>
#include <chrono>
#include <pthread.h>
#include <stdexcept>
#include <algorithm>
#include <climits>
#include <cstdlib>

const char OPERATION = 'S';

const int TOTAL_SIZE = 10000000;
const int NUM_THREADS = 8;

struct ThreadArgs {
    const int* data;
    int* result;
    int num_threads;
    int thread_id;
    int block_size;
    int total_size;
    pthread_barrier_t* barrier;
};


int sequential_reduction(const std::vector<int>& data, char op) {
    if (data.empty()) return 0;
    int res = 0;
    if (op == 'S') {
        for (int v : data) res += v;
    } else if (op == 'X') {
        res = *std::max_element(data.begin(), data.end());
    } else if (op == 'N') {
        res = *std::min_element(data.begin(), data.end());
    }
    return res;
}


void* reduction_thread(void* arg) {
    ThreadArgs* args = static_cast<ThreadArgs*>(arg);
    int i = args->thread_id;
    int N = args->num_threads;

    int start_idx = i * args->block_size;
    int end_idx = (i == N - 1) ? args->total_size : (i + 1) * args->block_size;

    int local_val = 0;
    if (OPERATION == 'S') {
        local_val = 0;
        for (int k = start_idx; k < end_idx; ++k) {
            local_val += args->data[k];
        }
    } else if (OPERATION == 'X') {
        local_val = INT_MIN;
        for (int k = start_idx; k < end_idx; ++k) {
            if (args->data[k] > local_val) local_val = args->data[k];
        }
    } else if (OPERATION == 'N') {
        local_val = INT_MAX;
        for (int k = start_idx; k < end_idx; ++k) {
            if (args->data[k] < local_val) local_val = args->data[k];
        }
    }

    args->result[i] = local_val;

    int step = N;

    while (step > 1) {
        pthread_barrier_wait(args->barrier);

        int p = step;

        step = (step + 1) / 2;

        if (i + step >= p) {
            continue;
        }

        int partner_idx = i + step;
        int partner_val = args->result[partner_idx];

        if (OPERATION == 'S') {
            args->result[i] += partner_val;
        } else if (OPERATION == 'X') {
            if (partner_val > args->result[i]) {
                args->result[i] = partner_val;
            }
        } else if (OPERATION == 'N') {
            if (partner_val < args->result[i]) {
                args->result[i] = partner_val;
            }
        }
    }

    return nullptr;
}


int main() {
    try {
        std::cout << "Generating array of " << TOTAL_SIZE << " integers..." << std::endl;
        std::vector<int> data(TOTAL_SIZE);

        for (int i = 0; i < TOTAL_SIZE; ++i) {
            data[i] = (rand() % 201) - 100;
        }

        std::vector<int> result(NUM_THREADS, 0);

        pthread_barrier_t barrier;
        int rc = pthread_barrier_init(&barrier, nullptr, NUM_THREADS);
        if (rc != 0) throw std::runtime_error("Barrier init failed");

        std::vector<pthread_t> threads(NUM_THREADS);
        std::vector<ThreadArgs> args(NUM_THREADS);
        int block_size = TOTAL_SIZE / NUM_THREADS;

        std::cout << "Starting parallel reduction (" << NUM_THREADS << " threads)..." << std::endl;

        auto start_par = std::chrono::high_resolution_clock::now();

        for (int t = 0; t < NUM_THREADS; ++t) {
            args[t].data = data.data();
            args[t].result = result.data();
            args[t].num_threads = NUM_THREADS;
            args[t].thread_id = t;
            args[t].block_size = block_size;
            args[t].total_size = TOTAL_SIZE;
            args[t].barrier = &barrier;

            rc = pthread_create(&threads[t], nullptr, reduction_thread, &args[t]);
            if (rc != 0) throw std::runtime_error("Thread create failed");
        }

        for (int t = 0; t < NUM_THREADS; ++t) {
            pthread_join(threads[t], nullptr);
        }

        auto end_par = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff_par = end_par - start_par;

        int par_result = result[0];

        std::cout << "Starting sequential reduction..." << std::endl;
        auto start_seq = std::chrono::high_resolution_clock::now();
        int seq_result = sequential_reduction(data, OPERATION);
        auto end_seq = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff_seq = end_seq - start_seq;

        std::string op_name = (OPERATION == 'S') ? "Sum" : (OPERATION == 'X') ? "Max" : "Min";

        std::cout << "\nResults:" << std::endl;
        std::cout << "Sequential (" << op_name << "): " << seq_result << " in " << diff_seq.count() << " s" << std::endl;
        std::cout << "Parallel   (" << op_name << "): " << par_result << " in " << diff_par.count() << " s" << std::endl;

        if (seq_result == par_result) {
            std::cout << "SUCCESS: Results match!" << std::endl;
        } else {
            std::cout << "ERROR: Results DO NOT match!" << std::endl;
        }

        if (diff_par.count() > 0) {
            double speedup = diff_seq.count() / diff_par.count();
            std::cout << "Speedup: " << speedup << "x" << std::endl;
        }

        pthread_barrier_destroy(&barrier);

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}