/* ************************************************************************
 * Copyright (C) 2016-2023 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * ************************************************************************ */

#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <vector>

#include "testing_common.hpp"

/* ============================================================================================ */

using hipblasSyrStridedBatchedModel
    = ArgumentModel<e_a_type, e_uplo, e_N, e_alpha, e_incx, e_lda, e_stride_scale, e_batch_count>;

inline void testname_syr_strided_batched(const Arguments& arg, std::string& name)
{
    hipblasSyrStridedBatchedModel{}.test_name(arg, name);
}

template <typename T>
void testing_syr_strided_batched(const Arguments& arg)
{
    bool FORTRAN = arg.api == hipblas_client_api::FORTRAN;
    auto hipblasSyrStridedBatchedFn
        = FORTRAN ? hipblasSyrStridedBatched<T, true> : hipblasSyrStridedBatched<T, false>;

    hipblasFillMode_t uplo         = char2hipblas_fill(arg.uplo);
    int               N            = arg.N;
    int               incx         = arg.incx;
    int               lda          = arg.lda;
    double            stride_scale = arg.stride_scale;
    int               batch_count  = arg.batch_count;

    int abs_incx = incx < 0 ? -incx : incx;

    hipblasStride strideA = size_t(lda) * N * stride_scale;
    hipblasStride stridex = size_t(abs_incx) * N * stride_scale;
    size_t        A_size  = strideA * batch_count;
    size_t        x_size  = stridex * batch_count;

    hipblasLocalHandle handle(arg);

    // argument sanity check, quick return if input parameters are invalid before allocating invalid
    // memory
    bool invalid_size = N < 0 || !incx || lda < N || lda < 1 || batch_count < 0;
    if(invalid_size || !N || !batch_count)
    {
        hipblasStatus_t actual = hipblasSyrStridedBatchedFn(
            handle, uplo, N, nullptr, nullptr, incx, stridex, nullptr, lda, strideA, batch_count);
        EXPECT_HIPBLAS_STATUS2(
            actual, (invalid_size ? HIPBLAS_STATUS_INVALID_VALUE : HIPBLAS_STATUS_SUCCESS));
        return;
    }

    // Naming: dK is in GPU (device) memory. hK is in CPU (host) memory
    host_vector<T> hA(A_size);
    host_vector<T> hA_cpu(A_size);
    host_vector<T> hA_host(A_size);
    host_vector<T> hA_device(A_size);
    host_vector<T> hx(x_size);

    device_vector<T> dA(A_size);
    device_vector<T> dx(x_size);
    device_vector<T> d_alpha(1);

    T h_alpha = arg.get_alpha<T>();

    double gpu_time_used, hipblas_error_host, hipblas_error_device;

    // Initial Data on CPU
    hipblas_init_matrix(
        hA, arg, N, N, lda, strideA, batch_count, hipblas_client_never_set_nan, true);
    hipblas_init_vector(
        hx, arg, N, abs_incx, stridex, batch_count, hipblas_client_alpha_sets_nan, false, true);
    hA_cpu = hA;

    // copy data from CPU to device
    ASSERT_HIP_SUCCESS(hipMemcpy(dA, hA.data(), sizeof(T) * A_size, hipMemcpyHostToDevice));
    ASSERT_HIP_SUCCESS(hipMemcpy(dx, hx.data(), sizeof(T) * x_size, hipMemcpyHostToDevice));
    ASSERT_HIP_SUCCESS(hipMemcpy(d_alpha, &h_alpha, sizeof(T), hipMemcpyHostToDevice));

    if(arg.unit_check || arg.norm_check)
    {
        /* =====================================================================
            HIPBLAS
        =================================================================== */
        ASSERT_HIPBLAS_SUCCESS(hipblasSetPointerMode(handle, HIPBLAS_POINTER_MODE_HOST));
        ASSERT_HIPBLAS_SUCCESS(hipblasSyrStridedBatchedFn(
            handle, uplo, N, &h_alpha, dx, incx, stridex, dA, lda, strideA, batch_count));

        ASSERT_HIP_SUCCESS(
            hipMemcpy(hA_host.data(), dA, sizeof(T) * A_size, hipMemcpyDeviceToHost));
        ASSERT_HIP_SUCCESS(hipMemcpy(dA, hA.data(), sizeof(T) * A_size, hipMemcpyHostToDevice));

        ASSERT_HIPBLAS_SUCCESS(hipblasSetPointerMode(handle, HIPBLAS_POINTER_MODE_DEVICE));
        ASSERT_HIPBLAS_SUCCESS(hipblasSyrStridedBatchedFn(
            handle, uplo, N, d_alpha, dx, incx, stridex, dA, lda, strideA, batch_count));

        ASSERT_HIP_SUCCESS(
            hipMemcpy(hA_device.data(), dA, sizeof(T) * A_size, hipMemcpyDeviceToHost));

        /* =====================================================================
           CPU BLAS
        =================================================================== */
        for(int b = 0; b < batch_count; b++)
        {
            cblas_syr<T>(
                uplo, N, h_alpha, hx.data() + b * stridex, incx, hA_cpu.data() + b * strideA, lda);
        }

        // enable unit check, notice unit check is not invasive, but norm check is,
        // unit check and norm check can not be interchanged their order
        if(arg.unit_check)
        {
            unit_check_general<T>(N, N, batch_count, lda, strideA, hA_cpu, hA_host);
            unit_check_general<T>(N, N, batch_count, lda, strideA, hA_cpu, hA_device);
        }
        if(arg.norm_check)
        {
            hipblas_error_host = norm_check_general<T>(
                'F', N, N, lda, strideA, hA_cpu.data(), hA_host.data(), batch_count);
            hipblas_error_device = norm_check_general<T>(
                'F', N, N, lda, strideA, hA_cpu.data(), hA_device.data(), batch_count);
        }
    }

    if(arg.timing)
    {
        ASSERT_HIP_SUCCESS(hipMemcpy(dA, hA.data(), sizeof(T) * A_size, hipMemcpyHostToDevice));
        hipStream_t stream;
        ASSERT_HIPBLAS_SUCCESS(hipblasGetStream(handle, &stream));
        ASSERT_HIPBLAS_SUCCESS(hipblasSetPointerMode(handle, HIPBLAS_POINTER_MODE_DEVICE));

        int runs = arg.cold_iters + arg.iters;
        for(int iter = 0; iter < runs; iter++)
        {
            if(iter == arg.cold_iters)
                gpu_time_used = get_time_us_sync(stream);

            ASSERT_HIPBLAS_SUCCESS(hipblasSyrStridedBatchedFn(
                handle, uplo, N, d_alpha, dx, incx, stridex, dA, lda, strideA, batch_count));
        }
        gpu_time_used = get_time_us_sync(stream) - gpu_time_used;

        hipblasSyrStridedBatchedModel{}.log_args<T>(std::cout,
                                                    arg,
                                                    gpu_time_used,
                                                    syr_gflop_count<T>(N),
                                                    syr_gbyte_count<T>(N),
                                                    hipblas_error_host,
                                                    hipblas_error_device);
    }
}

template <typename T>
hipblasStatus_t testing_syr_strided_batched_ret(const Arguments& arg)
{
    testing_syr_strided_batched<T>(arg);
    return HIPBLAS_STATUS_SUCCESS;
}