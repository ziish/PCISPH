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
 * Internal header file that provides macros to control symbol visibility.
 * Do not include this header directly or use any of the macros it defines.
 * They are not part of the API and are subject to change.
 *
 * @see http://gcc.gnu.org/wiki/Visibility
 */

#ifndef CLOGS_VISIBILITY_H
#define CLOGS_VISIBILITY_H

#if defined(_WIN32) || defined(__CYGWIN__)
# define CLOGS_DLL_IMPORT __declspec(dllimport)
# define CLOGS_DLL_EXPORT __declspec(dllexport)
# define CLOGS_DLL_LOCAL
#else
# if __GNUC__ >= 4
#  define CLOGS_DLL_DO_PUSH_POP
#  define CLOGS_DLL_IMPORT __attribute__((visibility("default")))
#  define CLOGS_DLL_EXPORT __attribute__((visibility("default")))
#  define CLOGS_DLL_LOCAL __attribute__((visibility("hidden")))
# else
#  define CLOGS_DLL_IMPORT
#  define CLOGS_DLL_EXPORT
#  define CLOGS_DLL_LOCAL
# endif
#endif

#ifdef CLOGS_DLL_DO_STATIC  /* defined by build system in static lib builds */
# define CLOGS_API CLOGS_DLL_LOCAL
#else
/* CLOGS_DLL_DO_EXPORT is defined by the build system when building the library */
#ifdef CLOGS_DLL_DO_EXPORT
# define CLOGS_API CLOGS_DLL_EXPORT
#else
# define CLOGS_API CLOGS_DLL_IMPORT
#endif
#endif
#define CLOGS_LOCAL CLOGS_DLL_LOCAL

#endif /* !CLOGS_VISIBILITY_H */
