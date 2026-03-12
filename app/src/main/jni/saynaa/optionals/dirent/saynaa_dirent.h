/*
 * Copyright (c) 2022-2025 Mohamed Abdifatah. All rights reserved.
 * Distributed Under The MIT License
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Windows Implementation
#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <stdio.h>
#include <windows.h>

#ifndef NAME_MAX
#define NAME_MAX 260 // MAX_PATH
#endif

// Define standard dirent structs for Windows
struct dirent {
  char d_name[NAME_MAX + 1];
  // We only implement what Saynaa uses currently.
  // Add d_type, d_ino etc if needed later.
};

typedef struct {
  HANDLE hFind;
  WIN32_FIND_DATAW data;
  int first_read;
  struct dirent cur_ent;
} DIR;

DIR* opendir(const char* name);
struct dirent* readdir(DIR* dir);
int closedir(DIR* dir);

#else

// POSIX Implementation (Linux, macOS, etc)
#include <dirent.h>

#endif

#ifdef __cplusplus
}
#endif
