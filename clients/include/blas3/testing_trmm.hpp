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

using hipblasTrmmModel
    = ArgumentModel<e_a_type, e_side, e_uplo, e_transA, e_diag, e_M, e_N, e_alpha, e_lda, e_ldb>;

inline void testname_trmm(const Arguments& arg, std::string& name)
{
    hipblasTrmmModel{}.test_name(arg, name);
}

template <typename T>
void testing_trmm_bad_arg(const Arguments& arg)
{
    auto hipblasTrmmFn = arg.fortran ? hipblasTrmm<T, true> : hipblasTrmm<T, false>;
    bool inplace       = arg.inplace;

    for(auto pointer_mode : {HIPBLAS_POINTER_MODE_DEVICE, HIPBLAS_POINTER_MODE_HOST})
    {
        hipblasSideMode_t  side   = HIPBLAS_SIDE_LEFT;
        hipblasFillMode_t  uplo   = HIPBLAS_FILL_MODE_LOWER;
        hipblasOperation_t transA = HIPBLAS_OP_N;
        hipblasDiagType_t  diag   = HIPBLAS_DIAG_NON_UNIT;
        int                M      = 100;
        int                N      = 101;
        int                lda    = 102;
        int                ldb    = 103;
        int                ldc    = 104;
        int                ldOut  = inplace ? ldb : ldc;
        int                K      = M;

        device_vector<T> alpha_d(1), zero_d(1);

        const T alpha_h(1), zero_h(0);

        const T* alpha = &alpha_h;
        const T* zero  = &zero_h;

        hipblasLocalHandle handle(arg);
        ASSERT_HIPBLAS_SUCCESS(hipblasSetPointerMode(handle, pointer_mode));

        if(pointer_mode == HIPBLAS_POINTER_MODE_DEVICE)
        {
            ASSERT_HIP_SUCCESS(hipMemcpy(alpha_d, alpha, sizeof(*alpha), hipMemcpyHostToDevice));
            alpha = alpha_d;
            ASSERT_HIP_SUCCESS(hipMemcpy(zero_d, zero, sizeof(*zero), hipMemcpyHostToDevice));
            zero = zero_d;
        }

        size_t A_size = size_t(lda) * K;
        size_t B_size = size_t(ldb) * N;

        size_t C_size = inplace ? 1 : size_t(ldc) * N;

        device_vector<T> dA(A_size);
        device_vector<T> dB(B_size);
        device_vector<T> dC(C_size);

        device_vector<T>* dOut = inplace ? &dB : &dC;

        // cuBLAS doesn't have CUBLAS_SIDE_BOTH so will return invalid_enum,
        // while rocBLAS has rocblas_side_both, but just invalid for this func.
        // invalid enums
        EXPECT_HIPBLAS_STATUS2(hipblasTrmmFn(handle,
                                             HIPBLAS_SIDE_BOTH,
                                             uplo,
                                             transA,
                                             diag,
                                             M,
                                             N,
                                             alpha,
                                             dA,
                                             lda,
                                             dB,
                                             ldb,
                                             *dOut,
                                             ldOut),
#ifndef __HIP_PLATFORM_NVCC__
                               HIPBLAS_STATUS_INVALID_VALUE);
#else
                               HIPBLAS_STATUS_INVALID_ENUM);
#endif

        EXPECT_HIPBLAS_STATUS2(hipblasTrmmFn(handle,
                                             side,
                                             HIPBLAS_FILL_MODE_FULL,
                                             transA,
                                             diag,
                                             M,
                                             N,
                                             alpha,
                                             dA,
                                             lda,
                                             dB,
                                             ldb,
                                             *dOut,
                                             ldOut),
#ifndef __HIP_PLATFORM_NVCC__
                               HIPBLAS_STATUS_INVALID_VALUE);
#else
                               HIPBLAS_STATUS_INVALID_ENUM);
#endif

        EXPECT_HIPBLAS_STATUS2(hipblasTrmmFn(handle,
                                             side,
                                             uplo,
                                             (hipblasOperation_t)HIPBLAS_SIDE_BOTH,
                                             diag,
                                             M,
                                             N,
                                             alpha,
                                             dA,
                                             lda,
                                             dB,
                                             ldb,
                                             *dOut,
                                             ldOut),
                               HIPBLAS_STATUS_INVALID_ENUM);

        EXPECT_HIPBLAS_STATUS2(hipblasTrmmFn(handle,
                                             side,
                                             uplo,
                                             transA,
                                             (hipblasDiagType_t)HIPBLAS_SIDE_BOTH,
                                             M,
                                             N,
                                             alpha,
                                             dA,
                                             lda,
                                             dB,
                                             ldb,
                                             *dOut,
                                             ldOut),
                               HIPBLAS_STATUS_INVALID_ENUM);

        // invalid sizes
        EXPECT_HIPBLAS_STATUS2(
            hipblasTrmmFn(
                handle, side, uplo, transA, diag, -1, N, alpha, dA, lda, dB, ldb, *dOut, ldOut),
            HIPBLAS_STATUS_INVALID_VALUE);

        EXPECT_HIPBLAS_STATUS2(
            hipblasTrmmFn(
                handle, side, uplo, transA, diag, M, -1, alpha, dA, lda, dB, ldb, *dOut, ldOut),
            HIPBLAS_STATUS_INVALID_VALUE);

        // invalid leading dims
        EXPECT_HIPBLAS_STATUS2(
            hipblasTrmmFn(
                handle, side, uplo, transA, diag, M, N, alpha, dA, M - 1, dB, ldb, *dOut, ldOut),
            HIPBLAS_STATUS_INVALID_VALUE);

        EXPECT_HIPBLAS_STATUS2(hipblasTrmmFn(handle,
                                             HIPBLAS_SIDE_RIGHT,
                                             uplo,
                                             transA,
                                             diag,
                                             M,
                                             N,
                                             alpha,
                                             dA,
                                             N - 1,
                                             dB,
                                             ldb,
                                             *dOut,
                                             ldOut),
                               HIPBLAS_STATUS_INVALID_VALUE);

        EXPECT_HIPBLAS_STATUS2(
            hipblasTrmmFn(
                handle, side, uplo, transA, diag, M, N, alpha, dA, lda, dB, M - 1, *dOut, ldOut),
            HIPBLAS_STATUS_INVALID_VALUE);

        EXPECT_HIPBLAS_STATUS2(
            hipblasTrmmFn(
                handle, side, uplo, transA, diag, M, N, alpha, dA, lda, dB, ldb, *dOut, M - 1),
            HIPBLAS_STATUS_INVALID_VALUE);

        // nullptr checks
        EXPECT_HIPBLAS_STATUS2(
            hipblasTrmmFn(
                nullptr, side, uplo, transA, diag, M, N, alpha, dA, lda, dB, ldb, *dOut, ldOut),
            HIPBLAS_STATUS_NOT_INITIALIZED);

        EXPECT_HIPBLAS_STATUS2(
            hipblasTrmmFn(
                handle, side, uplo, transA, diag, M, N, alpha, dA, lda, dB, ldb, nullptr, ldOut),
            HIPBLAS_STATUS_INVALID_VALUE);

#ifndef __HIP_PLATFORM_NVCC__
        // cuBLAS doesn't check for nullptrs for alpha, A, B
        EXPECT_HIPBLAS_STATUS2(
            hipblasTrmmFn(
                handle, side, uplo, transA, diag, M, N, nullptr, dA, lda, dB, ldb, *dOut, ldOut),
            HIPBLAS_STATUS_INVALID_VALUE);

        EXPECT_HIPBLAS_STATUS2(
            hipblasTrmmFn(
                handle, side, uplo, transA, diag, M, N, alpha, nullptr, lda, dB, ldb, *dOut, ldOut),
            HIPBLAS_STATUS_INVALID_VALUE);

        EXPECT_HIPBLAS_STATUS2(
            hipblasTrmmFn(
                handle, side, uplo, transA, diag, M, N, alpha, dA, lda, nullptr, ldb, *dOut, ldOut),
            HIPBLAS_STATUS_INVALID_VALUE);

        // quick return: if alpha == 0, both A & B can be nullptr
        EXPECT_HIPBLAS_STATUS2(hipblasTrmmFn(handle,
                                             side,
                                             uplo,
                                             transA,
                                             diag,
                                             M,
                                             N,
                                             zero,
                                             nullptr,
                                             lda,
                                             nullptr,
                                             ldb,
                                             *dOut,
                                             ldOut),
                               HIPBLAS_STATUS_SUCCESS);
#endif
        // quick return: if M == 0, then all other ptrs can be nullptr
        EXPECT_HIPBLAS_STATUS2(hipblasTrmmFn(handle,
                                             side,
                                             uplo,
                                             transA,
                                             diag,
                                             0,
                                             N,
                                             nullptr,
                                             nullptr,
                                             lda,
                                             nullptr,
                                             ldb,
                                             nullptr,
                                             ldOut),
                               HIPBLAS_STATUS_SUCCESS);

        // quick return: if N == 0, then all other ptrs can be nullptr
        EXPECT_HIPBLAS_STATUS2(hipblasTrmmFn(handle,
                                             side,
                                             uplo,
                                             transA,
                                             diag,
                                             M,
                                             0,
                                             nullptr,
                                             nullptr,
                                             lda,
                                             nullptr,
                                             ldb,
                                             nullptr,
                                             ldOut),
                               HIPBLAS_STATUS_SUCCESS);

        // in-place only checks
        if(inplace)
        {
            // if inplace, must have ldb == ldc
            EXPECT_HIPBLAS_STATUS2(hipblasTrmmFn(handle,
                                                 side,
                                                 uplo,
                                                 transA,
                                                 diag,
                                                 M,
                                                 N,
                                                 alpha,
                                                 dA,
                                                 lda,
                                                 dB,
                                                 ldb,
                                                 *dOut,
                                                 ldb + 1),
                                   HIPBLAS_STATUS_INVALID_VALUE);
        }
    }
}

template <typename T>
hipblasStatus_t testing_trmm_bad_arg_ret(const Arguments& arg)
{
    testing_trmm_bad_arg<T>(arg);
    return HIPBLAS_STATUS_SUCCESS;
}

template <typename T>
void testing_trmm(const Arguments& arg)
{
    bool FORTRAN       = arg.fortran;
    auto hipblasTrmmFn = FORTRAN ? hipblasTrmm<T, true> : hipblasTrmm<T, false>;
    bool inplace       = arg.inplace;

    hipblasSideMode_t  side   = char2hipblas_side(arg.side);
    hipblasFillMode_t  uplo   = char2hipblas_fill(arg.uplo);
    hipblasOperation_t transA = char2hipblas_operation(arg.transA);
    hipblasDiagType_t  diag   = char2hipblas_diagonal(arg.diag);
    int                M      = arg.M;
    int                N      = arg.N;
    int                lda    = arg.lda;
    int                ldb    = arg.ldb;
    int                ldc    = arg.ldc;
    int                ldOut  = inplace ? ldb : ldc;

    T h_alpha = arg.get_alpha<T>();

    int    K      = (side == HIPBLAS_SIDE_LEFT ? M : N);
    size_t A_size = size_t(lda) * K;
    size_t B_size = size_t(ldb) * N;

    // avoid unnecessary allocation if inplace
    size_t C_size   = inplace ? 1 : size_t(ldc) * N;
    size_t out_size = ldOut * N;

    // check here to prevent undefined memory allocation error
    if(M < 0 || N < 0 || lda < K || ldb < M || ldOut < M)
    {
        return;
    }

    // Naming: dK is in GPU (device) memory. hK is in CPU (host) memory
    host_vector<T> hA(A_size);
    host_vector<T> hB(B_size);
    host_vector<T> hC(C_size);

    host_vector<T> hOut_host(out_size);
    host_vector<T> hOut_device(out_size);
    host_vector<T> hOut_gold(out_size);

    device_vector<T> dA(A_size);
    device_vector<T> dB(B_size);
    device_vector<T> dC(C_size);
    device_vector<T> d_alpha(1);

    device_vector<T>* dOut = inplace ? &dB : &dC;

    double             gpu_time_used, hipblas_error_host, hipblas_error_device;
    hipblasLocalHandle handle(arg);

    // Initial Data on CPU
    hipblas_init_matrix(hA, arg, K, K, lda, 0, 1, hipblas_client_alpha_sets_nan, true);
    hipblas_init_matrix(hB, arg, M, N, ldb, 0, 1, hipblas_client_alpha_sets_nan, false, true);

    if(!inplace)
        hipblas_init_matrix(hC, arg, M, N, ldc, 0, 1, hipblas_client_alpha_sets_nan, false, true);

    hOut_host   = inplace ? hB : hC;
    hOut_device = hOut_host;
    hOut_gold   = hOut_host;

    // copy data from CPU to device
    ASSERT_HIP_SUCCESS(hipMemcpy(dA, hA, sizeof(T) * A_size, hipMemcpyHostToDevice));
    ASSERT_HIP_SUCCESS(hipMemcpy(dB, hB, sizeof(T) * B_size, hipMemcpyHostToDevice));
    ASSERT_HIP_SUCCESS(hipMemcpy(dC, hC, sizeof(T) * C_size, hipMemcpyHostToDevice));
    ASSERT_HIP_SUCCESS(hipMemcpy(d_alpha, &h_alpha, sizeof(T), hipMemcpyHostToDevice));

    if(arg.unit_check || arg.norm_check)
    {
        /* =====================================================================
            HIPBLAS
        =================================================================== */
        ASSERT_HIPBLAS_SUCCESS(hipblasSetPointerMode(handle, HIPBLAS_POINTER_MODE_HOST));

        ASSERT_HIPBLAS_SUCCESS(hipblasTrmmFn(
            handle, side, uplo, transA, diag, M, N, &h_alpha, dA, lda, dB, ldb, *dOut, ldOut));

        // copy output from device to CPU
        ASSERT_HIP_SUCCESS(
            hipMemcpy(hOut_host, *dOut, sizeof(T) * out_size, hipMemcpyDeviceToHost));
        ASSERT_HIP_SUCCESS(hipMemcpy(dB, hB, sizeof(T) * B_size, hipMemcpyHostToDevice));
        ASSERT_HIP_SUCCESS(hipMemcpy(dC, hC, sizeof(T) * C_size, hipMemcpyHostToDevice));

        ASSERT_HIPBLAS_SUCCESS(hipblasSetPointerMode(handle, HIPBLAS_POINTER_MODE_DEVICE));

        ASSERT_HIPBLAS_SUCCESS(hipblasTrmmFn(
            handle, side, uplo, transA, diag, M, N, d_alpha, dA, lda, dB, ldb, *dOut, ldOut));

        ASSERT_HIP_SUCCESS(
            hipMemcpy(hOut_device, *dOut, sizeof(T) * out_size, hipMemcpyDeviceToHost));

        /* =====================================================================
           CPU BLAS
        =================================================================== */
        // use hB matrix for cblas, copy into C matrix for !inplace version to compare with hipblas
        cblas_trmm<T>(side, uplo, transA, diag, M, N, h_alpha, hA, lda, hB, ldb);
        copy_matrix_with_different_leading_dimensions(hB, hOut_gold, M, N, ldb, ldOut);

        // enable unit check, notice unit check is not invasive, but norm check is,
        // unit check and norm check can not be interchanged their order
        if(arg.unit_check)
        {
            unit_check_general<T>(M, N, ldOut, hOut_gold, hOut_host);
            unit_check_general<T>(M, N, ldOut, hOut_gold, hOut_device);
        }
        if(arg.norm_check)
        {
            hipblas_error_host   = norm_check_general<T>('F', M, N, ldOut, hOut_gold, hOut_host);
            hipblas_error_device = norm_check_general<T>('F', M, N, ldOut, hOut_gold, hOut_device);
        }
    }

    if(arg.timing)
    {
        hipStream_t stream;
        ASSERT_HIPBLAS_SUCCESS(hipblasGetStream(handle, &stream));
        ASSERT_HIPBLAS_SUCCESS(hipblasSetPointerMode(handle, HIPBLAS_POINTER_MODE_DEVICE));

        int runs = arg.cold_iters + arg.iters;
        for(int iter = 0; iter < runs; iter++)
        {
            if(iter == arg.cold_iters)
                gpu_time_used = get_time_us_sync(stream);

            ASSERT_HIPBLAS_SUCCESS(hipblasTrmmFn(
                handle, side, uplo, transA, diag, M, N, d_alpha, dA, lda, dB, ldb, *dOut, ldOut));
        }
        gpu_time_used = get_time_us_sync(stream) - gpu_time_used;

        hipblasTrmmModel{}.log_args<T>(std::cout,
                                       arg,
                                       gpu_time_used,
                                       trmm_gflop_count<T>(M, N, K),
                                       trmm_gbyte_count<T>(M, N, K),
                                       hipblas_error_host,
                                       hipblas_error_device);
    }
}

template <typename T>
hipblasStatus_t testing_trmm_ret(const Arguments& arg)
{
    testing_trmm<T>(arg);
    return HIPBLAS_STATUS_SUCCESS;
}
