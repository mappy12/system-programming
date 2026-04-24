#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <cmath>
#include <string>
#include <chrono>
#include <iomanip>
#include <pthread.h>
#include <stdexcept>

struct Matrix {
    std::vector<double> data;
    int N;
};

struct ThreadArgs {
    const double* A;
    const double* B;
    double* C;
    int N;
    int start_row;
    int end_row;
};


Matrix read_matrix(const std::string& filename) {
    if (!std::filesystem::exists(filename)) {
        throw std::runtime_error("File does not exist: " + filename);
    }

    uintmax_t size = std::filesystem::file_size(filename);

    if (size % sizeof(double) != 0) {
        throw std::runtime_error("Invalid file size for double matrix: " + filename);
    }

    const int total_elements = static_cast<int>(size / sizeof(double));
    const int N = static_cast<int>(std::sqrt(total_elements));

    Matrix matrix;
    matrix.N = N;
    matrix.data.resize(total_elements);

    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filename);
    }

    file.read(reinterpret_cast<char*>(matrix.data.data()), size);
    if (!file) {
        throw std::runtime_error("Error reading file: " + filename);
    }

    return matrix;
}


void seq_mult_matrix(const double* A, const double* B, double* C, const int N) {
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            double sum = 0.0;
            for (int k = 0; k < N; ++k) {
                sum += A[i * N + k] * B[k * N + j];
            }
            C[i * N + j] = sum;
        }
    }
}


void* threads_mult_matrix(void* arg) {
    ThreadArgs* args = static_cast<ThreadArgs*>(arg);
    const int N = args->N;

    for (int i = args->start_row; i < args->end_row; ++i) {
        for (int j = 0; j < N; ++j) {
            double sum = 0.0;
            for (int k = 0; k < N; ++k) {
                sum += args->A[i * N + k] * args->B[k * N + j];
            }
            args->C[i * N + j] = sum;
        }
    }
    return nullptr;
}


void run_parallel_mult(const Matrix& matA, const Matrix& matB, std::vector<double>& C_par, int num_threads) {
    int N = matA.N;
    std::vector<pthread_t> threads(num_threads);
    std::vector<ThreadArgs> args(num_threads);

    int rows_per_thread = N / num_threads;
    int current_row = 0;

    for (int t = 0; t < num_threads; ++t) {
        args[t].A = matA.data.data();
        args[t].B = matB.data.data();
        args[t].C = C_par.data();
        args[t].N = N;
        args[t].start_row = current_row;

        if (t == num_threads - 1) {
            args[t].end_row = N;
        } else {
            args[t].end_row = current_row + rows_per_thread;
        }
        current_row = args[t].end_row;

        int rc = pthread_create(&threads[t], nullptr, threads_mult_matrix, &args[t]);
        if (rc != 0) {
            throw std::runtime_error("Error creating thread: " + std::to_string(rc));
        }
    }

    for (int t = 0; t < num_threads; ++t) {
        int rc = pthread_join(threads[t], nullptr);
        if (rc != 0) {
            throw std::runtime_error("pthread_join failed with code: " + std::to_string(rc));
        }
    }
}

bool check_results(const double* C_seq, const double* C_par, const int N) {
    const double epsilon = 1e-6;
    for (int i = 0; i < N * N; ++i) {
        if (std::abs(C_seq[i] - C_par[i]) > epsilon) {
            std::cout << "Mismatch at index " << i << ": Seq=" << C_seq[i] << ", Par=" << C_par[i] << std::endl;
            return false;
        }
    }
    return true;
}


void print_matrix(const double* data, int N, const std::string& name) {
    std::cout << "Matrix " << name << " (" << N << "x" << N << "):" << std::endl;
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            std::cout << data[i * N + j] << " ";
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;
}


int main() {
    try {
        const std::string fileA_small = "files/small_matrix1.bin";
        const std::string fileB_small = "files/small_matrix2.bin";
        const std::string fileA_large = "files/big_matrix1.bin";
        const std::string fileB_large = "files/big_matrix2.bin";

        std::cout << "=== TASK 1: SMALL MATRICES ===" << std::endl;

        Matrix matA_s = read_matrix(fileA_small);
        Matrix matB_s = read_matrix(fileB_small);
        int N_s = matA_s.N;

        std::cout << "Small matrices size N = " << N_s << std::endl;

        std::vector<double> C_seq_s(N_s * N_s, 0.0);
        std::vector<double> C_par_s(N_s * N_s, 0.0);

        auto start_seq_s = std::chrono::high_resolution_clock::now();
        seq_mult_matrix(matA_s.data.data(), matB_s.data.data(), C_seq_s.data(), N_s);
        auto end_seq_s = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff_seq_s = end_seq_s - start_seq_s;

        int threads_s = (N_s < 100) ? 2 : 4;
        auto start_par_s = std::chrono::high_resolution_clock::now();
        run_parallel_mult(matA_s, matB_s, C_par_s, threads_s);
        auto end_par_s = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff_par_s = end_par_s - start_par_s;

        std::cout << "Seq time: " << diff_seq_s.count() << " s" << std::endl;
        std::cout << "Par time: " << diff_par_s.count() << " s" << std::endl;

        if (check_results(C_seq_s.data(), C_par_s.data(), N_s)) {
            std::cout << "Results match!" << std::endl;
        } else {
            std::cout << "Results DO NOT match!" << std::endl;
        }

        if (N_s <= 10) {
            print_matrix(matA_s.data.data(), N_s, "A");
            print_matrix(matB_s.data.data(), N_s, "B");
            print_matrix(C_seq_s.data(), N_s, "Result (Seq)");
            print_matrix(C_par_s.data(), N_s, "Result (Par)");
        }

        std::cout << "\n=== TASK 1: LARGE MATRICES ===" << std::endl;

        Matrix matA_l = read_matrix(fileA_large);
        Matrix matB_l = read_matrix(fileB_large);
        int N_l = matA_l.N;

        std::cout << "Large matrices size N = " << N_l << std::endl;

        std::vector<double> C_seq_l(N_l * N_l, 0.0);
        std::vector<double> C_par_l(N_l * N_l, 0.0);

        std::cout << "Starting sequential multiplication..." << std::endl;
        auto start_seq_l = std::chrono::high_resolution_clock::now();
        seq_mult_matrix(matA_l.data.data(), matB_l.data.data(), C_seq_l.data(), N_l);
        auto end_seq_l = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff_seq_l = end_seq_l - start_seq_l;
        std::cout << "Sequential time: " << diff_seq_l.count() << " s" << std::endl;

        int threads_l = 8;
        std::cout << "Starting parallel multiplication (" << threads_l << " threads)..." << std::endl;
        auto start_par_l = std::chrono::high_resolution_clock::now();
        run_parallel_mult(matA_l, matB_l, C_par_l, threads_l);
        auto end_par_l = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff_par_l = end_par_l - start_par_l;
        std::cout << "Parallel time:   " << diff_par_l.count() << " s" << std::endl;

        std::cout << "Checking results..." << std::endl;
        if (check_results(C_seq_l.data(), C_par_l.data(), N_l)) {
            std::cout << "Results match!" << std::endl;
        } else {
            std::cout << "Results DO NOT match!" << std::endl;
        }

        if (diff_par_l.count() > 0) {
            double speedup = diff_seq_l.count() / diff_par_l.count();
            std::cout << "Speedup: " << speedup << "x" << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
