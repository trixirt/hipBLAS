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

using hipblasRotgStridedBatchedModel = ArgumentModel<e_a_type, e_stride_scale, e_batch_count>;

inline void testname_rotg_strided_batched(const Arguments& arg, std::string& name)
{
    hipblasRotgStridedBatchedModel{}.test_name(arg, name);
}

template <typename T>
void testing_rotg_strided_batched(const Arguments& arg)
{
    using U      = real_t<T>;
    bool FORTRAN = arg.api == hipblas_client_api::FORTRAN;
    auto hipblasRotgStridedBatchedFn
        = FORTRAN ? hipblasRotgStridedBatched<T, U, true> : hipblasRotgStridedBatched<T, U, false>;

    double        stride_scale = arg.stride_scale;
    hipblasStride stride_a     = stride_scale;
    hipblasStride stride_b     = stride_scale;
    hipblasStride stride_c     = stride_scale;
    hipblasStride stride_s     = stride_scale;
    int           batch_count  = arg.batch_count;

    const U rel_error = std::numeric_limits<U>::epsilon() * 1000;

    // check to prevent undefined memory allocation error
    if(batch_count <= 0)
        return;

    double gpu_time_used, hipblas_error_host, hipblas_error_device;

    hipblasLocalHandle handle(arg);

    size_t size_a = size_t(stride_a) * size_t(batch_count);
    size_t size_b = size_t(stride_b) * size_t(batch_count);
    size_t size_c = size_t(stride_c) * size_t(batch_count);
    size_t size_s = size_t(stride_s) * size_t(batch_count);

    host_vector<T> ha(size_a);
    host_vector<T> hb(size_b);
    host_vector<U> hc(size_c);
    host_vector<T> hs(size_s);

    // Initial data on CPU
    hipblas_init_vector(ha, arg, 1, 1, stride_a, batch_count, hipblas_client_alpha_sets_nan, true);
    hipblas_init_vector(hb, arg, 1, 1, stride_b, batch_count, hipblas_client_alpha_sets_nan, false);
    hipblas_init_vector(hc, arg, 1, 1, stride_c, batch_count, hipblas_client_alpha_sets_nan, false);
    hipblas_init_vector(hs, arg, 1, 1, stride_s, batch_count, hipblas_client_alpha_sets_nan, false);

    // CPU_BLAS
    host_vector<T> ca = ha;
    host_vector<T> cb = hb;
    host_vector<U> cc = hc;
    host_vector<T> cs = hs;

    // result vector for hipBLAS device
    host_vector<T> ra = ha;
    host_vector<T> rb = hb;
    host_vector<U> rc = hc;
    host_vector<T> rs = hs;

    device_vector<T> da(size_a);
    device_vector<T> db(size_b);
    device_vector<U> dc(size_c);
    device_vector<T> ds(size_s);

    ASSERT_HIP_SUCCESS(hipMemcpy(da, ha, sizeof(T) * size_a, hipMemcpyHostToDevice));
    ASSERT_HIP_SUCCESS(hipMemcpy(db, hb, sizeof(T) * size_b, hipMemcpyHostToDevice));
    ASSERT_HIP_SUCCESS(hipMemcpy(dc, hc, sizeof(U) * size_c, hipMemcpyHostToDevice));
    ASSERT_HIP_SUCCESS(hipMemcpy(ds, hs, sizeof(T) * size_s, hipMemcpyHostToDevice));

    if(arg.unit_check || arg.norm_check)
    {
        ASSERT_HIPBLAS_SUCCESS(hipblasSetPointerMode(handle, HIPBLAS_POINTER_MODE_HOST));
        ASSERT_HIPBLAS_SUCCESS((hipblasRotgStridedBatchedFn(
            handle, ha, stride_a, hb, stride_b, hc, stride_c, hs, stride_s, batch_count)));

        ASSERT_HIPBLAS_SUCCESS(hipblasSetPointerMode(handle, HIPBLAS_POINTER_MODE_DEVICE));
        ASSERT_HIPBLAS_SUCCESS((hipblasRotgStridedBatchedFn(
            handle, da, stride_a, db, stride_b, dc, stride_c, ds, stride_s, batch_count)));

        ASSERT_HIP_SUCCESS(hipMemcpy(ra, da, sizeof(T) * size_a, hipMemcpyDeviceToHost));
        ASSERT_HIP_SUCCESS(hipMemcpy(rb, db, sizeof(T) * size_b, hipMemcpyDeviceToHost));
        ASSERT_HIP_SUCCESS(hipMemcpy(rc, dc, sizeof(U) * size_c, hipMemcpyDeviceToHost));
        ASSERT_HIP_SUCCESS(hipMemcpy(rs, ds, sizeof(T) * size_s, hipMemcpyDeviceToHost));

        for(int b = 0; b < batch_count; b++)
        {
            cblas_rotg<T, U>(ca.data() + b * stride_a,
                             cb.data() + b * stride_b,
                             cc.data() + b * stride_c,
                             cs.data() + b * stride_s);
        }

        if(arg.unit_check)
        {
            near_check_general<T>(1, 1, batch_count, 1, stride_a, ca, ha, rel_error);
            near_check_general<T>(1, 1, batch_count, 1, stride_b, cb, hb, rel_error);
            near_check_general<U>(1, 1, batch_count, 1, stride_c, cc, hc, rel_error);
            near_check_general<T>(1, 1, batch_count, 1, stride_s, cs, hs, rel_error);

            near_check_general<T>(1, 1, batch_count, 1, stride_a, ca, ra, rel_error);
            near_check_general<T>(1, 1, batch_count, 1, stride_b, cb, rb, rel_error);
            near_check_general<U>(1, 1, batch_count, 1, stride_c, cc, rc, rel_error);
            near_check_general<T>(1, 1, batch_count, 1, stride_s, cs, rs, rel_error);
        }

        if(arg.norm_check)
        {
            hipblas_error_host = norm_check_general<T>('F', 1, 1, 1, stride_a, ca, ha, batch_count);
            hipblas_error_host
                += norm_check_general<T>('F', 1, 1, 1, stride_b, cb, hb, batch_count);
            hipblas_error_host
                += norm_check_general<U>('F', 1, 1, 1, stride_c, cc, hc, batch_count);
            hipblas_error_host
                += norm_check_general<T>('F', 1, 1, 1, stride_s, cs, hs, batch_count);

            hipblas_error_device
                = norm_check_general<T>('F', 1, 1, 1, stride_a, ca, ra, batch_count);
            hipblas_error_device
                += norm_check_general<T>('F', 1, 1, 1, stride_b, cb, rb, batch_count);
            hipblas_error_device
                += norm_check_general<U>('F', 1, 1, 1, stride_c, cc, rc, batch_count);
            hipblas_error_device
                += norm_check_general<T>('F', 1, 1, 1, stride_s, cs, rs, batch_count);
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

            ASSERT_HIPBLAS_SUCCESS((hipblasRotgStridedBatchedFn(
                handle, da, stride_a, db, stride_b, dc, stride_c, ds, stride_s, batch_count)));
        }
        gpu_time_used = get_time_us_sync(stream) - gpu_time_used;

        hipblasRotgStridedBatchedModel{}.log_args<T>(std::cout,
                                                     arg,
                                                     gpu_time_used,
                                                     ArgumentLogging::NA_value,
                                                     ArgumentLogging::NA_value,
                                                     hipblas_error_host,
                                                     hipblas_error_device);
    }
}

template <typename T>
hipblasStatus_t testing_rotg_strided_batched_ret(const Arguments& arg)
{
    testing_rotg_strided_batched<T>(arg);
    return HIPBLAS_STATUS_SUCCESS;
}
