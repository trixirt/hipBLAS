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

#include <stdio.h>
#include <stdlib.h>
#include <vector>

#include "testing_common.hpp"

/* ============================================================================================ */

using hipblasAxpyBatchedExModel = ArgumentModel<e_a_type,
                                                e_b_type,
                                                e_c_type,
                                                e_compute_type,
                                                e_N,
                                                e_alpha,
                                                e_incx,
                                                e_incy,
                                                e_batch_count>;

inline void testname_axpy_batched_ex(const Arguments& arg, std::string& name)
{
    hipblasAxpyBatchedExModel{}.test_name(arg, name);
}

template <typename Ta, typename Tx = Ta, typename Ty = Tx, typename Tex = Ty>
void testing_axpy_batched_ex(const Arguments& arg)
{
    bool FORTRAN                = arg.fortran;
    auto hipblasAxpyBatchedExFn = FORTRAN ? hipblasAxpyBatchedExFortran : hipblasAxpyBatchedEx;

    int N           = arg.N;
    int incx        = arg.incx;
    int incy        = arg.incy;
    int batch_count = arg.batch_count;

    hipblasDatatype_t alphaType     = arg.a_type;
    hipblasDatatype_t xType         = arg.b_type;
    hipblasDatatype_t yType         = arg.c_type;
    hipblasDatatype_t executionType = arg.compute_type;

    hipblasLocalHandle handle(arg);

    // argument sanity check, quick return if input parameters are invalid before allocating invalid
    // memory
    if(N <= 0 || batch_count <= 0)
    {
        ASSERT_HIPBLAS_SUCCESS(hipblasAxpyBatchedExFn(handle,
                                                      N,
                                                      nullptr,
                                                      alphaType,
                                                      nullptr,
                                                      xType,
                                                      incx,
                                                      nullptr,
                                                      yType,
                                                      incy,
                                                      batch_count,
                                                      executionType));
        return;
    }

    int abs_incx = incx < 0 ? -incx : incx;
    int abs_incy = incy < 0 ? -incy : incy;

    Ta h_alpha = arg.get_alpha<Ta>();

    // Naming: dX is in GPU (device) memory. hK is in CPU (host) memory, plz follow this practice
    host_batch_vector<Tx> hx(N, incx, batch_count);
    host_batch_vector<Ty> hy_host(N, incy, batch_count);
    host_batch_vector<Ty> hy_device(N, incy, batch_count);
    host_batch_vector<Ty> hy_cpu(N, incy, batch_count);

    device_batch_vector<Tx> dx(N, incx, batch_count);
    device_batch_vector<Ty> dy(N, incy, batch_count);
    device_vector<Ta>       d_alpha(1);

    ASSERT_HIP_SUCCESS(dx.memcheck());
    ASSERT_HIP_SUCCESS(dy.memcheck());

    double gpu_time_used, hipblas_error_host, hipblas_error_device;

    // Initial Data on CPU
    hipblas_init_vector(hx, arg, hipblas_client_alpha_sets_nan, true);
    hipblas_init_vector(hy_host, arg, hipblas_client_alpha_sets_nan, false);

    hy_device.copy_from(hy_host);
    hy_cpu.copy_from(hy_host);

    ASSERT_HIP_SUCCESS(dx.transfer_from(hx));
    ASSERT_HIP_SUCCESS(dy.transfer_from(hy_host));
    ASSERT_HIP_SUCCESS(hipMemcpy(d_alpha, &h_alpha, sizeof(Ta), hipMemcpyHostToDevice));

    /* =====================================================================
         HIPBLAS
    =================================================================== */
    ASSERT_HIPBLAS_SUCCESS(hipblasSetPointerMode(handle, HIPBLAS_POINTER_MODE_HOST));
    ASSERT_HIPBLAS_SUCCESS(hipblasAxpyBatchedExFn(handle,
                                                  N,
                                                  &h_alpha,
                                                  alphaType,
                                                  dx.ptr_on_device(),
                                                  xType,
                                                  incx,
                                                  dy.ptr_on_device(),
                                                  yType,
                                                  incy,
                                                  batch_count,
                                                  executionType));

    ASSERT_HIP_SUCCESS(hy_host.transfer_from(dy));
    ASSERT_HIP_SUCCESS(dy.transfer_from(hy_device));

    ASSERT_HIPBLAS_SUCCESS(hipblasSetPointerMode(handle, HIPBLAS_POINTER_MODE_DEVICE));
    ASSERT_HIPBLAS_SUCCESS(hipblasAxpyBatchedExFn(handle,
                                                  N,
                                                  d_alpha,
                                                  alphaType,
                                                  dx.ptr_on_device(),
                                                  xType,
                                                  incx,
                                                  dy.ptr_on_device(),
                                                  yType,
                                                  incy,
                                                  batch_count,
                                                  executionType));

    ASSERT_HIP_SUCCESS(hy_device.transfer_from(dy));

    if(arg.unit_check || arg.norm_check)
    {
        /* =====================================================================
                    CPU BLAS
        =================================================================== */
        for(int b = 0; b < batch_count; b++)
        {
            cblas_axpy(N, h_alpha, hx[b], incx, hy_cpu[b], incy);
        }

        // enable unit check, notice unit check is not invasive, but norm check is,
        // unit check and norm check can not be interchanged their order
        if(arg.unit_check)
        {
            unit_check_general<Ty>(1, N, batch_count, abs_incy, hy_cpu, hy_host);
            unit_check_general<Ty>(1, N, batch_count, abs_incy, hy_cpu, hy_device);
        }
        if(arg.norm_check)
        {
            hipblas_error_host
                = norm_check_general<Ty>('F', 1, N, abs_incy, hy_cpu, hy_host, batch_count);
            hipblas_error_device
                = norm_check_general<Ty>('F', 1, N, abs_incy, hy_cpu, hy_device, batch_count);
        }

    } // end of if unit check

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

            ASSERT_HIPBLAS_SUCCESS(hipblasAxpyBatchedExFn(handle,
                                                          N,
                                                          d_alpha,
                                                          alphaType,
                                                          dx.ptr_on_device(),
                                                          xType,
                                                          incx,
                                                          dy.ptr_on_device(),
                                                          yType,
                                                          incy,
                                                          batch_count,
                                                          executionType));
        }
        gpu_time_used = get_time_us_sync(stream) - gpu_time_used;

        hipblasAxpyBatchedExModel{}.log_args<Ta>(std::cout,
                                                 arg,
                                                 gpu_time_used,
                                                 axpy_gflop_count<Ta>(N),
                                                 axpy_gbyte_count<Ta>(N),
                                                 hipblas_error_host,
                                                 hipblas_error_device);
    }
}

template <typename Ta, typename Tx = Ta, typename Ty = Tx, typename Tex = Ty>
hipblasStatus_t testing_axpy_batched_ex_ret(const Arguments& arg)
{
    testing_axpy_batched_ex<Ta, Tx, Ty, Tex>(arg);
    return HIPBLAS_STATUS_SUCCESS;
}
