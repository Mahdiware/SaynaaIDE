/*
 * Copyright (c) 2022-2026 Mohamed Abdifatah. All rights reserved.
 * Distributed Under The MIT License
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Returns the platform specific path separator.
char saynaa_path_separator(void);

// Returns true if the path is absolute.
bool saynaa_path_is_absolute(const char* path);

// Generates an absolute path based on a base.
// Returns the length of the result.
size_t saynaa_path_get_absolute(const char* base, const char* path,
                                char* buffer, size_t buffer_size);

// Generates a relative path based on a base.
// Returns the length of the result.
size_t saynaa_path_get_relative(const char* base_directory, const char* path,
                                char* buffer, size_t buffer_size);

// Joins two paths.
size_t saynaa_path_join(const char* path_a, const char* path_b, char* buffer, size_t buffer_size);

// Normalizes the path.
size_t saynaa_path_normalize(const char* path, char* buffer, size_t buffer_size);

// Gets the directory name (length of the prefix).
void saynaa_path_dirname(const char* path, size_t* length);

// Gets the base name.
void saynaa_path_basename(const char* path, const char** basename, size_t* length);

// Gets the extension.
bool saynaa_path_extension(const char* path, const char** extension, size_t* length);

#ifdef __cplusplus
}
#endif
