/* Copyright (c) 2012-2013 University of Cape Town
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
 * Basic types shared by several primitives.
 */

#ifndef CLOGS_CORE_H
#define CLOGS_CORE_H

#include <clogs/visibility_push.h>
#include <CL/cl.hpp>
#include <stdexcept>
#include <string>
#include <vector>
#include <clogs/visibility_pop.h>

/**
 * OpenCL primitives.
 *
 * The primary classes of interest are @ref Scan and @ref Radixsort, which
 * provide the algorithms. The other classes are utilities and helpers.
 */
namespace clogs
{

/// API major version number.
#define CLOGS_VERSION_MAJOR 1
/// API minor version number.
#define CLOGS_VERSION_MINOR 0

/**
 * Exception thrown on internal errors that are not the user's fault.
 */
class CLOGS_API InternalError : public std::runtime_error
{
public:
    InternalError(const std::string &msg);
};

/**
 * Exception thrown when the autotuning cache could not be read.
 */
class CLOGS_API CacheError : public InternalError
{
public:
    CacheError(const std::string &msg);
};

/**
 * Enumeration of scalar types supported by OpenCL C which can be stored in a buffer.
 */
enum CLOGS_API BaseType
{
    TYPE_VOID,
    TYPE_UCHAR,
    TYPE_CHAR,
    TYPE_USHORT,
    TYPE_SHORT,
    TYPE_UINT,
    TYPE_INT,
    TYPE_ULONG,
    TYPE_LONG,
    TYPE_HALF,
    TYPE_FLOAT,
    TYPE_DOUBLE
};

/**
 * Encapsulation of an OpenCL built-in type that can be stored in a buffer.
 *
 * An instance of this class can represent either a scalar, a vector, or the
 * @c void type.
 */
class CLOGS_API Type
{
private:
    BaseType baseType;    ///< Element type
    unsigned int length;  ///< Vector length (1 for scalars)

public:
    /// Default constructor, creating the void type.
    Type();
    /**
     * Constructor. It is deliberately not declared @c explicit, so that
     * an instance of @ref BaseType can be used where @ref Type is expected.
     *
     * @pre @a baseType is not @c TYPE_VOID.
     */
    Type(BaseType baseType, unsigned int length = 1);

    /**
     * Whether the type can be stored in a buffer and read/written in a CL C
     * program using the assignment operator.
     */
    bool isStorable(const cl::Device &device) const;

    /**
     * Whether the type can be used in expressions.
     */
    bool isComputable(const cl::Device &device) const;
    bool isIntegral() const;       ///< True if the type stores integer values.
    bool isSigned() const;         ///< True if the type is signed.
    std::string getName() const;   ///< Name of the CL C type
    ::size_t getSize() const;      ///< Size in bytes of the C API form of the type (0 for void)
    ::size_t getBaseSize() const;  ///< Size in bytes of the scalar elements (0 for void)

    /// The scalar element type.
    BaseType getBaseType() const;
    /// The vector length (1 for scalars, 0 for void).
    unsigned int getLength() const;

    /// Returns a list of all supported types
    static std::vector<Type> allTypes();
};

} // namespace clogs

#endif /* !CLOGS_CORE_H */
