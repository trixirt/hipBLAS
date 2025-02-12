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

using hipblasTbsvModel = ArgumentModel<e_a_type, e_uplo, e_transA, e_diag, e_N, e_K, e_lda, e_incx>;

inline void testname_tbsv(const Arguments& arg, std::string& name)
{
    hipblasTbsvModel{}.test_name(arg, name);
}

template <typename T>
void testing_tbsv(const Arguments& arg)
{
    bool FORTRAN       = arg.api == hipblas_client_api::FORTRAN;
    auto hipblasTbsvFn = FORTRAN ? hipblasTbsv<T, true> : hipblasTbsv<T, false>;

    hipblasFillMode_t  uplo   = char2hipblas_fill(arg.uplo);
    hipblasDiagType_t  diag   = char2hipblas_diagonal(arg.diag);
    hipblasOperation_t transA = char2hipblas_operation(arg.transA);
    int                N      = arg.N;
    int                K      = arg.K;
    int                incx   = arg.incx;
    int                lda    = arg.lda;

    hipblasLocalHandle handle(arg);

    // argument sanity check, quick return if input parameters are invalid before allocating invalid
    // memory
    bool invalid_size = N < 0 || K < 0 || lda < K + 1 || !incx;
    if(invalid_size || !N)
    {
        hipblasStatus_t actual
            = hipblasTbsvFn(handle, uplo, transA, diag, N, K, nullptr, lda, nullptr, incx);
        EXPECT_HIPBLAS_STATUS2(
            actual, (invalid_size ? HIPBLAS_STATUS_INVALID_VALUE : HIPBLAS_STATUS_SUCCESS));
        return;
    }

    int    abs_incx = incx < 0 ? -incx : incx;
    size_t size_A   = size_t(N) * N;
    size_t size_AB  = size_t(lda) * N;
    size_t size_x   = abs_incx * size_t(N);

    // Naming: dK is in GPU (device) memory. hK is in CPU (host) memory
    host_vector<T> hA(size_A);
    host_vector<T> hAB(size_AB);
    host_vector<T> AAT(size_A);
    host_vector<T> hb(size_x);
    host_vector<T> hx(size_x);
    host_vector<T> hx_or_b_1(size_x);

    device_vector<T> dAB(size_AB);
    device_vector<T> dx_or_b(size_x);

    double gpu_time_used, hipblas_error;

    // Initial Data on CPU
    hipblas_init_matrix(hA, arg, size_A, 1, 1, 0, 1, hipblas_client_never_set_nan, true);
    hipblas_init_vector(hx, arg, N, abs_incx, 0, 1, hipblas_client_never_set_nan, false, true);
    hb = hx;

    banded_matrix_setup(uplo == HIPBLAS_FILL_MODE_UPPER, (T*)hA, N, N, K);

    prepare_triangular_solve((T*)hA, N, (T*)AAT, N, arg.uplo);
    if(diag == HIPBLAS_DIAG_UNIT)
    {
        make_unit_diagonal(uplo, (T*)hA, N, N);
    }

    regular_to_banded(uplo == HIPBLAS_FILL_MODE_UPPER, (T*)hA, N, (T*)hAB, lda, N, K);
    ASSERT_HIP_SUCCESS(hipMemcpy(dAB, hAB.data(), sizeof(T) * size_AB, hipMemcpyHostToDevice));

    cblas_tbmv<T>(uplo, transA, diag, N, K, hAB, lda, hb, incx);
    hx_or_b_1 = hb;

    // copy data from CPU to device
    ASSERT_HIP_SUCCESS(
        hipMemcpy(dx_or_b, hx_or_b_1.data(), sizeof(T) * size_x, hipMemcpyHostToDevice));

    /* =====================================================================
           HIPBLAS
    =================================================================== */
    if(arg.unit_check || arg.norm_check)
    {
        ASSERT_HIPBLAS_SUCCESS(
            hipblasTbsvFn(handle, uplo, transA, diag, N, K, dAB, lda, dx_or_b, incx));

        // copy output from device to CPU
        ASSERT_HIP_SUCCESS(
            hipMemcpy(hx_or_b_1.data(), dx_or_b, sizeof(T) * size_x, hipMemcpyDeviceToHost));

        // Calculating error
        hipblas_error = std::abs(vector_norm_1<T>(N, abs_incx, hx.data(), hx_or_b_1.data()));

        if(arg.unit_check)
        {
            double tolerance = std::numeric_limits<real_t<T>>::epsilon() * 40 * N;
            unit_check_error(hipblas_error, tolerance);
        }
    }

    if(arg.timing)
    {
        hipStream_t stream;
        ASSERT_HIPBLAS_SUCCESS(hipblasGetStream(handle, &stream));

        int runs = arg.cold_iters + arg.iters;
        for(int iter = 0; iter < runs; iter++)
        {
            if(iter == arg.cold_iters)
                gpu_time_used = get_time_us_sync(stream);

            ASSERT_HIPBLAS_SUCCESS(
                hipblasTbsvFn(handle, uplo, transA, diag, N, K, dAB, lda, dx_or_b, incx));
        }
        gpu_time_used = get_time_us_sync(stream) - gpu_time_used; // in microseconds

        hipblasTbsvModel{}.log_args<T>(std::cout,
                                       arg,
                                       gpu_time_used,
                                       tbsv_gflop_count<T>(N, K),
                                       tbsv_gbyte_count<T>(N, K),
                                       hipblas_error);
    }
}

template <typename T>
hipblasStatus_t testing_tbsv_ret(const Arguments& arg)
{
    testing_tbsv<T>(arg);
    return HIPBLAS_STATUS_SUCCESS;
}
