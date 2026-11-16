/* SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only */
/*
 * YT8531C portable PHY driver - basic types, error codes and helpers.
 *
 * This header is self-contained and freestanding: it includes no C or C++
 * standard library headers and only relies on compiler-provided type macros
 * (with plain scalar fallbacks for compilers that do not define them).  It is
 * therefore usable from a Linux kernel module, from a freestanding userspace
 * library and from a C consumer of the C++ implementation.
 */

#ifndef YT8531C_TYPES_H
#define YT8531C_TYPES_H

/*
 * ABI version of the public structures below.  Bump the major part whenever a
 * structure layout changes and extend the reserved areas instead of growing
 * existing structures so that binaries built against an older header keep
 * working with a newer implementation.
 */
#define YT8531C_VERSION_MAJOR 1
#define YT8531C_VERSION_MINOR 0
#define YT8531C_VERSION_PATCH 0
#define YT8531C_VERSION_MAKE(major, minor, patch) \
	(((yt_u32)(major) << 16) | ((yt_u32)(minor) << 8) | (yt_u32)(patch))
#define YT8531C_VERSION \
	YT8531C_VERSION_MAKE(YT8531C_VERSION_MAJOR, YT8531C_VERSION_MINOR, \
			     YT8531C_VERSION_PATCH)

#if defined(__UINT8_TYPE__) && defined(__UINT16_TYPE__) && \
	defined(__UINT32_TYPE__) && defined(__UINT64_TYPE__)
typedef __UINT8_TYPE__ yt_u8;
typedef __UINT16_TYPE__ yt_u16;
typedef __UINT32_TYPE__ yt_u32;
typedef __UINT64_TYPE__ yt_u64;
typedef __INT8_TYPE__ yt_s8;
typedef __INT16_TYPE__ yt_s16;
typedef __INT32_TYPE__ yt_s32;
typedef __INT64_TYPE__ yt_s64;
#if defined(__SIZE_TYPE__)
typedef __SIZE_TYPE__ yt_size;
#else
typedef __UINT64_TYPE__ yt_size;
#endif
#else
typedef unsigned char yt_u8;
typedef unsigned short yt_u16;
typedef unsigned int yt_u32;
typedef unsigned long long yt_u64;
typedef signed char yt_s8;
typedef signed short yt_s16;
typedef signed int yt_s32;
typedef signed long long yt_s64;
typedef unsigned long long yt_size;
#endif

/* Return type used by every public entry point. */
typedef int yt_rc;

#define YT8531C_OK 0
#define YT8531C_ERR_ARG (-1)	     /* invalid argument */
#define YT8531C_ERR_IO (-2)	     /* MDIO transfer failed */
#define YT8531C_ERR_TIMEOUT (-3)     /* hardware did not respond in time */
#define YT8531C_ERR_ID (-4)	     /* not a supported PHY */
#define YT8531C_ERR_UNSUPPORTED (-5) /* valid request, unsupported by chip */
#define YT8531C_ERR_STATE (-6)	     /* operation not valid in this state */
#define YT8531C_ERR_RANGE (-7)	     /* value outside the documented range */
#define YT8531C_ERR_VERIFY (-8)	     /* read back did not match */

/* Log levels passed to the optional port log callback. */
#define YT8531C_LOG_DEBUG 0
#define YT8531C_LOG_INFO 1
#define YT8531C_LOG_WARN 2
#define YT8531C_LOG_ERROR 3

/* Sentinel meaning "leave the hardware value untouched". */
#define YT8531C_AUTO 0xFFu

/* Magic value stored in yt8531c_dev to catch uninitialised contexts. */
#define YT8531C_DEV_MAGIC 0x59543835u /* "YT85" */

#if defined(__cplusplus)
#define YT8531C_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#define YT8531C_EXTERN_C extern "C"
#else
#define YT8531C_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#define YT8531C_EXTERN_C
#endif

#ifndef YT8531C_BIT
#define YT8531C_BIT(n) (1u << (n))
#endif
#ifndef YT8531C_GENMASK
#define YT8531C_GENMASK(hi, lo) \
	(((~0u) << (lo)) & (~0u >> (31 - (hi))))
#endif
#ifndef YT8531C_FIELD_GET
#define YT8531C_FIELD_GET(mask, val) \
	(((yt_u32)(val) & (mask)) >> (__builtin_ctz((mask))))
#endif
#ifndef YT8531C_FIELD_PREP
#define YT8531C_FIELD_PREP(mask, val) \
	(((yt_u32)(val) << (__builtin_ctz((mask)))) & (mask))
#endif
#ifndef YT8531C_ARRAY_SIZE
#define YT8531C_ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

#if defined(__GNUC__) || defined(__clang__)
#define YT8531C_WARN_UNUSED __attribute__((warn_unused_result))
#define YT8531C_NONNULL(...) __attribute__((nonnull(__VA_ARGS__)))
#define YT8531C_PACKED __attribute__((packed))
#else
#define YT8531C_WARN_UNUSED
#define YT8531C_NONNULL(...)
#define YT8531C_PACKED
#endif

#endif /* YT8531C_TYPES_H */
