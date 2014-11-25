/* Copyright (c) 2012 University of Cape Town
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
 */

/**
 * @file
 *
 * OpenCL primitives.
 */

#ifndef CLOGS_CLOGS_H
#define CLOGS_CLOGS_H

#include <clogs/visibility_push.h>
#include <CL/cl.hpp>
#include <string>
#include <stdexcept>
#include <clogs/visibility_pop.h>

#include <clogs/core.h>
#include <clogs/scan.h>
#include <clogs/reduce.h>
#include <clogs/radixsort.h>

/**
 * @mainpage
 *
 * Please refer to the user manual for an introduction to CLOGS, or the
 * @ref clogs namespace page for reference documentation of the classes.
 */

/**
 * OpenCL primitives.
 *
 * The primary classes of interest are @ref Scan, @ref Reduce and @ref Radixsort, which
 * provide the algorithms. The other classes are utilities and helpers.
 */
namespace clogs
{
} // namespace clogs

#endif /* !CLOGS_CLOGS_H */
