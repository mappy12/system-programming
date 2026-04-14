#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <string>
#include <chrono>
#include <iomanip>
#include <pthread.h>
#include <stdexcept>
#include <algorithm>

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
    std::ifstream file(filename, std::ios::binary | std::ios::ate);

    if (!file.is_open()) throw std::runtime_error("Cannot open file: " + filename);

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (size % sizeof(double) != 0) {
        throw std::runtime_error("Invalid file size for double matrix: " + filename);
    }

    const int total_elements = static_cast<int>(size / sizeof(double));
    const int N = static_cast<int>(std::sqrt(total_elements));

    Matrix matrix;
    matrix.N = N;
    matrix.data.resize(total_elements);

    if (!file.read(reinterpret_cast<char*>(matrix.data.data()), size)) {
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
    const std::string fileA = "matrixA.bin";
    const std::string fileB = "matrixB.bin";

    Matrix matrixA = read_matrix(fileA);
    Matrix matrixB = read_matrix(fileB);

    const int N = matrixA.N;
    std::cout << "Matrix size N = " << N << std::endl;

    std::vector<double> C_seq(N * N, 0.0);
    std::vector<double> C_par(N * N, 0.0);

    std::cout << "Starting sequential multiplication..." << std::endl;
    const auto start_seq = std::chrono::high_resolution_clock::now();
    seq_mult_matrix(matrixA.data.data(), matrixB.data.data(), C_seq.data(), N);
    const auto end_seq = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff_seq = end_seq - start_seq;
    std::cout << "Sequential time: " << diff_seq.count() << " seconds" << std::endl;

    std::cout << "Starting parallel multiplication..." << std::endl;

    int num_threads = 4;
    if (N < 100) num_threads = 2;

    std::vector<pthread_t> threads(num_threads);
    std::vector<ThreadArgs> args(num_threads);

    const auto start_par = std::chrono::high_resolution_clock::now();

    int rows_per_thread = N / num_threads;
    int current_row = 0;
    for (int t = 0; t < num_threads; ++t) {
        args[t].A = matrixA.data.data();
        args[t].B = matrixB.data.data();
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
        if (rc) {
            throw std::runtime_error("Error creating thread: " + std::to_string(rc));
        }
    }

    for (int t = 0; t < num_threads; ++t) {
        pthread_join(threads[t], nullptr);
    }

    const auto end_par = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff_par = end_par - start_par;
    std::cout << "Parallel time (" << num_threads << " threads): " << diff_par.count() << " seconds" << std::endl;

    std::cout << "Checking results..." << std::endl;
    if (check_results(C_seq.data(), C_par.data(), N)) {
        std::cout << "Results match!" << std::endl;
    } else {
        std::cout << "Results DO NOT match!" << std::endl;
    }

    if (N <= 10) {
        print_matrix(matrixA.data.data(), N, "A");
        print_matrix(matrixB.data.data(), N, "B");
        print_matrix(C_seq.data(), N, "Result (Seq)");
        print_matrix(C_par.data(), N, "Result (Par)");
    }

    return 0;
}
