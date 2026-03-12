/*
 * Copyright (c) 2022-2025 Mohamed Abdifatah. All rights reserved.
 * Distributed Under The MIT License
 */

#include "saynaa_dirent.h"

// Only compile implementation on Windows
#if defined(_WIN32)

#include <stdlib.h>
#include <string.h>

static void str_to_wstr(const char* src, wchar_t* dst, size_t max_len) {
  if (max_len == 0)
    return;
  int res = MultiByteToWideChar(CP_UTF8, 0, src, -1, dst, (int) max_len);
  if (res == 0 && max_len > 0)
    dst[0] = 0;
}

static void wstr_to_str(const wchar_t* src, char* dst, size_t max_len) {
  if (max_len == 0)
    return;
  int res = WideCharToMultiByte(CP_UTF8, 0, src, -1, dst, (int) max_len, NULL, NULL);
  if (res == 0 && max_len > 0)
    dst[0] = 0;
}

DIR* opendir(const char* name) {
  if (name == NULL || *name == '\0')
    return NULL;

  DIR* dir = (DIR*) malloc(sizeof(DIR));
  if (!dir)
    return NULL;

  size_t len = strlen(name);
  // Allocate space for path + "/*" + null
  char* search_pattern = (char*) malloc(len + 5);
  if (!search_pattern) {
    free(dir);
    return NULL;
  }

  strcpy(search_pattern, name);

  // Normalize logic: ensure it ends with "/*" or "\*"
  // If it ends with / or \, just add *
  // Else add /*
  if (len > 0) {
    char last = search_pattern[len - 1];
    if (last == '/' || last == '\\') {
      strcat(search_pattern, "*");
    } else {
      strcat(search_pattern, "/*");
    }
  } else {
    strcat(search_pattern, "*");
  }

  wchar_t wpattern[NAME_MAX + 1]; // NAME_MAX is usually 260
  str_to_wstr(search_pattern, wpattern, NAME_MAX + 1);
  free(search_pattern);

  dir->hFind = FindFirstFileW(wpattern, &dir->data);

  if (dir->hFind == INVALID_HANDLE_VALUE) {
    free(dir);
    return NULL;
  }

  dir->first_read = 1;
  return dir;
}

struct dirent* readdir(DIR* dir) {
  if (!dir || dir->hFind == INVALID_HANDLE_VALUE)
    return NULL;

  if (dir->first_read) {
    dir->first_read = 0;
  } else {
    if (!FindNextFileW(dir->hFind, &dir->data)) {
      return NULL;
    }
  }

  // Convert filename to UTF-8
  wstr_to_str(dir->data.cFileName, dir->cur_ent.d_name, NAME_MAX + 1);

  return &dir->cur_ent;
}

int closedir(DIR* dir) {
  if (!dir)
    return -1;

  if (dir->hFind != INVALID_HANDLE_VALUE) {
    FindClose(dir->hFind);
  }

  free(dir);
  return 0;
}

#endif
