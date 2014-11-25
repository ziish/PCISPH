/* Copyright (c) 2012 University of Cape Town
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
 * Radix-sort interface.
 */

#ifndef CLOGS_RADIXSORT_H
#define CLOGS_RADIXSORT_H

#include <clogs/visibility_push.h>
#include <CL/cl.hpp>
#include <cstddef>
#include <clogs/visibility_pop.h>

#include <clogs/core.h>

namespace clogs
{

namespace detail
{
    class Radixsort;
    class RadixsortProblem;
} // namespace detail

class Radixsort;

/**
 * Encapsulates the specifics of a radixsort problem. After construction, you
 * need to call @ref setKeyType and possibly @ref setValueType to configure the
 * sort.
 */
class CLOGS_API RadixsortProblem
{
private:
    friend class Radixsort;
    detail::RadixsortProblem *detail_;

public:
    RadixsortProblem();
    ~RadixsortProblem();
    RadixsortProblem(const RadixsortProblem &);
    RadixsortProblem &operator=(const RadixsortProblem &);

    /**
     * Set the key type for sorting.
     *
     * @param keyType      The key type
     * @throw std::invalid_argument if @a keyType is not an unsigned integral scalar type
     */
    void setKeyType(const Type &keyType);

    /**
     * Set the value type for sorting. This can be <code>Type()</code> to indicate
     * that no values will be sorted.
     */
    void setValueType(const Type &valueType);
};

/**
 * Radix-sort interface.
 *
 * One instance of this class can be re-used for multiple sorts, provided that
 *  - calls to @ref enqueue do not overlap; and
 *  - their execution does not overlap.
 *
 * An instance of the class is specialized to a specific context, device, and
 * types for the keys and values. The keys can be any unsigned integral scalar
 * type, and the values can be any built-in OpenCL type (including @c void to
 * indicate that there are no values).
 *
 * The implementation is loosely based on the reduce-then-scan strategy
 * described at http://code.google.com/p/back40computing/wiki/RadixSorting,
 * but does not appear to be as efficient.
 */
class CLOGS_API Radixsort
{
private:
    detail::Radixsort *detail_;

    /* Prevent copying */
    Radixsort(const Radixsort &);
    Radixsort &operator=(const Radixsort &);

public:
    /**
     * Constructor.
     *
     * @param context              OpenCL context to use
     * @param device               OpenCL device to use.
     * @param keyType              %Type for the keys. Must be an unsigned integral scalar type.
     * @param valueType            %Type for the values. Can be any storable type, including void.
     *
     * @throw std::invalid_argument if @a keyType is not an unsigned integral scalar type.
     * @throw std::invalid_argument if @a valueType is not a storable type for @a device.
     * @throw clogs::InternalError if there was a problem with initialization
     *
     * @deprecated This constructor is deprecated, because it does not scale to
     * future features. Use the constructor taking a @ref clogs::RadixsortProblem instead.
     */
    Radixsort(const cl::Context &context, const cl::Device &device,
              const Type &keyType, const Type &valueType = Type());

    /**
     * Constructor.
     *
     * @param context              OpenCL context to use
     * @param device               OpenCL device to use.
     * @param problem              Problem parameters.
     *
     * @throw std::invalid_argument if @a problem.keyType is not an unsigned integral scalar type.
     * @throw std::invalid_argument if @a problem.valueType is not a storable type for @a device.
     * @throw clogs::InternalError if there was a problem with initialization
     */
    Radixsort(const cl::Context &context, const cl::Device &device,
              const RadixsortProblem &problem);

    ~Radixsort(); ///< Destructor

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
     * Enqueue a sort operation on a command queue.
     *
     * If the range of the keys is known to be smaller than the range of the
     * the type holding them, passing an appropriate @a maxBits can improve
     * performance. If no maximum is known, passing 0 (the default) indicates
     * that the worst case should be assumed.
     *
     * @param commandQueue         The command queue to use.
     * @param keys                 The keys to sort.
     * @param values               The values associated with the keys.
     * @param elements             The number of elements to sort.
     * @param maxBits              Upper bound on the number of bits in any key, or 0.
     * @param events               Events to wait for before starting.
     * @param event                Event that will be signaled on completion.
     *
     * @throw cl::Error            If @a keys or @a values is not read-write.
     * @throw cl::Error            If the element range overruns either buffer.
     * @throw cl::Error            If @a elements or @a maxBits is zero.
     * @throw cl::Error            If @a maxBits is greater than the number of bits in the key type.
     * @pre
     * - @a commandQueue was created with the context and device given to the constructor.
     * - @a keys and @a values do not overlap in memory.
     * - @a maxBits is zero, or all keys are strictly less than 2<sup>@a maxBits</sup>.
     * @post
     * - After execution, the keys will be sorted (with stability), and the values will be
     *   in the same order as the keys.
     */
    void enqueue(const cl::CommandQueue &commandQueue,
                 const cl::Buffer &keys, const cl::Buffer &values,
                 ::size_t elements, unsigned int maxBits = 0,
                 const VECTOR_CLASS<cl::Event> *events = NULL,
                 cl::Event *event = NULL);

    /**
     * Set temporary buffers used during sorting. These buffers are
     * used if they are big enough (as big as the buffers that are
     * being sorted); otherwise temporary buffers are allocated on
     * the fly. Providing suitably large buffers guarantees that
     * no buffer storage is allocated by @ref enqueue.
     *
     * It is legal to set either or both values to <code>cl::Buffer()</code>
     * to clear the temporary buffer, in which case @ref enqueue will revert
     * to allocating its own temporary buffers as needed.
     *
     * This object will retain references to the buffers, so it is
     * safe for the caller to release its reference.
     */
    void setTemporaryBuffers(const cl::Buffer &keys, const cl::Buffer &values);
};

} // namespace clogs

#endif /* !CLOGS_RADIXSORT_H */
