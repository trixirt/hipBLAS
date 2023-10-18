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

using hipblasRotExModel
    = ArgumentModel<e_a_type, e_b_type, e_c_type, e_compute_type, e_N, e_incx, e_incy>;

inline void testname_rot_ex(const Arguments& arg, std::string& name)
{
    hipblasRotExModel{}.test_name(arg, name);
}

template <typename Tx, typename Ty = Tx, typename Tcs = Ty, typename Tex = Tcs>
void testing_rot_ex(const Arguments& arg)
{
    bool FORTRAN        = arg.fortran;
    auto hipblasRotExFn = FORTRAN ? hipblasRotExFortran : hipblasRotEx;

    int N    = arg.N;
    int incx = arg.incx;
    int incy = arg.incy;

    hipblasDatatype_t xType         = arg.a_type;
    hipblasDatatype_t yType         = arg.b_type;
    hipblasDatatype_t csType        = arg.c_type;
    hipblasDatatype_t executionType = arg.compute_type;

    hipblasLocalHandle handle(arg);

    // check to prevent undefined memory allocation error
    if(N <= 0)
    {
        ASSERT_HIPBLAS_SUCCESS(hipblasRotExFn(handle,
                                              N,
                                              nullptr,
                                              xType,
                                              incx,
                                              nullptr,
                                              yType,
                                              incy,
                                              nullptr,
                                              nullptr,
                                              csType,
                                              executionType));
        return;
    }

    double gpu_time_used, hipblas_error_host, hipblas_error_device;

    int    abs_incx = incx >= 0 ? incx : -incx;
    int    abs_incy = incy >= 0 ? incy : -incy;
    size_t size_x   = N * size_t(abs_incx);
    size_t size_y   = N * size_t(abs_incy);
    if(!size_x)
        size_x = 1;
    if(!size_y)
        size_y = 1;

    device_vector<Tx>  dx(size_x);
    device_vector<Ty>  dy(size_y);
    device_vector<Tcs> dc(1);
    device_vector<Tcs> ds(1);

    // Initial Data on CPU
    host_vector<Tx>  hx_host(size_x);
    host_vector<Ty>  hy_host(size_y);
    host_vector<Tx>  hx_device(size_x);
    host_vector<Ty>  hy_device(size_y);
    host_vector<Tx>  hx_cpu(size_x);
    host_vector<Ty>  hy_cpu(size_y);
    host_vector<Tcs> hc(1);
    host_vector<Tcs> hs(1);

    // Random alpha (0 - 10)
    host_vector<int> alpha(1);

    hipblas_init_vector(hx_host, arg, N, abs_incx, 0, 1, hipblas_client_never_set_nan, true);
    hipblas_init_vector(hy_host, arg, N, abs_incy, 0, 1, hipblas_client_never_set_nan, false);
    hipblas_init_vector(alpha, arg, 1, 1, 0, 1, hipblas_client_never_set_nan, false);
    hipblas_init_vector(hc, arg, 1, 1, 0, 1, hipblas_client_never_set_nan, false);
    hipblas_init_vector(hs, arg, 1, 1, 0, 1, hipblas_client_never_set_nan, false);

    // // cos and sin of alpha (in rads)
    // hc[0] = cos(alpha[0]);
    // hs[0] = sin(alpha[0]);

    // CPU BLAS reference data
    hx_cpu = hx_device = hx_host;
    hy_cpu = hy_device = hy_host;

    ASSERT_HIP_SUCCESS(hipMemcpy(dx, hx_host, sizeof(Tx) * size_x, hipMemcpyHostToDevice));
    ASSERT_HIP_SUCCESS(hipMemcpy(dy, hy_host, sizeof(Ty) * size_y, hipMemcpyHostToDevice));
    ASSERT_HIP_SUCCESS(hipMemcpy(dc, hc, sizeof(Tcs), hipMemcpyHostToDevice));
    ASSERT_HIP_SUCCESS(hipMemcpy(ds, hs, sizeof(Tcs), hipMemcpyHostToDevice));

    if(arg.unit_check || arg.norm_check)
    {
        // HIPBLAS
        ASSERT_HIPBLAS_SUCCESS(hipblasSetPointerMode(handle, HIPBLAS_POINTER_MODE_HOST));
        ASSERT_HIPBLAS_SUCCESS(hipblasRotExFn(
            handle, N, dx, xType, incx, dy, yType, incy, hc, hs, csType, executionType));

        ASSERT_HIP_SUCCESS(hipMemcpy(hx_host, dx, sizeof(Tx) * size_x, hipMemcpyDeviceToHost));
        ASSERT_HIP_SUCCESS(hipMemcpy(hy_host, dy, sizeof(Ty) * size_y, hipMemcpyDeviceToHost));
        ASSERT_HIP_SUCCESS(hipMemcpy(dx, hx_device, sizeof(Tx) * size_x, hipMemcpyHostToDevice));
        ASSERT_HIP_SUCCESS(hipMemcpy(dy, hy_device, sizeof(Ty) * size_y, hipMemcpyHostToDevice));

        ASSERT_HIPBLAS_SUCCESS(hipblasSetPointerMode(handle, HIPBLAS_POINTER_MODE_DEVICE));
        ASSERT_HIPBLAS_SUCCESS(hipblasRotExFn(
            handle, N, dx, xType, incx, dy, yType, incy, dc, ds, csType, executionType));

        ASSERT_HIP_SUCCESS(hipMemcpy(hx_device, dx, sizeof(Tx) * size_x, hipMemcpyDeviceToHost));
        ASSERT_HIP_SUCCESS(hipMemcpy(hy_device, dy, sizeof(Ty) * size_y, hipMemcpyDeviceToHost));

        // CBLAS
        // TODO: execution type in cblas_rot
        cblas_rot<Tx, Tcs, Tcs>(N, hx_cpu.data(), incx, hy_cpu.data(), incy, *hc, *hs);

        if(arg.unit_check)
        {
            unit_check_general<Tx>(1, N, abs_incx, hx_cpu, hx_host);
            unit_check_general<Ty>(1, N, abs_incy, hy_cpu, hy_host);
            unit_check_general<Tx>(1, N, abs_incx, hx_cpu, hx_device);
            unit_check_general<Ty>(1, N, abs_incy, hy_cpu, hy_device);
        }
        if(arg.norm_check)
        {
            hipblas_error_host = norm_check_general<Tx>('F', 1, N, abs_incx, hx_cpu, hx_host);
            hipblas_error_host += norm_check_general<Ty>('F', 1, N, abs_incy, hy_cpu, hy_host);
            hipblas_error_device = norm_check_general<Tx>('F', 1, N, abs_incx, hx_cpu, hx_device);
            hipblas_error_device += norm_check_general<Ty>('F', 1, N, abs_incy, hy_cpu, hy_device);
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

            ASSERT_HIPBLAS_SUCCESS(hipblasRotExFn(
                handle, N, dx, xType, incx, dy, yType, incy, dc, ds, csType, executionType));
        }
        gpu_time_used = get_time_us_sync(stream) - gpu_time_used;

        hipblasRotExModel{}.log_args<Tx>(std::cout,
                                         arg,
                                         gpu_time_used,
                                         rot_gflop_count<Tx, Ty, Tcs, Tcs>(N),
                                         rot_gbyte_count<Tx>(N),
                                         hipblas_error_host,
                                         hipblas_error_device);
    }
}

template <typename Tx, typename Ty = Tx, typename Tcs = Ty, typename Tex = Tcs>
hipblasStatus_t testing_rot_ex_ret(const Arguments& arg)
{
    testing_rot_ex<Tx, Ty, Tcs, Tex>(arg);
    return HIPBLAS_STATUS_SUCCESS;
}