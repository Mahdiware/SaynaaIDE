/*
 * Copyright (c) 2022-2026 Mohamed Abdifatah. All rights reserved.
 * Distributed Under The MIT License
 */

#pragma once

#include "../cli/saynaa.h"
#include "saynaa_common.h"

// Commonly used C standard headers across the sources. Don't include any
// headers that are specific to a single source here, instead include them in
// their source files explicitly (can not be implicitly included by another
// header).
#include <assert.h>
#include <errno.h>
#include <float.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// __STDC_LIMIT_MACROS and __STDC_CONSTANT_MACROS are a workaround to
// allow C++ programs to use stdint.h macros specified in the C99
// standard that aren't in the C++ standard.
#define __STDC_LIMIT_MACROS
#include <stdint.h>

#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#elif defined(__clang__)
#pragma clang diagnostic ignored "-Wint-to-pointer-cast"
#pragma clang diagnostic ignored "-Wunused-parameter"
#elif defined(_MSC_VER)
#pragma warning(disable : 26812)
#endif

/*****************************************************************************/
/* INTERNAL MACROS                                                           */
/*****************************************************************************/

// The file extension for the Saynaa script files.
#define SAYNAA_FILE_EXT ".sa"

// The maximum path length for a module to import.
// This is an arbitrary value and can be changed if needed.
#define MAX_PATH_LEN 4096

// The maximum number of locals or global (if compiling top level module)
// to lookup from the compiling context. Also it's limited by it's opcode
// which is using a single byte value to identify the local.
#define MAX_VARIABLES 256

// The maximum number of constant literal a module can contain. Also it's
// limited by it's opcode which is using a short value to identify.
#define MAX_CONSTANTS (1 << 16)

// The maximum number of upvalues a literal function can capture from it's
// enclosing function.
#define MAX_UPVALUES 256

// The maximum number of names that were used before defined. Its just the size
// of the Forward buffer of the compiler. Feel free to increase it if it
// require more.
#define MAX_FORWARD_NAMES 256

// two types of interpolation.
//
//   1. Name interpolation       ex: "Hello $name!"
//   2. Expression interpolation ex: "Hello ${getName()}!"
//                               ex: "Result ${20+10}"
//
// Consider a string: "a ${ b "c ${d}" } e" -- Here the depth of 'b' is 1 and
// the depth of 'd' is 2 and so on. The maximum depth an expression can go is
// defined as MAX_STR_INTERP_DEPTH below.
#define MAX_STR_INTERP_DEPTH 32

// The maximum address possible to jump. Similar limitation as above.
#define MAX_JUMP (1 << 16)

// Max number of break statement in a loop statement to patch.
#define MAX_BREAK_PATCH 256

// Set this to dump compiled opcodes of each functions.
#define DUMP_BYTECODE 0

// Dump the stack values and the globals.
#define DUMP_STACK 0

// Nan-Tagging could be disable for debugging/portability purposes. See "var.h"
// header for more information on Nan-tagging.
#define VAR_NAN_TAGGING 1

// The maximum size of the stack. This value is arbitrary. currently
// it's 100 MB. Change this to any value upto 2147483647 (signed integer max)
// if you want.
#define MAX_STACK_SIZE 100 * (1024 * 1024)

// The maximum number of argument function supported to call. This
// value is arbitrary and feel free to change it. (Just used this limit for an
// internal buffer to store values before calling a new fiber).
#define MAX_ARGC 32

// The factor by which a buffer will grow when it's capacity reached.
#define GROW_FACTOR 2

// The initial minimum capacity of a buffer to allocate.
#define MIN_CAPACITY 8

// The size of the error message buffer, used ar vsnprintf (since c99) buffer.
#define ERROR_MESSAGE_SIZE 512

// The maximum number of temporary object reference to protect them from being
// garbage collected.
#define MAX_TEMP_REFERENCE 64

// The capacity of the builtin function array in the VM.
#define BUILTIN_FN_CAPACITY 50

// Initially allocated call frame capacity. Will grow dynamically.
#define INITIAL_CALL_FRAMES 4

// The minimum size of the stack that will be initialized for a fiber before
// running one.
#define MIN_STACK_SIZE 128

// The allocated size that will trigger the first GC. (~10MB).
#define INITIAL_GC_SIZE (1024 * 1024 * 10)

// The heap size might shrink if the remaining allocated bytes after a GC
// is less than the one before the last GC. So we need a minimum size.
#define MIN_HEAP_SIZE (1024 * 1024)

// The heap size for the next GC will be calculated as the bytes we have
// allocated so far plus the fill factor of it.
#define HEAP_FILL_PERCENT 75

// Here we're switching the FNV-1a hash value of the name (cstring). Which is
// an efficient way than having multiple if (attrib == "name"). From O(n) * k
// to O(1) where n is the length of the string and k is the number of string
// comparison.
//
// ex:
//     switch (attrib->hash) { // str = "length"
//       case CHECK_HASH("length", 0x83d03615) : { return string->length; }
//     }
//
#define CHECK_HASH(name, hash) hash

// The formated string to convert double to string. It'll be with the minimum
// length string representation of either a regular float or a scientific
// notation (at most 15 decimal points).
// Reference: https://www.cplusplus.com/reference/cstdio/printf/
#define DOUBLE_FMT "%.16g"

// Double number to string buffer size, used in sprintf with DOUBLE_FMT.
//  A largest number : "-1.234567890123456e+308"
// +  1 fot sign '+' or '-'
// + 16 fot significant digits
// +  1 for decimal point '.'
// +  1 for exponent char 'e'
// +  1 for sign of exponent
// +  3 for the exponent digits
// +  1 for null byte '\0'
#define STR_DBL_BUFF_SIZE 24

// Integer number to string buffer size, used in sprintf with format "%d".
// The minimum 32 bit integer = -2147483648
// +  1 for sign '-'
// + 10 for digits
// +  1 for null byte '\0'
#define STR_INT_BUFF_SIZE 12

// Integer number (double) to hex string buffer size.
// The maximum value an unsigned 64 bit integer can get is
// 0xffffffffffffffff which is 16 characters.
// + 16 for hex digits
// +  1 for sign '-'
// +  2 for '0x' prefix
// +  1 for null byte '\0'
#define STR_HEX_BUFF_SIZE 20

// Integer number (double) to bin string buffer size.
// The maximum value an unsigned 64 bit integer can get is 0b11111... 64 1s.
// + 64 for bin digits
// +  1 for sign '-'
// +  2 for '0b' prefix
// +  1 for null byte '\0'
#define STR_BIN_BUFF_SIZE 68

/*****************************************************************************/
/* ALLOCATION MACROS                                                         */
/*****************************************************************************/

// Allocate object of [type] using the vmRealloc function.
#define ALLOCATE(vm, type) ((type*) vmRealloc(vm, NULL, 0, sizeof(type)))

// Allocate object of [type] which has a dynamic tail array of type [tail_type]
// with [count] entries.
#define ALLOCATE_DYNAMIC(vm, type, count, tail_type) \
  ((type*) vmRealloc(vm, NULL, 0, sizeof(type) + sizeof(tail_type) * (count)))

// Allocate [count] amount of object of [type] array.
#define ALLOCATE_ARRAY(vm, type, count) \
  ((type*) vmRealloc(vm, NULL, 0, sizeof(type) * (count)))

// Deallocate a pointer allocated by vmRealloc before.
#define DEALLOCATE(vm, pointer, type) vmRealloc(vm, pointer, sizeof(type), 0)

// Deallocate object of [type] which has a dynamic tail array of type
// [tail_type] with [count] entries.
#define DEALLOCATE_DYNAMIC(vm, pointer, type, count, tail_type) \
  ((type*) vmRealloc(vm, pointer, sizeof(type) + sizeof(tail_type) * (count), 0))

// Deallocate [count] amount of object of [type] array.
#define DEALLOCATE_ARRAY(vm, pointer, type, count) \
  ((type*) vmRealloc(vm, pointer, sizeof(type) * (count), 0))
