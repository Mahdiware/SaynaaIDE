/*
 * Copyright (c) 2022-2026 Mohamed Abdifatah. All rights reserved.
 * Distributed Under The MIT License
 */

#include "saynaa_json.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* parse_value(saynaa_json* item, const char* val);

static char* saynaa_strdup(const char* str) {
  if (!str)
    return NULL;
  size_t len = strlen(str) + 1;
  char* copy = (char*) malloc(len);
  if (copy)
    memcpy(copy, str, len);
  return copy;
}

static saynaa_json* saynaa_json_new(void) {
  saynaa_json* node = (saynaa_json*) calloc(1, sizeof(saynaa_json));
  return node;
}

void saynaa_json_delete(saynaa_json* item) {
  saynaa_json* next;
  while (item) {
    next = item->next;
    if (item->child)
      saynaa_json_delete(item->child);
    if (item->valuestring)
      free(item->valuestring);
    if (item->key)
      free(item->key);
    free(item);
    item = next;
  }
}

// Skip whitespace
static const char* skip(const char* in) {
  while (in && *in && (unsigned char) *in <= 32)
    in++;
  return in;
}

// Parse Number
static const char* parse_number(saynaa_json* item, const char* num) {
  char* end;
  double n = strtod(num, &end);
  if (end == num)
    return NULL; // Parsing failure

  item->type = SAYNAA_JSON_NUMBER;
  item->valuedouble = n;
  return end;
}

// Parse String
static unsigned parse_hex4(const char* str) {
  unsigned h = 0;
  for (int i = 0; i < 4; i++) {
    char c = *str++;
    h <<= 4;
    if (c >= '0' && c <= '9')
      h += c - '0';
    else if (c >= 'a' && c <= 'f')
      h += c - 'a' + 10;
    else if (c >= 'A' && c <= 'F')
      h += c - 'A' + 10;
    else
      return 0;
  }
  return h;
}

static const char* parse_string(saynaa_json* item, const char* str) {
  const char* ptr = str + 1;
  const char* end_ptr = ptr;
  char* out;
  int len = 0;

  // Calculate length needed
  while (*end_ptr != '"' && *end_ptr) {
    if (*end_ptr == '\\') {
      end_ptr++;
      if (!*end_ptr)
        return NULL; // Unexpected end
    }
    end_ptr++;
    len++;
  }
  if (!*end_ptr)
    return NULL; // string not terminated

  out = (char*) malloc(len + 1);
  if (!out)
    return NULL;
  item->valuestring = out;
  item->type = SAYNAA_JSON_STRING;

  while (ptr < end_ptr) {
    if (*ptr != '\\') {
      *out++ = *ptr++;
    } else {
      ptr++;
      switch (*ptr) {
        case 'b':
          *out++ = '\b';
          break;
        case 'f':
          *out++ = '\f';
          break;
        case 'n':
          *out++ = '\n';
          break;
        case 'r':
          *out++ = '\r';
          break;
        case 't':
          *out++ = '\t';
          break;
        case 'u':
          {
            // Simple unicode handling (skip or best effort)
            // For now, let's just parse logic but not full UTF8 encoding logic manually here
            // to keep it short unless required.
            // Actually cJSON does full UTF8. Let's do basic hex decode if valid.
            unsigned uc = parse_hex4(ptr + 1);
            ptr += 4; // advanced 1 by switch, need +4 more
            if (uc < 0x80)
              *out++ = (char) uc;
            // TODO: Proper multi-byte utf8 encoding if needed.
            // For MVP, just dumping ASCII part if < 128
            else
              *out++ = '?';
            break;
          }
        default:
          *out++ = *ptr;
          break;
      }
      ptr++;
    }
  }
  *out = 0;
  return end_ptr + 1;
}

// Parse Array
static const char* parse_array(saynaa_json* item, const char* value) {
  saynaa_json* child;
  value = skip(value + 1);
  if (*value == ']') {
    item->type = SAYNAA_JSON_ARRAY;
    return value + 1; // Empty array
  }

  item->type = SAYNAA_JSON_ARRAY;
  item->child = child = saynaa_json_new();
  value = parse_value(child, value);
  if (!value)
    return NULL;

  while (*(value = skip(value)) == ',') {
    saynaa_json* new_item = saynaa_json_new();
    child->next = new_item;
    child = new_item;
    value = parse_value(child, skip(value + 1));
    if (!value)
      return NULL;
  }

  if (*value == ']')
    return value + 1;
  return NULL; // Malformed
}

// Parse Object
static const char* parse_object(saynaa_json* item, const char* value) {
  saynaa_json* child;
  value = skip(value + 1);
  if (*value == '}') {
    item->type = SAYNAA_JSON_OBJECT;
    return value + 1;
  }

  item->type = SAYNAA_JSON_OBJECT;
  item->child = child = saynaa_json_new();

  value = skip(value);
  if (*value != '"')
    return NULL; // Key must be string

  // Parse key (hackily using parse_string into temporary item)
  saynaa_json key_item = {0};
  value = parse_string(&key_item, value);
  if (!value)
    return NULL;
  child->key = key_item.valuestring;

  value = skip(value);
  if (*value != ':')
    return NULL;

  value = parse_value(child, skip(value + 1));
  if (!value)
    return NULL;

  while (*(value = skip(value)) == ',') {
    saynaa_json* new_item = saynaa_json_new();
    child->next = new_item;
    child = new_item;

    value = skip(value + 1);
    if (*value != '"')
      return NULL;

    saynaa_json k = {0};
    value = parse_string(&k, value);
    if (!value)
      return NULL;
    child->key = k.valuestring;

    value = skip(value);
    if (*value != ':') {
      free(k.valuestring);
      return NULL;
    }

    value = parse_value(child, skip(value + 1));
    if (!value)
      return NULL;
  }

  if (*value == '}')
    return value + 1;
  return NULL;
}

static const char* parse_value(saynaa_json* item, const char* val) {
  if (!val)
    return NULL;
  val = skip(val);

  if (!strncmp(val, "null", 4)) {
    item->type = SAYNAA_JSON_NULL;
    return val + 4;
  }
  if (!strncmp(val, "false", 5)) {
    item->type = SAYNAA_JSON_FALSE;
    return val + 5;
  }
  if (!strncmp(val, "true", 4)) {
    item->type = SAYNAA_JSON_TRUE;
    return val + 4;
  }
  if (*val == '"')
    return parse_string(item, val);
  if (*val == '-' || (*val >= '0' && *val <= '9'))
    return parse_number(item, val);
  if (*val == '[')
    return parse_array(item, val);
  if (*val == '{')
    return parse_object(item, val);

  return NULL;
}

saynaa_json* saynaa_json_parse(const char* value) {
  saynaa_json* root = saynaa_json_new();
  if (!parse_value(root, value)) {
    saynaa_json_delete(root);
    return NULL;
  }
  return root;
}

// Constructors
saynaa_json* saynaa_json_create_null(void) {
  saynaa_json* item = saynaa_json_new();
  item->type = SAYNAA_JSON_NULL;
  return item;
}
saynaa_json* saynaa_json_create_bool(int b) {
  saynaa_json* item = saynaa_json_new();
  item->type = b ? SAYNAA_JSON_TRUE : SAYNAA_JSON_FALSE;
  return item;
}
saynaa_json* saynaa_json_create_number(double num) {
  saynaa_json* item = saynaa_json_new();
  item->type = SAYNAA_JSON_NUMBER;
  item->valuedouble = num;
  return item;
}
saynaa_json* saynaa_json_create_string(const char* string) {
  saynaa_json* item = saynaa_json_new();
  item->type = SAYNAA_JSON_STRING;
  item->valuestring = saynaa_strdup(string ? string : "");
  return item;
}
saynaa_json* saynaa_json_create_array(void) {
  saynaa_json* item = saynaa_json_new();
  item->type = SAYNAA_JSON_ARRAY;
  return item;
}
saynaa_json* saynaa_json_create_object(void) {
  saynaa_json* item = saynaa_json_new();
  item->type = SAYNAA_JSON_OBJECT;
  return item;
}

void saynaa_json_add_item_to_array(saynaa_json* array, saynaa_json* item) {
  if (!array || !item)
    return;
  if (array->type != SAYNAA_JSON_ARRAY)
    return;

  if (!array->child) {
    array->child = item;
  } else {
    saynaa_json* c = array->child;
    while (c->next)
      c = c->next;
    c->next = item;
  }
}

void saynaa_json_add_item_to_object(saynaa_json* object, const char* key, saynaa_json* item) {
  if (!object || !item)
    return;
  if (object->type != SAYNAA_JSON_OBJECT)
    return;
  if (item->key)
    free(item->key);
  item->key = saynaa_strdup(key);

  if (!object->child) {
    object->child = item;
  } else {
    saynaa_json* c = object->child;
    while (c->next)
      c = c->next;
    c->next = item;
  }
}

// Printing (Simple buffer implementation)
typedef struct {
  char* buffer;
  size_t length;
  size_t offset;
} printbuffer;

static void ensure(printbuffer* p, size_t needed) {
  if (!p->buffer) {
    p->length = needed + 256;
    p->buffer = (char*) malloc(p->length);
    p->offset = 0;
  } else if (p->offset + needed >= p->length) {
    p->length = p->length * 2 + needed;
    p->buffer = (char*) realloc(p->buffer, p->length);
  }
}

static void print_string_ptr(const char* str, printbuffer* p) {
  ensure(p, strlen(str) * 2 + 3); // Conservative size for escapes
  p->buffer[p->offset++] = '"';
  // Escape chars
  const char* ptr = str;
  while (*ptr) {
    if (*ptr == '"' || *ptr == '\\') {
      ensure(p, 2);
      p->buffer[p->offset++] = '\\';
      p->buffer[p->offset++] = *ptr;
    } else if (*ptr == '\n') {
      ensure(p, 2);
      p->buffer[p->offset++] = '\\';
      p->buffer[p->offset++] = 'n';
    } else {
      ensure(p, 1);
      p->buffer[p->offset++] = *ptr;
    }
    ptr++;
  }
  ensure(p, 2);
  p->buffer[p->offset++] = '"';
  p->buffer[p->offset] = '\0';
}

static void print_value_internal(const saynaa_json* item, printbuffer* p, int depth, int fmt) {
  if (!item)
    return;
  switch (item->type) {
    case SAYNAA_JSON_NULL:
      ensure(p, 5);
      strcpy(p->buffer + p->offset, "null");
      p->offset += 4;
      break;
    case SAYNAA_JSON_FALSE:
      ensure(p, 6);
      strcpy(p->buffer + p->offset, "false");
      p->offset += 5;
      break;
    case SAYNAA_JSON_TRUE:
      ensure(p, 5);
      strcpy(p->buffer + p->offset, "true");
      p->offset += 4;
      break;
    case SAYNAA_JSON_NUMBER:
      {
        char buf[64];
        sprintf(buf, "%g", item->valuedouble); // Simple number print
        ensure(p, strlen(buf) + 1);
        strcpy(p->buffer + p->offset, buf);
        p->offset += strlen(buf);
        break;
      }
    case SAYNAA_JSON_STRING:
      print_string_ptr(item->valuestring, p);
      break;
    case SAYNAA_JSON_ARRAY:
      {
        ensure(p, 2);
        p->buffer[p->offset++] = '[';
        if (fmt)
          p->buffer[p->offset++] = '\n';
        saynaa_json* child = item->child;
        while (child) {
          if (fmt)
            for (int i = 0; i < depth + 1; i++) {
              ensure(p, 1);
              p->buffer[p->offset++] = '\t';
            }
          print_value_internal(child, p, depth + 1, fmt);
          if (child->next) {
            ensure(p, 2);
            p->buffer[p->offset++] = ',';
            if (fmt)
              p->buffer[p->offset++] = '\n';
          }
          child = child->next;
        }
        if (fmt) {
          ensure(p, 2 + depth);
          p->buffer[p->offset++] = '\n';
          for (int i = 0; i < depth; i++)
            p->buffer[p->offset++] = '\t';
        }
        ensure(p, 2);
        p->buffer[p->offset++] = ']';
        p->buffer[p->offset] = '\0';
        break;
      }
    case SAYNAA_JSON_OBJECT:
      {
        ensure(p, 2);
        p->buffer[p->offset++] = '{';
        if (fmt)
          p->buffer[p->offset++] = '\n';
        saynaa_json* child = item->child;
        while (child) {
          if (fmt)
            for (int i = 0; i < depth + 1; i++) {
              ensure(p, 1);
              p->buffer[p->offset++] = '\t';
            }
          print_string_ptr(child->key, p);
          ensure(p, 2);
          p->buffer[p->offset++] = ':';
          if (fmt)
            p->buffer[p->offset++] = ' ';
          print_value_internal(child, p, depth + 1, fmt);
          if (child->next) {
            ensure(p, 2);
            p->buffer[p->offset++] = ',';
            if (fmt)
              p->buffer[p->offset++] = '\n';
          }
          child = child->next;
        }
        if (fmt) {
          ensure(p, 2 + depth);
          p->buffer[p->offset++] = '\n';
          for (int i = 0; i < depth; i++)
            p->buffer[p->offset++] = '\t';
        }
        ensure(p, 2);
        p->buffer[p->offset++] = '}';
        p->buffer[p->offset] = '\0';
        break;
      }
  }
}

char* saynaa_json_print(const saynaa_json* item) {
  printbuffer p = {0};
  print_value_internal(item, &p, 0, 1);
  return p.buffer;
}

char* saynaa_json_print_unformatted(const saynaa_json* item) {
  printbuffer p = {0};
  print_value_internal(item, &p, 0, 0);
  return p.buffer;
}
