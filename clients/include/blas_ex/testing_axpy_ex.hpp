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

using hipblasAxpyExModel
    = ArgumentModel<e_a_type, e_b_type, e_c_type, e_compute_type, e_N, e_alpha, e_incx, e_incy>;

inline void testname_axpy_ex(const Arguments& arg, std::string& name)
{
    hipblasAxpyExModel{}.test_name(arg, name);
}

template <typename Ta, typename Tx = Ta, typename Ty = Tx, typename Tex = Ty>
void testing_axpy_ex(const Arguments& arg)
{
    bool FORTRAN         = arg.fortran;
    auto hipblasAxpyExFn = FORTRAN ? hipblasAxpyExFortran : hipblasAxpyEx;

    int N    = arg.N;
    int incx = arg.incx;
    int incy = arg.incy;

    hipblasLocalHandle handle(arg);

    hipblasDatatype_t alphaType     = arg.a_type;
    hipblasDatatype_t xType         = arg.b_type;
    hipblasDatatype_t yType         = arg.c_type;
    hipblasDatatype_t executionType = arg.compute_type;

    // argument sanity check, quick return if input parameters are invalid before allocating invalid
    // memory
    if(N <= 0)
    {
        ASSERT_HIPBLAS_SUCCESS(hipblasAxpyExFn(handle,
                                               N,
                                               nullptr,
                                               alphaType,
                                               nullptr,
                                               xType,
                                               incx,
                                               nullptr,
                                               yType,
                                               incy,
                                               executionType));
        return;
    }

    int abs_incx = incx < 0 ? -incx : incx;
    int abs_incy = incy < 0 ? -incy : incy;

    size_t sizeX = size_t(N) * abs_incx;
    size_t sizeY = size_t(N) * abs_incy;
    if(!sizeX)
        sizeX = 1;
    if(!sizeY)
        sizeY = 1;

    Ta h_alpha = arg.get_alpha<Ta>();

    // Naming: dX is in GPU (device) memory. hK is in CPU (host) memory, plz follow this practice
    host_vector<Tx> hx(sizeX);
    host_vector<Ty> hy_host(sizeY);
    host_vector<Ty> hy_device(sizeY);
    host_vector<Ty> hy_cpu(sizeY);

    device_vector<Tx> dx(sizeX);
    device_vector<Ty> dy(sizeY);
    device_vector<Ta> d_alpha(1);

    double gpu_time_used, hipblas_error_host, hipblas_error_device;

    // Initial Data on CPU
    hipblas_init_vector(hx, arg, N, abs_incx, 0, 1, hipblas_client_alpha_sets_nan, true);
    hipblas_init_vector(hy_host, arg, N, abs_incy, 0, 1, hipblas_client_alpha_sets_nan, false);
    // hx[0] = (Tx)5.0f;
    // hy_host[0] = (Ty)22.0f;

    hy_device = hy_host;
    hy_cpu    = hy_host;

    ASSERT_HIP_SUCCESS(hipMemcpy(dx, hx, sizeof(Tx) * sizeX, hipMemcpyHostToDevice));
    ASSERT_HIP_SUCCESS(hipMemcpy(dy, hy_host, sizeof(Ty) * sizeY, hipMemcpyHostToDevice));
    ASSERT_HIP_SUCCESS(hipMemcpy(d_alpha, &h_alpha, sizeof(Ta), hipMemcpyHostToDevice));

    /* =====================================================================
         HIPBLAS
    =================================================================== */
    ASSERT_HIPBLAS_SUCCESS(hipblasSetPointerMode(handle, HIPBLAS_POINTER_MODE_HOST));
    ASSERT_HIPBLAS_SUCCESS(hipblasAxpyExFn(
        handle, N, &h_alpha, alphaType, dx, xType, incx, dy, yType, incy, executionType));

    // copy output from device to CPU
    ASSERT_HIP_SUCCESS(hipMemcpy(hy_host, dy, sizeof(Ty) * sizeY, hipMemcpyDeviceToHost));
    ASSERT_HIP_SUCCESS(hipMemcpy(dy, hy_device, sizeof(Ty) * sizeY, hipMemcpyHostToDevice));

    ASSERT_HIPBLAS_SUCCESS(hipblasSetPointerMode(handle, HIPBLAS_POINTER_MODE_DEVICE));
    ASSERT_HIPBLAS_SUCCESS(hipblasAxpyExFn(
        handle, N, d_alpha, alphaType, dx, xType, incx, dy, yType, incy, executionType));
    ASSERT_HIP_SUCCESS(hipMemcpy(hy_device, dy, sizeof(Ty) * sizeY, hipMemcpyDeviceToHost));

    if(arg.unit_check || arg.norm_check)
    {
        /* =====================================================================
                    CPU BLAS
        =================================================================== */
        cblas_axpy(N, h_alpha, hx.data(), incx, hy_cpu.data(), incy);

        // enable unit check, notice unit check is not invasive, but norm check is,
        // unit check and norm check can not be interchanged their order
        if(arg.unit_check)
        {
            unit_check_general<Ty>(1, N, abs_incy, hy_cpu, hy_host);
            unit_check_general<Ty>(1, N, abs_incy, hy_cpu, hy_device);
        }
        if(arg.norm_check)
        {
            hipblas_error_host   = norm_check_general<Ty>('F', 1, N, abs_incy, hy_cpu, hy_host);
            hipblas_error_device = norm_check_general<Ty>('F', 1, N, abs_incy, hy_cpu, hy_device);
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

            ASSERT_HIPBLAS_SUCCESS(hipblasAxpyExFn(
                handle, N, d_alpha, alphaType, dx, xType, incx, dy, yType, incy, executionType));
        }
        gpu_time_used = get_time_us_sync(stream) - gpu_time_used;

        hipblasAxpyExModel{}.log_args<Ta>(std::cout,
                                          arg,
                                          gpu_time_used,
                                          axpy_gflop_count<Ta>(N),
                                          axpy_gbyte_count<Ta>(N),
                                          hipblas_error_host,
                                          hipblas_error_device);
    }
}

template <typename Ta, typename Tx = Ta, typename Ty = Tx, typename Tex = Ty>
hipblasStatus_t testing_axpy_ex_ret(const Arguments& arg)
{
    testing_axpy_ex<Ta, Tx, Ty, Tex>(arg);
    return HIPBLAS_STATUS_SUCCESS;
}
