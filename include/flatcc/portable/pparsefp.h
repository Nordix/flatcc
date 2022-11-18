#ifndef PPARSEFP_H
#define PPARSEFP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h> /* memcpy */

/*
 * Parses a float or double number and returns the length parsed if
 * successful. The length argument is of limited value due to dependency
 * on `strtod` - buf[len] must be accessible and must not be part of
 * a valid number, including hex float numbers..
 *
 * Unlike strtod, whitespace is not parsed.
 *
 * May return:
 * - null on error,
 * - buffer start if first character does not start a number,
 * - or end of parse on success.
 *
 */

#define PDIAGNOSTIC_IGNORE_UNUSED_FUNCTION
#include "pdiagnostic_push.h"

/*
 * isinf is needed in order to stay compatible with strtod's
 * over/underflow handling but isinf has some portability issues.
 *
 * Use the parse_double/float_is_range_error instead of isinf directly.
 * This ensures optimizations can be added when not using strtod.
 *
 * On gcc, clang and msvc we can use isinf or equivalent directly.
 * Other compilers such as xlc may require linking with -lm which may not
 * be convienent so a default isinf is provided. If isinf is available
 * and there is a noticable performance issue, define
 * `PORTABLE_USE_ISINF`. This flag also affects isnan.
 */
#if defined(__GNUC__) || defined(__clang__) || defined(_MSC_VER) || defined(PORTABLE_USE_ISINF)
#include <math.h>
#if defined(_MSC_VER) && !defined(isinf)
#include <float.h>
#define isnan _isnan
#define isinf(x) (!_finite(x))
#endif
/*
 * clang-5 through clang-8 but not clang-9 issues incorrect precision
 * loss warning with -Wconversion flag when cast is absent.
 */
#if defined(__clang__)
#if __clang_major__ >= 3 && __clang_major__ <= 8
#define parse_double_isinf(x) isinf((float)x)
#define parse_double_isnan(x) isnan((float)x)
#endif
#endif
#if !defined(parse_double_isinf)
#define parse_double_isinf isinf
#endif
#define parse_float_isinf isinf

#else

#ifndef UINT8_MAX
#include <stdint.h>
#endif

/* Avoid linking with libmath but depends on float/double being IEEE754 */
static inline int parse_double_isinf(const double x)
{
    uint64_t u64x;

    memcpy(&u64x, &x, sizeof(u64x));
    return (u64x & 0x7fffffff00000000ULL) == 0x7ff0000000000000ULL;
}

static inline int parse_float_isinf(float x)
{
    uint32_t u32x;

    memcpy(&u32x, &x, sizeof(u32x));
    return (u32x & 0x7fffffff) == 0x7f800000;
}

#endif

#if !defined(parse_double_isnan)
#define parse_double_isnan isnan
#endif
#if !defined(parse_float_isnan)
#define parse_float_isnan isnan
#endif

/* Returns 0 when in range, 1 on overflow, and -1 on underflow. */
static inline int parse_double_is_range_error(double x)
{
    return parse_double_isinf(x) ? (x < 0.0 ? -1 : 1) : 0;
}

static inline int parse_float_is_range_error(float x)
{
    return parse_float_isinf(x) ? (x < 0.0f ? -1 : 1) : 0;
}

#ifndef PORTABLE_USE_GRISU3
#define PORTABLE_USE_GRISU3 1
#endif

#if PORTABLE_USE_GRISU3
#include "grisu3_parse.h"
#endif

#ifdef grisu3_parse_double_is_defined
static inline const char *parse_double(const char *buf, size_t len, double *result)
{
    return grisu3_parse_double(buf, len, result);
}
#else
#include <stdio.h>
static inline const char *parse_double(const char *buf, size_t len, double *result)
{
    char *end;

    (void)len;
    *result = strtod(buf, &end);
    return end;
}
#endif

static inline const char *parse_float(const char *buf, size_t len, float *result)
{
    const char *end;
    double v;
    union { uint32_t u32; float f32; } inf;
    inf.u32 = 0x7f800000;

    end = parse_double(buf, len, &v);
    *result = (float)v;
    if (parse_float_isinf(*result)) {
        *result = v < 0 ? -inf.f32 : inf.f32;
        return buf;
    }
    return end;
}

/* Inspired by https://bitbashing.io/comparing-floats.html */
static inline int parse_double_compare(const double x, const double y)
{
    /* This also handles NaN */
    if (x == y) return 0;
    return x < y ? -1 : 1;
}

static inline int parse_float_compare(const float x, const float y)
{
    int32_t i32x, i32y;
    
    if (x == y) return 0;
    if (parse_float_isnan(x)) return 1;
    if (parse_float_isnan(y)) return 1;
    memcpy(&i32x, &x, sizeof(i32x));
    memcpy(&i32y, &y, sizeof(i32y));
    return i32x < i32y ? -1 : 1;
}

int parse_double_is_equal(const double x, const double y)
{
    return x == y;
}

/* Works around GCC double precisoni conversion of floats. */
static inline int parse_float_is_equal(const float x, const float y)
{
    return parse_float_compare(x, y) == 0;
}

#include "pdiagnostic_pop.h"

#ifdef __cplusplus
}
#endif

#endif /* PPARSEFP_H */
