/*
 * Copyright (c) 2022-2026 Mohamed Abdifatah. All rights reserved.
 * Distributed Under The MIT License
 */

#include "saynaa_path.h"

#include <ctype.h>
#include <string.h>

#ifdef _WIN32
#define SEPARATOR '\\'
#define ALT_SEPARATOR '/'
#else
#define SEPARATOR '/'
#define ALT_SEPARATOR '\\'
#endif

static bool is_sep(char c) {
#ifdef _WIN32
  return c == '\\' || c == '/';
#else
  return c == '/';
#endif
}

char saynaa_path_separator(void) {
  return SEPARATOR;
}

bool saynaa_path_is_absolute(const char* path) {
  if (!path || !*path)
    return false;
#ifdef _WIN32
  if (isalpha((unsigned char) path[0]) && path[1] == ':' && is_sep(path[2]))
    return true;
  if (is_sep(path[0]) && is_sep(path[1]))
    return true;
  return false;
#else
  return is_sep(path[0]);
#endif
}

// Helper to append string to buffer with bounds check
// Uses memmove for safety with overlapping buffers
static size_t append(char* dest, size_t dest_sz, size_t pos, const char* src, size_t src_len) {
  if (src_len == 0)
    return pos;
  size_t available = (dest_sz > pos) ? (dest_sz - pos) : 0;
  // Reserve space for final null terminator?
  // We assume caller handles final null. We write what fits.
  // But we shouldn't fill the last byte if we want to null terminate later.
  // Let's protect 1 byte.
  if (available > 1) {
    size_t to_copy = (src_len < available - 1) ? src_len : (available - 1);
    memmove(dest + pos, src, to_copy);
  }
  return pos + src_len;
}

static size_t append_char(char* dest, size_t dest_sz, size_t pos, char c) {
  if (dest_sz > pos + 1) {
    dest[pos] = c;
  }
  return pos + 1;
}

static void terminate(char* dest, size_t dest_sz, size_t pos) {
  if (dest_sz > 0) {
    if (pos < dest_sz)
      dest[pos] = '\0';
    else
      dest[dest_sz - 1] = '\0';
  }
}

size_t saynaa_path_normalize(const char* path, char* buffer, size_t buffer_size) {
  if (!path || !*path) {
    size_t new_len = append(buffer, buffer_size, 0, ".", 1);
    terminate(buffer, buffer_size, new_len);
    return new_len;
  }

  size_t read_pos = 0;
  size_t write_pos = 0;

  bool is_abs = saynaa_path_is_absolute(path);
  if (is_abs) {
#ifdef _WIN32
    if (is_sep(path[0]) && is_sep(path[1])) {
      // UNC
      write_pos = append(buffer, buffer_size, write_pos, "\\\\", 2);
      read_pos = 2;
    } else if (isalpha((unsigned char) path[0]) && path[1] == ':') {
      // Drive
      write_pos = append_char(buffer, buffer_size, write_pos,
                              (char) toupper((unsigned char) path[0]));
      write_pos = append(buffer, buffer_size, write_pos, ":\\", 2);
      read_pos = 3;
    } else {
      write_pos = append_char(buffer, buffer_size, write_pos, '\\');
      read_pos = 1;
    }
#else
    write_pos = append_char(buffer, buffer_size, write_pos, '/');
    read_pos = 1;
#endif
  }

  const char* p = path + read_pos;

  while (*p) {
    while (is_sep(*p))
      p++;
    if (!*p)
      break;

    const char* start = p;
    while (*p && !is_sep(*p))
      p++;
    size_t len = p - start;

    if (len == 1 && start[0] == '.') {
      // Ignore
    } else if (len == 2 && start[0] == '.' && start[1] == '.') {
      bool can_pop = false;
      size_t last_sep = 0;

      if (write_pos > 0) {
        size_t scan = write_pos;
        bool found_sep_back = false;
        while (scan > 0) {
          scan--;
          if (buffer[scan] == SEPARATOR) { // assuming buffer uses canonical separator
            found_sep_back = true;
            last_sep = scan;
            break;
          }
        }

        if (found_sep_back) {
          const char* seg = buffer + last_sep + 1;
          // Must limit comparison to what we wrote past last_sep
          size_t seg_len = write_pos - (last_sep + 1);
          if (seg_len == 2 && seg[0] == '.' && seg[1] == '.') {
            can_pop = false;
          } else {
#ifdef _WIN32
            if (last_sep == 2 && buffer[1] == ':')
              can_pop = false;
            else
              can_pop = true;
#else
            // if last_sep is 0 (root), can pop.
            can_pop = true;
#endif
          }
        } else {
          // scan reached 0, no separator.
          if (write_pos == 2 && buffer[0] == '.' && buffer[1] == '.') {
            can_pop = false;
          } else {
            if (is_abs) {
              // Should be impossible for unix abs to not have sep?
              // /...
            }
            if (write_pos > 0)
              can_pop = true;
          }
        }
      } else {
        // write_pos == 0.
      }

      if (can_pop) {
        size_t root_len = 0;
#ifdef _WIN32
        if (is_abs)
          root_len = (buffer[1] == ':') ? 3 : 2;
#else
        if (is_abs)
          root_len = 1;
#endif

        // Scan back for separator before write_pos
        size_t k = write_pos;
        while (k > root_len && buffer[k - 1] != SEPARATOR) {
          k--;
        }

        // Safety
        if (k < write_pos && buffer[k] == '.' && buffer[k + 1] == '.' && (k + 2 == write_pos)) {
          goto append_dotdot;
        }

        if (k == root_len) {
          if (is_abs) {
            // ignore
          } else {
            if (write_pos == 0)
              goto append_dotdot;
            write_pos = 0;
          }
        } else {
          write_pos = k - 1;
          if (write_pos < root_len && is_abs)
            write_pos = root_len;
        }
      } else {
        goto append_dotdot;
      }

      continue;

    append_dotdot:
      if (write_pos > 0 && buffer[write_pos - 1] != SEPARATOR) {
        write_pos = append_char(buffer, buffer_size, write_pos, SEPARATOR);
      }
      write_pos = append(buffer, buffer_size, write_pos, "..", 2);
    } else {
      if (write_pos > 0) {
        if (buffer[write_pos - 1] != SEPARATOR) {
          write_pos = append_char(buffer, buffer_size, write_pos, SEPARATOR);
        }
      }
      write_pos = append(buffer, buffer_size, write_pos, start, len);
    }
  }

  if (write_pos == 0) {
    write_pos = append(buffer, buffer_size, write_pos, ".", 1);
  } else {
    size_t root_len = 0;
#ifdef _WIN32
    if (is_abs)
      root_len = (buffer[1] == ':') ? 3 : 2;
#else
    if (is_abs)
      root_len = 1;
#endif
    if (write_pos > root_len && buffer[write_pos - 1] == SEPARATOR) {
      write_pos--;
    }
  }

  terminate(buffer, buffer_size, write_pos);
  return write_pos;
}

size_t saynaa_path_join(const char* path_a, const char* path_b, char* buffer, size_t buffer_size) {
  if (buffer != path_a) {
    size_t len_a = strlen(path_a);
    if (len_a >= buffer_size)
      len_a = buffer_size - 1;
    memmove(buffer, path_a, len_a);
    // We do NOT modify buffer[len_a] significantly yet, we track pos.

    size_t pos = len_a;
    char last = (pos > 0) ? buffer[pos - 1] : 0;

    if (!is_sep(last) && !is_sep(*path_b)) {
      pos = append_char(buffer, buffer_size, pos, SEPARATOR);
    } else if (is_sep(last) && is_sep(*path_b)) {
      // Skip logic? Normalize cleans up.
    }

    size_t len_b = strlen(path_b);
    pos = append(buffer, buffer_size, pos, path_b, len_b);

    terminate(buffer, buffer_size, pos);
  } else {
    // In-place append
    size_t len_a = strlen(buffer);
    size_t pos = len_a;
    char last = (pos > 0) ? buffer[pos - 1] : 0;

    if (!is_sep(last) && !is_sep(*path_b)) {
      pos = append_char(buffer, buffer_size, pos, SEPARATOR);
    }

    size_t len_b = strlen(path_b);
    pos = append(buffer, buffer_size, pos, path_b, len_b);
    terminate(buffer, buffer_size, pos);
  }

  return saynaa_path_normalize(buffer, buffer, buffer_size);
}

size_t saynaa_path_get_absolute(const char* base, const char* path,
                                char* buffer, size_t buffer_size) {
  if (saynaa_path_is_absolute(path)) {
    return saynaa_path_normalize(path, buffer, buffer_size);
  }
  return saynaa_path_join(base, path, buffer, buffer_size);
}

void saynaa_path_dirname(const char* path, size_t* length) {
  if (!path) {
    *length = 0;
    return;
  }
  size_t len = strlen(path);
  while (len > 0 && is_sep(path[len - 1]))
    len--;
  while (len > 0 && !is_sep(path[len - 1]))
    len--;
  if (len > 0)
    *length = len;
  else
    *length = 0;
}

void saynaa_path_basename(const char* path, const char** basename, size_t* length) {
  if (!path) {
    *basename = NULL;
    *length = 0;
    return;
  }
  size_t len = strlen(path);
  size_t end = len;
  while (end > 0 && is_sep(path[end - 1]))
    end--;

  if (end == 0 && len > 0) {
    *basename = path + len;
    *length = 0;
    return;
  }

  size_t start = end;
  while (start > 0 && !is_sep(path[start - 1]))
    start--;

  *basename = path + start;
  *length = end - start;
}

bool saynaa_path_extension(const char* path, const char** extension, size_t* length) {
  const char* base;
  size_t base_len;
  saynaa_path_basename(path, &base, &base_len);

  if (base_len == 0)
    return false;

  const char* p = base + base_len;
  while (p > base) {
    p--;
    if (*p == '.') {
      if (p == base)
        return false;
      if (extension)
        *extension = p;
      if (length)
        *length = (base + base_len) - p;
      return true;
    }
  }
  return false;
}

size_t saynaa_path_get_relative(const char* base_directory, const char* path,
                                char* buffer, size_t buffer_size) {
  const char* b = base_directory;
  const char* p = path;
  const char* last_sep_b = b;
  const char* last_sep_p = p;

  while (*b && *p
         && (tolower((unsigned char) *b) == tolower((unsigned char) *p)
             || (is_sep(*b) && is_sep(*p)))) {
    if (is_sep(*b)) {
      last_sep_b = b;
      last_sep_p = p;
    }
    b++;
    p++;
  }

  if (!*b && !*p) {
    if (buffer_size > 1) {
      buffer[0] = '.';
      buffer[1] = '\0';
    }
    return 1;
  }

  if (*b && !is_sep(*b))
    b = last_sep_b + (is_sep(*last_sep_b) ? 1 : 0);
  if (*p && !is_sep(*p))
    p = last_sep_p + (is_sep(*last_sep_p) ? 1 : 0);

  size_t write_pos = 0;
  while (*b) {
    if (!is_sep(*b)) {
      write_pos = append(buffer, buffer_size, write_pos, "..", 2);
      write_pos = append_char(buffer, buffer_size, write_pos, SEPARATOR);
      while (*b && !is_sep(*b))
        b++;
    } else {
      b++;
    }
  }

  if (*p) {
    while (is_sep(*p))
      p++;
    write_pos = append(buffer, buffer_size, write_pos, p, strlen(p));
  } else {
    if (write_pos > 0 && buffer[write_pos - 1] == SEPARATOR)
      write_pos--;
    if (write_pos == 0)
      write_pos = append(buffer, buffer_size, write_pos, ".", 1);
  }
  terminate(buffer, buffer_size, write_pos);
  return write_pos;
}
