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