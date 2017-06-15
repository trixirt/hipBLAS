/* ************************************************************************
 * Copyright 2016 Advanced Micro Devices, Inc.
 *
 * ************************************************************************ */

#include <stdlib.h>
#include <stdio.h>
#include <vector>

#include "hipblas.hpp"
#include "utility.h"
#include "cblas_interface.h"
#include "norm.h"
#include "unit.h"
#include <complex.h>

using namespace std;

/* ============================================================================================ */

template<typename T1, typename T2>
hipblasStatus_t testing_asum(Arguments argus)
{

    int N = argus.N;
    int incx = argus.incx;

    hipblasStatus_t status = HIPBLAS_STATUS_SUCCESS;

    //check to prevent undefined memory allocation error
    if( N < 0 || incx < 0 ){
        status = HIPBLAS_STATUS_INVALID_VALUE;
        return status;
    }

    int sizeX = N * incx;

    //Naming: dX is in GPU (device) memory. hK is in CPU (host) memory, plz follow this practice
    vector<T1> hx(sizeX);

    T1 *dx;
    T2 *d_rocblas_result;
    T2 cpu_result, rocblas_result;

    int device_pointer = 1;

    double gpu_time_used, cpu_time_used;
    double rocblas_error;

    hipblasHandle_t handle;
    hipblasCreate(&handle);

    //allocate memory on device
    CHECK_HIP_ERROR(hipMalloc(&dx, sizeX * sizeof(T1)));
    CHECK_HIP_ERROR(hipMalloc(&d_rocblas_result, sizeof(T2)));

    //Initial Data on CPU
    srand(1);
    hipblas_init<T1>(hx, 1, N, incx);

    //copy data from CPU to device, does not work for incx != 1
    CHECK_HIP_ERROR(hipMemcpy(dx, hx.data(), sizeof(T1)*N*incx, hipMemcpyHostToDevice));


    /* =====================================================================
         ROCBLAS
    =================================================================== */
     /* =====================================================================
                 CPU BLAS
     =================================================================== */
     //hipblasAsum accept both dev/host pointer for the scalar
    if(device_pointer){
        status = hipblasAsum<T1, T2>(handle,
                        N,
                        dx, incx,
                        d_rocblas_result);
    }
    else{
        status = hipblasAsum<T1, T2>(handle,
                        N,
                        dx, incx,
                        &rocblas_result);
    }

    if (status != HIPBLAS_STATUS_SUCCESS) {
        CHECK_HIP_ERROR(hipFree(dx));
        CHECK_HIP_ERROR(hipFree(d_rocblas_result));
        hipblasDestroy(handle);
        return status;
    }

    if(device_pointer)    CHECK_HIP_ERROR(hipMemcpy(&rocblas_result, d_rocblas_result, sizeof(T1), hipMemcpyDeviceToHost));

    if(argus.unit_check){

     /* =====================================================================
                 CPU BLAS
     =================================================================== */

        cblas_asum<T1, T2>(N,
                    hx.data(), incx,
                    &cpu_result);



        if(argus.unit_check){
            unit_check_general<T2>(1, 1, 1, &cpu_result, &rocblas_result);
        }

    }// end of if unit/norm check


//  BLAS_1_RESULT_PRINT


    CHECK_HIP_ERROR(hipFree(dx));
    CHECK_HIP_ERROR(hipFree(d_rocblas_result));
    hipblasDestroy(handle);
    return HIPBLAS_STATUS_SUCCESS;
}
