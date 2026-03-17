/*
 * Copyright (c) 2022-2026 Mohamed Abdifatah. All rights reserved.
 * Distributed Under The MIT License
 */

#include "../shared/saynaa_value.h"
#include "saynaa_optionals.h"

#if defined(__has_include)
#if __has_include(<pcre2.h>)
#define SAYNAA_HAS_PCRE2 1
#endif
#endif

#ifndef SAYNAA_HAS_PCRE2
#define SAYNAA_HAS_PCRE2 0
#endif

#if SAYNAA_HAS_PCRE2
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

// Forward decl
void setRegexError(VM* vm, int errornumber);

// Helper to convert PCRE2 errors to VM errors
void setRegexError(VM* vm, int errornumber) {
  PCRE2_UCHAR buffer[256];
  pcre2_get_error_message(errornumber, buffer, sizeof(buffer));
  SetRuntimeErrorFmt(vm, "Regex Error: %s", (char*) buffer);
}

// Helper to compile regex
static pcre2_code* compileRegex(VM* vm, const char* pattern) {
  int errornumber;
  PCRE2_SIZE erroroffset;
  pcre2_code* re = pcre2_compile((PCRE2_SPTR) pattern, PCRE2_ZERO_TERMINATED, 0,
                                 &errornumber, &erroroffset, NULL);
  if (!re) {
    setRegexError(vm, errornumber);
    return NULL;
  }
  return re;
}

saynaa_function(
    _reMatch, "re.match(pattern: String, text: String) -> String|Null",
    "Match a regular expression pattern to the beginning of a string.") {
  const char* pattern;
  const char* text;
  if (!ValidateSlotString(vm, 1, &pattern, NULL))
    return;
  if (!ValidateSlotString(vm, 2, &text, NULL))
    return;

  pcre2_code* re = compileRegex(vm, pattern);
  if (!re)
    return;

  pcre2_match_data* match_data = pcre2_match_data_create_from_pattern(re, NULL);
  int rc = pcre2_match(re, (PCRE2_SPTR) text, strlen(text), 0, PCRE2_ANCHORED,
                       match_data, NULL);

  if (rc >= 0) {
    PCRE2_SIZE* ovector = pcre2_get_ovector_pointer(match_data);
    setSlotStringLength(vm, 0, text + ovector[0], ovector[1] - ovector[0]);
  } else {
    setSlotNull(vm, 0);
  }

  pcre2_match_data_free(match_data);
  pcre2_code_free(re);
}

saynaa_function(_reFullMatch, "re.fullmatch(pattern: String, text: String) -> String|Null",
                "Match a regular expression pattern to all of a string.") {
  const char* pattern;
  const char* text;
  uint32_t len32 = 0;
  if (!ValidateSlotString(vm, 1, &pattern, NULL))
    return;
  if (!ValidateSlotString(vm, 2, &text, &len32))
    return;
  size_t len = (size_t) len32;

  pcre2_code* re = compileRegex(vm, pattern);
  if (!re)
    return;

  pcre2_match_data* match_data = pcre2_match_data_create_from_pattern(re, NULL);
  int rc = pcre2_match(re, (PCRE2_SPTR) text, len, 0, PCRE2_ANCHORED, match_data, NULL);

  if (rc >= 0) {
    PCRE2_SIZE* ovector = pcre2_get_ovector_pointer(match_data);
    if (ovector[0] == 0 && ovector[1] == len) {
      setSlotStringLength(vm, 0, text, len);
    } else {
      setSlotNull(vm, 0);
    }
  } else {
    setSlotNull(vm, 0);
  }

  pcre2_match_data_free(match_data);
  pcre2_code_free(re);
}

saynaa_function(_reSearch, "re.search(pattern: String, text: String) -> String|Null",
                "Returns the first matching substring.") {
  const char* pattern;
  const char* text;
  if (!ValidateSlotString(vm, 1, &pattern, NULL))
    return;
  if (!ValidateSlotString(vm, 2, &text, NULL))
    return;

  pcre2_code* re = compileRegex(vm, pattern);
  if (!re)
    return;

  pcre2_match_data* match_data = pcre2_match_data_create_from_pattern(re, NULL);
  int rc = pcre2_match(re, (PCRE2_SPTR) text, strlen(text), 0, 0, match_data, NULL);

  if (rc >= 0) {
    PCRE2_SIZE* ovector = pcre2_get_ovector_pointer(match_data);
    setSlotStringLength(vm, 0, text + ovector[0], ovector[1] - ovector[0]);
  } else {
    setSlotNull(vm, 0);
  }

  pcre2_match_data_free(match_data);
  pcre2_code_free(re);
}

saynaa_function(_reSplit, "re.split(pattern: String, text: String, maxsplit: Int) -> List",
                "Split a string by the occurrences of a pattern.") {
  const char* pattern;
  const char* text;
  uint32_t len32 = 0;
  int maxsplit = 0;

  if (!ValidateSlotString(vm, 1, &pattern, NULL))
    return;
  if (!ValidateSlotString(vm, 2, &text, &len32))
    return;
  size_t len = (size_t) len32;

  int argc = GetArgc(vm);
  if (argc > 3)
    maxsplit = (int) GetSlotNumber(vm, 3);
  else if (argc == 3 && GetSlotType(vm, 3) == vNUMBER)
    maxsplit = (int) GetSlotNumber(vm, 3);

  pcre2_code* re = compileRegex(vm, pattern);
  if (!re)
    return;

  NewList(vm, 0);

  pcre2_match_data* match_data = pcre2_match_data_create_from_pattern(re, NULL);
  PCRE2_SIZE start_offset = 0;
  PCRE2_SIZE last_end = 0;

  int splits = 0;

  while (start_offset <= len && (maxsplit <= 0 || splits < maxsplit)) {
    int rc = pcre2_match(re, (PCRE2_SPTR) text, len, start_offset, 0, match_data, NULL);
    if (rc < 0)
      break;

    PCRE2_SIZE* ovector = pcre2_get_ovector_pointer(match_data);
    PCRE2_SIZE match_start = ovector[0];
    PCRE2_SIZE match_end = ovector[1];

    if (match_start == match_end && match_start == start_offset) {
      if (start_offset < len) {
        start_offset++;
        continue;
      } else {
        break;
      }
    }

    setSlotStringLength(vm, 1, text + last_end, match_start - last_end);
    ListInsert(vm, 0, -1, 1);
    splits++;

    if (rc > 1) {
      for (int i = 1; i < rc; i++) {
        setSlotStringLength(vm, 1, text + ovector[2 * i],
                            ovector[2 * i + 1] - ovector[2 * i]);
        ListInsert(vm, 0, -1, 1);
      }
    }

    last_end = match_end;
    start_offset = match_end;
  }

  setSlotStringLength(vm, 1, text + last_end, len - last_end);
  ListInsert(vm, 0, -1, 1);

  pcre2_match_data_free(match_data);
  pcre2_code_free(re);
}

saynaa_function(_reSub, "re.sub(pattern: String, repl: String, text: String) -> String",
                "Substitute occurrences of a pattern found in a string.") {
  const char* pattern;
  const char* repl;
  const char* text;
  uint32_t text_len32 = 0;

  if (!ValidateSlotString(vm, 1, &pattern, NULL))
    return;
  if (!ValidateSlotString(vm, 2, &repl, NULL))
    return;
  if (!ValidateSlotString(vm, 3, &text, &text_len32))
    return;

  size_t text_len = (size_t) text_len32;

  pcre2_code* re = compileRegex(vm, pattern);
  if (!re)
    return;

  PCRE2_SIZE outlen = text_len * 2 + 100;
  PCRE2_UCHAR* output = malloc(outlen);

  int options = PCRE2_SUBSTITUTE_GLOBAL | PCRE2_SUBSTITUTE_OVERFLOW_LENGTH
                | PCRE2_SUBSTITUTE_EXTENDED;
  pcre2_match_data* match_data = pcre2_match_data_create_from_pattern(re, NULL);

  int rc = pcre2_substitute(re, (PCRE2_SPTR) text, text_len, 0, options, match_data,
                            NULL, (PCRE2_SPTR) repl, strlen(repl), output, &outlen);

  if (rc == PCRE2_ERROR_NOMEMORY) {
    free(output);
    output = malloc(outlen);
    rc = pcre2_substitute(re, (PCRE2_SPTR) text, text_len, 0, options, match_data,
                          NULL, (PCRE2_SPTR) repl, strlen(repl), output, &outlen);
  }

  if (rc >= 0) {
    setSlotStringLength(vm, 0, (char*) output, outlen);
  } else {
    setRegexError(vm, rc);
  }

  free(output);
  pcre2_match_data_free(match_data);
  pcre2_code_free(re);
}

saynaa_function(_reSubn, "re.subn(pattern: String, repl: String, text: String) -> List",
                "Same as sub, but returns [new_string, count].") {
  const char* pattern;
  const char* repl;
  const char* text;
  uint32_t text_len32 = 0;

  if (!ValidateSlotString(vm, 1, &pattern, NULL))
    return;
  if (!ValidateSlotString(vm, 2, &repl, NULL))
    return;
  if (!ValidateSlotString(vm, 3, &text, &text_len32))
    return;

  size_t text_len = (size_t) text_len32;

  pcre2_code* re = compileRegex(vm, pattern);
  if (!re)
    return;

  PCRE2_SIZE outlen = text_len * 2 + 100;
  PCRE2_UCHAR* output = malloc(outlen);

  int options = PCRE2_SUBSTITUTE_GLOBAL | PCRE2_SUBSTITUTE_OVERFLOW_LENGTH
                | PCRE2_SUBSTITUTE_EXTENDED;
  pcre2_match_data* match_data = pcre2_match_data_create_from_pattern(re, NULL);

  int rc = pcre2_substitute(re, (PCRE2_SPTR) text, text_len, 0, options, match_data,
                            NULL, (PCRE2_SPTR) repl, strlen(repl), output, &outlen);

  if (rc == PCRE2_ERROR_NOMEMORY) {
    free(output);
    output = malloc(outlen);
    rc = pcre2_substitute(re, (PCRE2_SPTR) text, text_len, 0, options, match_data,
                          NULL, (PCRE2_SPTR) repl, strlen(repl), output, &outlen);
  }

  if (rc >= 0) {
    NewList(vm, 0);
    setSlotStringLength(vm, 1, (char*) output, outlen);
    ListInsert(vm, 0, -1, 1);

    // Use slot 1 for int count
    setSlotNumber(vm, 1, (double) rc);
    ListInsert(vm, 0, -1, 1);
  } else {
    setRegexError(vm, rc);
  }

  free(output);
  pcre2_match_data_free(match_data);
  pcre2_code_free(re);
}

saynaa_function(_reEscape, "re.escape(pattern: String) -> String",
                "Backslash all non-alphanumerics in a string.") {
  const char* pattern;
  if (!ValidateSlotString(vm, 1, &pattern, NULL))
    return;

  size_t len = strlen(pattern);
  char* result = malloc(len * 2 + 1);
  size_t j = 0;
  for (size_t i = 0; i < len; i++) {
    char c = pattern[i];
    // Check if alphanumeric or underscore (Python allows _)
    bool is_safe = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                   || (c >= '0' && c <= '9') || (c == '_');
    if (!is_safe) {
      result[j++] = '\\';
    }
    result[j++] = c;
  }
  result[j] = '\0';
  setSlotString(vm, 0, result);
  free(result);
}

saynaa_function(_rePurge, "re.purge() -> Null", "Clear the regular expression cache.") {
  setSlotNull(vm, 0);
}

saynaa_function(_reExtract, "re.extract(pattern: String, text: String) -> List|Null",
                "Returns a list of captured groups.") {
  const char* pattern;
  const char* text;
  if (!ValidateSlotString(vm, 1, &pattern, NULL))
    return;
  if (!ValidateSlotString(vm, 2, &text, NULL))
    return;

  pcre2_code* re = compileRegex(vm, pattern);
  if (!re)
    return;

  pcre2_match_data* match_data = pcre2_match_data_create_from_pattern(re, NULL);
  int rc = pcre2_match(re, (PCRE2_SPTR) text, strlen(text), 0, 0, match_data, NULL);

  if (rc >= 0) {
    NewList(vm, 0);
    PCRE2_SIZE* ovector = pcre2_get_ovector_pointer(match_data);
    for (int i = 0; i < rc; i++) {
      setSlotStringLength(vm, 1, text + ovector[2 * i],
                          ovector[2 * i + 1] - ovector[2 * i]);
      ListInsert(vm, 0, -1, 1);
    }
  } else {
    setSlotNull(vm, 0);
  }

  pcre2_match_data_free(match_data);
  pcre2_code_free(re);
}

saynaa_function(_reFindAll, "re.findall(pattern: String, text: String) -> List",
                "Returns all non-overlapping matches.") {
  const char* pattern;
  const char* text;
  if (!ValidateSlotString(vm, 1, &pattern, NULL))
    return;
  if (!ValidateSlotString(vm, 2, &text, NULL))
    return;

  pcre2_code* re = compileRegex(vm, pattern);
  if (!re)
    return;

  NewList(vm, 0);
  pcre2_match_data* match_data = pcre2_match_data_create_from_pattern(re, NULL);
  PCRE2_SIZE offset = 0;
  PCRE2_SIZE len = strlen(text);

  while (offset < len) {
    int rc = pcre2_match(re, (PCRE2_SPTR) text, len, offset, 0, match_data, NULL);
    if (rc < 0)
      break;

    PCRE2_SIZE* ovector = pcre2_get_ovector_pointer(match_data);

    if (rc == 1) {
      setSlotStringLength(vm, 1, text + ovector[0], ovector[1] - ovector[0]);
      ListInsert(vm, 0, -1, 1);
    } else {
      NewList(vm, 1);
      for (int i = 1; i < rc; i++) {
        setSlotStringLength(vm, 2, text + ovector[2 * i],
                            ovector[2 * i + 1] - ovector[2 * i]);
        ListInsert(vm, 1, -1, 2);
      }
      ListInsert(vm, 0, -1, 1);
    }
    offset = ovector[1];
    if (ovector[0] == ovector[1])
      offset++;
  }

  pcre2_match_data_free(match_data);
  pcre2_code_free(re);
}

void registerModuleRegex(VM* vm) {
  Handle* re = NewModule(vm, "re");
  ModuleAddFunction(vm, re, "match", _reMatch, 2, NULL);
  ModuleAddFunction(vm, re, "fullmatch", _reFullMatch, 2, NULL);
  ModuleAddFunction(vm, re, "search", _reSearch, 2, NULL);
  ModuleAddFunction(vm, re, "sub", _reSub, -1, NULL);
  ModuleAddFunction(vm, re, "subn", _reSubn, -1, NULL);
  ModuleAddFunction(vm, re, "split", _reSplit, -1, NULL);
  ModuleAddFunction(vm, re, "extract", _reExtract, 2, NULL);
  ModuleAddFunction(vm, re, "findall", _reFindAll, 2, NULL);
  ModuleAddFunction(vm, re, "escape", _reEscape, 1, NULL);
  ModuleAddFunction(vm, re, "purge", _rePurge, 0, NULL);
  registerModule(vm, re);
  releaseHandle(vm, re);
}

#else

static void re_set_unavailable_error(VM* vm) {
  SetRuntimeError(
      vm, "Regex module is unavailable: PCRE2 headers/library are missing at build time.");
}

saynaa_function(_reMatch, "re.match(pattern: String, text: String) -> String|Null",
                "Regex module unavailable.") {
  re_set_unavailable_error(vm);
}

saynaa_function(_reFullMatch, "re.fullmatch(pattern: String, text: String) -> String|Null",
                "Regex module unavailable.") {
  re_set_unavailable_error(vm);
}

saynaa_function(_reSearch, "re.search(pattern: String, text: String) -> String|Null",
                "Regex module unavailable.") {
  re_set_unavailable_error(vm);
}

saynaa_function(_reSplit, "re.split(pattern: String, text: String, maxsplit: Int) -> List",
                "Regex module unavailable.") {
  re_set_unavailable_error(vm);
}

saynaa_function(_reSub, "re.sub(pattern: String, repl: String, text: String) -> String",
                "Regex module unavailable.") {
  re_set_unavailable_error(vm);
}

saynaa_function(_reSubn, "re.subn(pattern: String, repl: String, text: String) -> List",
                "Regex module unavailable.") {
  re_set_unavailable_error(vm);
}

saynaa_function(_reEscape, "re.escape(pattern: String) -> String", "Regex module unavailable.") {
  re_set_unavailable_error(vm);
}

saynaa_function(_rePurge, "re.purge() -> Null", "Regex module unavailable.") {
  re_set_unavailable_error(vm);
}

saynaa_function(_reExtract, "re.extract(pattern: String, text: String) -> List|Null",
                "Regex module unavailable.") {
  re_set_unavailable_error(vm);
}

saynaa_function(_reFindAll, "re.findall(pattern: String, text: String) -> List",
                "Regex module unavailable.") {
  re_set_unavailable_error(vm);
}

void registerModuleRegex(VM* vm) {
  Handle* re = NewModule(vm, "re");
  ModuleAddFunction(vm, re, "match", _reMatch, 2, NULL);
  ModuleAddFunction(vm, re, "fullmatch", _reFullMatch, 2, NULL);
  ModuleAddFunction(vm, re, "search", _reSearch, 2, NULL);
  ModuleAddFunction(vm, re, "sub", _reSub, -1, NULL);
  ModuleAddFunction(vm, re, "subn", _reSubn, -1, NULL);
  ModuleAddFunction(vm, re, "split", _reSplit, -1, NULL);
  ModuleAddFunction(vm, re, "extract", _reExtract, 2, NULL);
  ModuleAddFunction(vm, re, "findall", _reFindAll, 2, NULL);
  ModuleAddFunction(vm, re, "escape", _reEscape, 1, NULL);
  ModuleAddFunction(vm, re, "purge", _rePurge, 0, NULL);
  registerModule(vm, re);
  releaseHandle(vm, re);
}

#endif
