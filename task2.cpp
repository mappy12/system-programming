#include <iostream>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <vector>
#include <chrono>
#include <pthread.h>
#include <stdexcept>
#include <algorithm>
#include <climits>
#include <cstdlib>
#include <cstring>

const char DEFAULT_OPERATION = 'S';
const std::string DEFAULT_FILE = "../files/search.bin";
const int DEFAULT_NUM_THREADS = 8;

struct ThreadArgs {
    const int* data;
    long long* result;
    int num_threads;
    int thread_id;
    int block_size;
    int total_size;
    pthread_barrier_t* barrier;
    char operation;
};

std::vector<int> read_array(const std::string& filename) {
    if (!std::filesystem::exists(filename)) {
        throw std::runtime_error("File does not exist: " + filename);
    }

    uintmax_t size = std::filesystem::file_size(filename);

    if (size % sizeof(int) != 0) {
        throw std::runtime_error("Invalid file size for int array: " + filename);
    }

    const int count = size / sizeof(int);
    std::cout << "Read " << count << " integers from " << filename << std::endl;

    std::vector<int> data(count);

    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filename);
    }

    file.read(reinterpret_cast<char*>(data.data()), size);
    if (!file) {
        throw std::runtime_error("Error reading file: " + filename);
    }

    return data;
}

long long sequential_reduction(const std::vector<int>& data, char op) {
    if (data.empty()) return 0;

    long long res = 0;

    if (op == 'S') {
        for (int v : data) res += v;
    } else if (op == 'X') {
        res = INT_MIN;
        for (int v : data) {
            if (v > res) res = v;
        }
    } else if (op == 'N') {
        res = INT_MAX;
        for (int v : data) {
            if (v < res) res = v;
        }
    }
    return res;
}


void* reduction_thread(void* arg) {
    ThreadArgs* args = static_cast<ThreadArgs*>(arg);
    int i = args->thread_id;
    int N = args->num_threads;
    char op = args->operation;

    int start_idx = i * args->block_size;
    int end_idx = (i == N - 1) ? args->total_size : (i + 1) * args->block_size;

    long long local_val = 0;
    if (op == 'S') {
        local_val = 0;
        for (int k = start_idx; k < end_idx; ++k) {
            local_val += args->data[k];
        }
    } else if (op == 'X') {
        local_val = INT_MIN;
        for (int k = start_idx; k < end_idx; ++k) {
            if (args->data[k] > local_val) local_val = args->data[k];
        }
    } else if (op == 'N') {
        local_val = INT_MAX;
        for (int k = start_idx; k < end_idx; ++k) {
            if (args->data[k] < local_val) local_val = args->data[k];
        }
    }

    args->result[i] = local_val;

    int step = N;

    while (step > 1) {
        int rc = pthread_barrier_wait(args->barrier);

        if (rc != 0 && rc != PTHREAD_BARRIER_SERIAL_THREAD) {
            std::cerr << "Barrier wait error: " << strerror(rc) << std::endl;
            return nullptr;
        }

        int p = step;
        step = (step + 1) / 2;

        if (i + step >= p) {
            continue;
        }

        const int partner_idx = i + step;
        const long long partner_val = args->result[partner_idx];

        if (op == 'S') {
            args->result[i] += partner_val;
        } else if (op == 'X') {
            if (partner_val > args->result[i]) {
                args->result[i] = partner_val;
            }
        } else if (op == 'N') {
            if (partner_val < args->result[i]) {
                args->result[i] = partner_val;
            }
        }
    }

    return nullptr;
}


int main(int argc, char* argv[]) {
    try {
        std::string filename = DEFAULT_FILE;
        char operation = DEFAULT_OPERATION;
        int num_threads = DEFAULT_NUM_THREADS;

        if (argc >= 2) filename = argv[1];
        if (argc >= 3) operation = argv[2][0];
        if (argc >= 4) num_threads = std::stoi(argv[3]);

        operation = toupper(operation);

        std::cout << "Reading array from: " << filename << std::endl;
        std::vector<int> data = read_array(filename);
        std::cout << "Array size: " << data.size() << " elements." << std::endl;

        std::vector<long long> result(num_threads, 0);

        pthread_barrier_t barrier;
        int rc = pthread_barrier_init(&barrier, nullptr, num_threads);
        if (rc != 0) throw std::runtime_error("Barrier init failed");

        std::vector<pthread_t> threads(num_threads);
        std::vector<ThreadArgs> args(num_threads);
        const int block_size = data.size() / num_threads;

        std::cout << "Starting parallel reduction (" << num_threads << " threads, Operation: "
            << operation << ")..." << std::endl;

        auto start_par = std::chrono::high_resolution_clock::now();

        for (int t = 0; t < num_threads; ++t) {
            args[t].data = data.data();
            args[t].result = result.data();
            args[t].num_threads = num_threads;
            args[t].thread_id = t;
            args[t].block_size = block_size;
            args[t].total_size = data.size();
            args[t].barrier = &barrier;
            args[t].operation = operation;

            rc = pthread_create(&threads[t], nullptr, reduction_thread, &args[t]);
            if (rc != 0) throw std::runtime_error("Thread create failed");
        }

        for (int t = 0; t < num_threads; ++t) {
            rc = pthread_join(threads[t], nullptr);

            if (rc != 0) throw std::runtime_error("Thread join failed");
        }

        auto end_par = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff_par = end_par - start_par;

        long long par_result = result[0];

        std::cout << "Starting sequential reduction..." << std::endl;
        auto start_seq = std::chrono::high_resolution_clock::now();
        long long seq_result = sequential_reduction(data, operation);
        auto end_seq = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff_seq = end_seq - start_seq;

        std::string op_name = (operation == 'S') ? "Sum" : (operation == 'X') ? "Max" : "Min";

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
