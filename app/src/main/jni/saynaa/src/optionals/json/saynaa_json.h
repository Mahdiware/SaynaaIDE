/*
 * Copyright (c) 2022-2026 Mohamed Abdifatah. All rights reserved.
 * Distributed Under The MIT License
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  SAYNAA_JSON_NULL,
  SAYNAA_JSON_FALSE,
  SAYNAA_JSON_TRUE,
  SAYNAA_JSON_NUMBER,
  SAYNAA_JSON_STRING,
  SAYNAA_JSON_ARRAY,
  SAYNAA_JSON_OBJECT
} saynaa_json_type;

typedef struct saynaa_json {
  struct saynaa_json* next;
  struct saynaa_json* child; // First child of object/array

  int type;
  char* key;          // For object properties (key name)
  char* valuestring;  // For strings
  double valuedouble; // For numbers
} saynaa_json;

// Parse a string into a JSON object
saynaa_json* saynaa_json_parse(const char* value);

// Print a JSON object to string
char* saynaa_json_print(const saynaa_json* item);

// Print a JSON object to string (compact)
char* saynaa_json_print_unformatted(const saynaa_json* item);

// Delete the object and all children
void saynaa_json_delete(saynaa_json* item);

// Constructors
saynaa_json* saynaa_json_create_null(void);
saynaa_json* saynaa_json_create_bool(int b);
saynaa_json* saynaa_json_create_number(double num);
saynaa_json* saynaa_json_create_string(const char* string);
saynaa_json* saynaa_json_create_array(void);
saynaa_json* saynaa_json_create_object(void);

// Modifiers
void saynaa_json_add_item_to_array(saynaa_json* array, saynaa_json* item);
void saynaa_json_add_item_to_object(saynaa_json* object, const char* key, saynaa_json* item);

#ifdef __cplusplus
}
#endif
