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

#include "testing_exceptions.hpp"
#include "utility.h"
#include <math.h>
#include <stdexcept>
#include <vector>

namespace
{

    TEST(hipblas_auxiliary, statusToString)
    {
        EXPECT_EQ(0,
                  strcmp("HIPBLAS_STATUS_ALLOC_FAILED",
                         hipblasStatusToString(HIPBLAS_STATUS_ALLOC_FAILED)));
    }

    TEST(hipblas_auxiliary, badOperation)
    {
        EXPECT_EQ(testing_bad_operation(), HIPBLAS_STATUS_INVALID_ENUM);
    }

    TEST(hipblas_auxiliary, createHandle)
    {
        EXPECT_EQ(testing_handle(), HIPBLAS_STATUS_SUCCESS);
    }

} // namespace
