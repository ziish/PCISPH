/* Copyright (c) 2012, 2014 University of Cape Town
 * Copyright (c) 2014, Bruce Merry
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
 * Scan primitive.
 */

#ifndef CLOGS_SCAN_H
#define CLOGS_SCAN_H

#include <clogs/visibility_push.h>
#include <CL/cl.hpp>
#include <cstddef>
#include <clogs/visibility_pop.h>

#include <clogs/core.h>

namespace clogs
{

namespace detail
{
    class Scan;
    class ScanProblem;
} // namespace detail

class Scan;

/**
 * Encapsulates the specifics of a scan problem. After construction, use
 * methods (particularly @ref setType) to configure the scan.
 */
class CLOGS_API ScanProblem
{
private:
    friend class Scan;
    detail::ScanProblem *detail_;

public:
    ScanProblem();
    ~ScanProblem();
    ScanProblem(const ScanProblem &);
    ScanProblem &operator=(const ScanProblem &);

    /**
     * Set the element type for the scan.
     *
     * @param type      The element type
     * @throw std::invalid_argument if @a type is not an integral scalar or vector type
     */
    void setType(const Type &type);
};

/**
 * Exclusive scan (prefix sum) primitive.
 *
 * One instance of this class can be reused for multiple scans, provided that
 *  - calls to @ref enqueue do not overlap; and
 *  - their execution does not overlap.
 *
 * An instance of the class is specialized to a specific context, device, and
 * type of value to scan. Any CL integral scalar or vector type can
 * be used.
 *
 * The implementation is based on the reduce-then-scan strategy described at
 * https://sites.google.com/site/duanemerrill/ScanTR2.pdf?attredirects=0
 */
class CLOGS_API Scan
{
private:
    detail::Scan *detail_;

    /* Prevent copying */
    Scan(const Scan &);
    Scan &operator=(const Scan &);

public:
    /**
     * Constructor.
     *
     * @param context              OpenCL context to use
     * @param device               OpenCL device to use.
     * @param type                 %Type of the values to scan.
     *
     * @throw std::invalid_argument if @a type is not an integral type supported on the device.
     * @throw clogs::InternalError if there was a problem with initialization.
     *
     * @deprecated This interface is deprecated as it does not scale with future feature
     * additions. Use the constructor taking a @ref clogs::ScanProblem instead.
     */
    Scan(const cl::Context &context, const cl::Device &device, const Type &type);

    /**
     * Constructor.
     *
     * @param context              OpenCL context to use
     * @param device               OpenCL device to use.
     * @param problem              Description of the specific scan problem.
     *
     * @throw std::invalid_argument if @a problem is not supported on the device or is not initialized.
     * @throw clogs::InternalError if there was a problem with initialization.
     */
    Scan(const cl::Context &context, const cl::Device &device, const ScanProblem &problem);

    ~Scan(); ///< Destructor

    /**
     * Set a callback function that will receive a list of all underlying events.
     * The callback will be called multiple times during each enqueue, because
     * the implementation uses multiple commands. This allows profiling information
     * to be extracted from the events once they complete.
     *
     * The callback may also be set to @c NULL to disable it.
     *
     * @note This is not an event completion callback: it is called during
     * @c enqueue, generally before the events complete.
     *
     * @param callback The callback function.
     * @param userData Arbitrary data to be passed to the callback.
     */
    void setEventCallback(void (CL_CALLBACK *callback)(const cl::Event &, void *), void *userData);

    /**
     * Enqueue a scan operation on a command queue (in-place).
     *
     * This is equivalent to calling
     * @c enqueue(@a commandQueue, @a buffer, @a buffer, @a elements, @a offset, @a events, @a event);
     */
    void enqueue(const cl::CommandQueue &commandQueue,
                 const cl::Buffer &buffer,
                 ::size_t elements,
                 const void *offset = NULL,
                 const VECTOR_CLASS<cl::Event> *events = NULL,
                 cl::Event *event = NULL);

    /**
     * Enqueue a scan operation on a command queue.
     *
     * An initial offset may optionally be passed in @a offset, which will be
     * added to all elements of the result. The pointer must point to the
     * type of element specified to the constructor. If no offset is desired,
     * @c NULL may be passed instead.
     *
     * The input and output buffers may be the same to do an in-place scan.
     *
     * @param commandQueue         The command queue to use.
     * @param inBuffer             The buffer to scan.
     * @param outBuffer            The buffer to fill with output.
     * @param elements             The number of elements to scan.
     * @param offset               The offset to add to all elements, or @c NULL.
     * @param events               Events to wait for before starting.
     * @param event                Event that will be signaled on completion.
     *
     * @throw cl::Error            If @a inBuffer is not readable on the device.
     * @throw cl::Error            If @a outBuffer is not writable on the device.
     * @throw cl::Error            If the element range overruns the buffer.
     * @throw cl::Error            If @a elements is zero.
     * @pre
     * - @a commandQueue was created with the context and device given to the constructor.
     * @post
     * - After execution, element @c i will be replaced by the sum of all elements strictly
     *   before @c i, plus the @a offset (if any).
     */
    void enqueue(const cl::CommandQueue &commandQueue,
                 const cl::Buffer &inBuffer,
                 const cl::Buffer &outBuffer,
                 ::size_t elements,
                 const void *offset = NULL,
                 const VECTOR_CLASS<cl::Event> *events = NULL,
                 cl::Event *event = NULL);

    /**
     * Enqueue a scan operation on a command queue, with an offset in a buffer (in-place).
     *
     * This is equivalent to calling
     * @c enqueue(@a commandQueue, @a buffer, @a buffer, @a elements, @a offsetBuffer, @a offsetIndex, @a events, @a event);
     */
    void enqueue(const cl::CommandQueue &commandQueue,
                 const cl::Buffer &buffer,
                 ::size_t elements,
                 const cl::Buffer &offsetBuffer,
                 cl_uint offsetIndex,
                 const VECTOR_CLASS<cl::Event> *events = NULL,
                 cl::Event *event = NULL);

    /**
     * Enqueue a scan operation on a command queue, with an offset in a buffer.
     *
     * The offset is of the same type as the elements to be scanned, and is
     * stored in a buffer. It is added to all elements of the result. It is legal
     * for the offset to be in the same buffer as the values to scan, and it may
     * even be safely overwritten by the scan (it will be read before being
     * overwritten). This makes it possible to use do multi-pass algorithms with
     * variable output. The counting pass fills in the desired allocations, a
     * scan is used with one extra element at the end to hold the grand total,
     * and the subsequent passes use this extra element as the offset.
     *
     * The input and output buffers may be the same to do an in-place scan.
     *
     * @param commandQueue         The command queue to use.
     * @param inBuffer             The buffer to scan.
     * @param outBuffer            The buffer to fill with output.
     * @param elements             The number of elements to scan.
     * @param offsetBuffer         Buffer containing a value to add to all elements.
     * @param offsetIndex          Index (in units of the scan type) into @a offsetBuffer.
     * @param events               Events to wait for before starting.
     * @param event                Event that will be signaled on completion.
     *
     * @throw cl::Error            If @a inBuffer is not readable on the device.
     * @throw cl::Error            If @a outBuffer is not writable on the device.
     * @throw cl::Error            If the element range overruns the buffer.
     * @throw cl::Error            If @a elements is zero.
     * @throw cl::Error            If @a offsetBuffer is not readable.
     * @throw cl::Error            If @a offsetIndex overruns @a offsetBuffer.
     * @pre
     * - @a commandQueue was created with the context and device given to the constructor.
     * @post
     * - After execution, element @c i will be replaced by the sum of all elements strictly
     *   before @c i, plus the offset.
     */
    void enqueue(const cl::CommandQueue &commandQueue,
                 const cl::Buffer &inBuffer,
                 const cl::Buffer &outBuffer,
                 ::size_t elements,
                 const cl::Buffer &offsetBuffer,
                 cl_uint offsetIndex,
                 const VECTOR_CLASS<cl::Event> *events = NULL,
                 cl::Event *event = NULL);
};

} // namespace clogs

#endif /* !CLOGS_SCAN_H */
