#include "alignment_internal.hpp"

#include <cuda_runtime.h>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

static void cuda_check(cudaError_t error, const char* operation) {
    if (error != cudaSuccess) {
        throw std::runtime_error(
            std::string(operation) + ": " + cudaGetErrorString(error)
        );
    }
}

template <typename T>
class DeviceBuffer {
public:
    T* data = nullptr;

    explicit DeviceBuffer(std::size_t count) {
        if (count > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
            throw std::overflow_error("CUDA allocation size overflow.");
        }
        if (count > 0) {
            cuda_check(
                cudaMalloc(reinterpret_cast<void**>(&data), count * sizeof(T)),
                "cudaMalloc"
            );
        }
    }

    ~DeviceBuffer() {
        if (data != nullptr) cudaFree(data);
    }

    DeviceBuffer(const DeviceBuffer&) = delete;
    DeviceBuffer& operator=(const DeviceBuffer&) = delete;
};

__global__ void wavefront_kernel(
    const char* a,
    const char* b,
    int* h,
    int* e,
    int* f,
    std::size_t cols,
    std::size_t diagonal,
    std::size_t first_row,
    std::size_t count,
    Scoring scoring
) {
    const std::size_t offset =
        static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (offset >= count) return;

    const std::size_t i = first_row + offset;
    const std::size_t j = diagonal - i;
    const std::size_t cell = i * cols + j;
    const std::size_t up = (i - 1) * cols + j;
    const std::size_t left = cell - 1;

    e[cell] = max(
        h[up] - scoring.gap_open,
        e[up] - scoring.gap_extend
    );
    f[cell] = max(
        h[left] - scoring.gap_open,
        f[left] - scoring.gap_extend
    );

    const int substitution =
        a[i - 1] == b[j - 1] ? scoring.match : scoring.mismatch;

    h[cell] = max(
        max(0, h[up - 1] + substitution),
        max(e[cell], f[cell])
    );
}

__global__ void tiled_kernel(
    const char* a,
    const char* b,
    int* h,
    int* e,
    int* f,
    std::size_t length_a,
    std::size_t length_b,
    std::size_t tile_size,
    std::size_t tile_diagonal,
    std::size_t first_tile_row,
    Scoring scoring
) {
    const std::size_t tile_row = first_tile_row + blockIdx.x;
    const std::size_t tile_col = tile_diagonal - tile_row;
    const std::size_t begin_i = tile_row * tile_size + 1;
    const std::size_t begin_j = tile_col * tile_size + 1;
    const std::size_t height =
        min(tile_size, length_a - begin_i + 1);
    const std::size_t width =
        min(tile_size, length_b - begin_j + 1);

    const std::size_t cols = length_b + 1;
    const std::size_t stride = tile_size + 1;
    const std::size_t plane = stride * stride;

    extern __shared__ int storage[];
    int* sh = storage;
    int* se = sh + plane;
    int* sf = se + plane;

    // Load the top boundary, including the top-left corner.
    for (std::size_t j = threadIdx.x; j <= width; j += blockDim.x) {
        const std::size_t global =
            (begin_i - 1) * cols + begin_j - 1 + j;
        sh[j] = h[global];
        se[j] = e[global];
        sf[j] = f[global];
    }

    // Load the left boundary.
    for (std::size_t i = threadIdx.x + 1;
         i <= height; i += blockDim.x) {
        const std::size_t local = i * stride;
        const std::size_t global =
            (begin_i + i - 1) * cols + begin_j - 1;
        sh[local] = h[global];
        se[local] = e[global];
        sf[local] = f[global];
    }
    __syncthreads();

    for (std::size_t diagonal = 2;
         diagonal <= height + width; ++diagonal) {
        const std::size_t i = threadIdx.x + 1;
        if (i <= height && diagonal > i) {
            const std::size_t j = diagonal - i;
            if (j <= width) {
                const std::size_t cell = i * stride + j;
                const std::size_t up = cell - stride;
                const std::size_t left = cell - 1;

                se[cell] = max(
                    sh[up] - scoring.gap_open,
                    se[up] - scoring.gap_extend
                );
                sf[cell] = max(
                    sh[left] - scoring.gap_open,
                    sf[left] - scoring.gap_extend
                );

                const int substitution =
                    a[begin_i + i - 2] == b[begin_j + j - 2]
                    ? scoring.match : scoring.mismatch;

                sh[cell] = max(
                    max(0, sh[up - 1] + substitution),
                    max(se[cell], sf[cell])
                );
            }
        }
        __syncthreads();
    }

    for (std::size_t offset = threadIdx.x;
         offset < height * width; offset += blockDim.x) {
        const std::size_t i = offset / width + 1;
        const std::size_t j = offset % width + 1;
        const std::size_t local = i * stride + j;
        const std::size_t global =
            (begin_i + i - 1) * cols + begin_j + j - 1;

        h[global] = sh[local];
        e[global] = se[local];
        f[global] = sf[local];
    }
}

static Alignment align_cuda_impl(
    const std::string& a,
    const std::string& b,
    const Scoring& scoring,
    std::size_t tile_size
) {
    validate_alignment_inputs(a, b, scoring);
    if (a.empty() || b.empty()) return Alignment{};

    const std::size_t rows = a.size() + 1;
    const std::size_t cols = b.size() + 1;
    const std::size_t cells = rows * cols;
    const int negative_infinity =
        -std::numeric_limits<int>::max() / 4;

    std::vector<int> h(cells, 0);
    std::vector<int> e(cells, negative_infinity);
    std::vector<int> f(cells, negative_infinity);
    const std::size_t bytes = cells * sizeof(int);

    DeviceBuffer<char> device_a(a.size());
    DeviceBuffer<char> device_b(b.size());
    DeviceBuffer<int> device_h(cells);
    DeviceBuffer<int> device_e(cells);
    DeviceBuffer<int> device_f(cells);

    cuda_check(cudaMemcpy(
        device_a.data, a.data(), a.size(), cudaMemcpyHostToDevice
    ), "Copy sequence 1");

    cuda_check(cudaMemcpy(
        device_b.data, b.data(), b.size(), cudaMemcpyHostToDevice
    ), "Copy sequence 2");

    cuda_check(cudaMemcpy(
        device_h.data, h.data(), bytes, cudaMemcpyHostToDevice
    ), "Initialize H");

    cuda_check(cudaMemcpy(
        device_e.data, e.data(), bytes, cudaMemcpyHostToDevice
    ), "Initialize E");

    cuda_check(cudaMemcpy(
        device_f.data, f.data(), bytes, cudaMemcpyHostToDevice
    ), "Initialize F");

    if (tile_size == 0) {
    constexpr unsigned int threads = 256;

    for (std::size_t diagonal = 2;
         diagonal < rows + cols - 1; ++diagonal) {
        const std::size_t first =
            diagonal > b.size() ? diagonal - b.size() : 1;
        const std::size_t last = std::min(a.size(), diagonal - 1);
        const std::size_t count = last - first + 1;
        const unsigned int blocks =
            static_cast<unsigned int>((count + threads - 1) / threads);

        wavefront_kernel<<<blocks, threads>>>(
            device_a.data, device_b.data,
            device_h.data, device_e.data, device_f.data,
            cols, diagonal, first, count, scoring
        );
        cuda_check(cudaGetLastError(), "Launch wavefront kernel");
    }

    } else {
        const std::size_t tile_rows =
            a.size() / tile_size + (a.size() % tile_size != 0);
        const std::size_t tile_cols =
            b.size() / tile_size + (b.size() % tile_size != 0);
        const std::size_t shared_bytes =
            3 * (tile_size + 1) * (tile_size + 1) * sizeof(int);

        for (std::size_t diagonal = 0;
             diagonal < tile_rows + tile_cols - 1; ++diagonal) {
            const std::size_t first =
                diagonal >= tile_cols ? diagonal - tile_cols + 1 : 0;
            const std::size_t last = std::min(tile_rows - 1, diagonal);
            const unsigned int blocks =
                static_cast<unsigned int>(last - first + 1);

            tiled_kernel<<<blocks, static_cast<unsigned int>(tile_size),
                           shared_bytes>>>(
                device_a.data, device_b.data,
                device_h.data, device_e.data, device_f.data,
                a.size(), b.size(), tile_size, diagonal, first, scoring
            );
            cuda_check(cudaGetLastError(), "Launch tiled kernel");
        }
    }

    cuda_check(cudaDeviceSynchronize(), "Execute CUDA kernels");

    cuda_check(cudaMemcpy(
        h.data(), device_h.data, bytes, cudaMemcpyDeviceToHost
    ), "Read H");

    cuda_check(cudaMemcpy(
        e.data(), device_e.data, bytes, cudaMemcpyDeviceToHost
    ), "Read E");

    cuda_check(cudaMemcpy(
        f.data(), device_f.data, bytes, cudaMemcpyDeviceToHost
    ), "Read F");

    return traceback_alignment(a, b, scoring, h, e, f);
}


Alignment align_cuda(
    const std::string& a,
    const std::string& b,
    const Scoring& scoring
) {
    return align_cuda_impl(a, b, scoring, 0);
}

Alignment align_cuda_tiled(
    const std::string& a,
    const std::string& b,
    const Scoring& scoring,
    std::size_t tile_size
) {
    if (tile_size == 0 || tile_size > 32) {
        throw std::invalid_argument("CUDA tile size must be between 1 and 32.");
    }
    return align_cuda_impl(a, b, scoring, tile_size);
}
