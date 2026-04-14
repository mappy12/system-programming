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

Matrix read_matrix(const  std::string& filename) {
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

bool check_results(const double* C_seq, const double* C_par, int N) {
    const double epsilon = 1e-6;
    for (int i = 0; i < N * N; ++i) {
        if (std::abs(C_seq[i] - C_par[i]) > epsilon) {
            std::cout << "Mismatch at index " << i << ": Seq=" << C_seq[i] << ", Par=" << C_par[i] << std::endl;
            return false;
        }
    }
    return true;
}


