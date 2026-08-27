/*
 * Copyright (c) 2022-2026 Mohamed Abdifatah. All rights reserved.
 * Distributed Under The MIT License
 */

#include "saynaa_built_classes.h"

#include <math.h>

static void _listJoinImpl(VM* vm, List* list, String* sep) {
  ByteBuffer buff;
  ByteBufferInit(&buff);

  for (uint32_t i = 0; i < list->elements.count; i++) {
    String* str = varToString(vm, list->elements.data[i], false);
    if (str == NULL)
      RET(VAR_NULL);
    vmPushTempRef(vm, &str->_super); // elem
    if (sep != NULL && i != 0) {
      ByteBufferAddString(&buff, vm, sep->data, sep->length);
    }
    ByteBufferAddString(&buff, vm, str->data, str->length);
    vmPopTempRef(vm); // elem
  }

  String* str = newStringLength(vm, (const char*) buff.data, buff.count);
  ByteBufferClear(&buff, vm);
  RET(VAR_OBJ(str));
}

/*****************************************************************************/
/* BUILTIN CLASS CONSTRUCTORS                                                */
/*****************************************************************************/

static void _ctorNull(VM* vm) {
  RET(VAR_NULL);
}

static void _ctorBool(VM* vm) {
  RET(VAR_BOOL(toBool(ARG(1))));
}

static void _ctorNumber(VM* vm) {
  double value;

  if (isNumeric(ARG(1), &value)) {
    RET(VAR_NUM(value));
  }

  if (IS_OBJ_TYPE(ARG(1), OBJ_STRING)) {
    String* str = (String*) AS_OBJ(ARG(1));
    const char* err = utilToNumber(str->data, &value);
    if (err == NULL)
      RET(VAR_NUM(value));
    VM_SET_ERROR(vm, newString(vm, err));
    RET(VAR_NULL);
  }

  VM_SET_ERROR(vm, newString(vm, "Argument must be numeric or string."));
}

static void _ctorPointer(VM* vm) {
  if (!CheckArgcRange(vm, ARGC, 1, 1)) {
    VM_SET_ERROR(vm, newString(vm, "Argument must be Pointer"));
    return;
  }

  if (IS_OBJ_TYPE(ARG(1), OBJ_POINTER)) {
    Pointer* ptr = (Pointer*) AS_OBJ(ARG(1));
    RET(VAR_OBJ(newPointer(vm, ptr->native_ptr, ptr->destructor)));
  } else {
    VM_SET_ERROR(vm, newString(vm, "Argument must be Pointer"));
  }
}

static void _ctorString(VM* vm) {
  if (!CheckArgcRange(vm, ARGC, 0, 1))
    return;
  if (ARGC == 0) {
    RET(VAR_OBJ(newStringLength(vm, NULL, 0)));
    return;
  }
  String* str = varToString(vm, ARG(1), false);
  if (str == NULL)
    RET(VAR_NULL);
  RET(VAR_OBJ(str));
}

static void _ctorList(VM* vm) {
  List* list = newList(vm, ARGC);
  vmPushTempRef(vm, &list->_super); // list.
  for (int i = 0; i < ARGC; i++) {
    listAppend(vm, list, ARG(i + 1));
  }
  vmPopTempRef(vm); // list.
  RET(VAR_OBJ(list));
}

static void _ctorMap(VM* vm) {
  RET(VAR_OBJ(newMap(vm)));
}

static void _ctorRange(VM* vm) {
  double from, to;
  if (!validateNumeric(vm, ARG(1), &from, "Argument 1"))
    return;
  if (!validateNumeric(vm, ARG(2), &to, "Argument 2"))
    return;

  RET(VAR_OBJ(newRange(vm, from, to)));
}

static void _ctorFiber(VM* vm) {
  Closure* closure;
  if (!validateArgClosure(vm, 1, &closure))
    return;
  RET(VAR_OBJ(newFiber(vm, closure)));
}

/*****************************************************************************/
/* BUILTIN CLASS METHODS                                                     */
/*****************************************************************************/

#define THIS (vm->fiber->thiz)
saynaa_function(_objTypeName, "Object.typename() -> String",
                "Returns the type name of the object.") {
  RET(VAR_OBJ(newString(vm, varTypeName(THIS))));
}

saynaa_function(_objRepr, "Object._repr() -> String", "Returns the repr string of the object.") {
  RET(VAR_OBJ(toRepr(vm, THIS)));
}

saynaa_function(_objGetattr, "Object.getattr(name:String[, skipGetter: bool]) -> Var",
                "Returns the value of the named attribute of an object.") {
  if (!CheckArgcRange(vm, ARGC, 1, 2))
    return;

  String* name;
  if (!validateArgString(vm, 1, &name))
    return;

  bool skipGetter = (ARGC >= 2 ? toBool(ARG(2)) : false);
  RET(varGetAttrib(vm, THIS, name, skipGetter, false));
}

saynaa_function(_objSetattr, "Object.setattr(name:String, value:Var[, skipSetter: bool]) -> Null",
                "Sets the value of the attribute of an object.") {
  if (!CheckArgcRange(vm, ARGC, 2, 3))
    return;

  String* name;
  if (!validateArgString(vm, 1, &name))
    return;

  bool skipSetter = (ARGC >= 3 ? toBool(ARG(3)) : false);
  varSetAttrib(vm, THIS, name, ARG(2), skipSetter);
}

saynaa_function(_objNew, "Object._new([cls:Class]) -> Var",
                "Allocate an instance of [cls] (or this class)"
                " without calling _new/_init.") {
  Class* cls = NULL;
  if (!CheckArgcRange(vm, ARGC, 0, 1))
    return;

  if (ARGC == 1) {
    if (!validateArgClass(vm, 1, &cls))
      return;
  } else {
    if (!IS_OBJ_TYPE(THIS, OBJ_CLASS)) {
      RET_ERR(newString(vm, "_new requires a Class context."));
    }
    cls = (Class*) AS_OBJ(THIS);
  }

  RET(preConstructThis(vm, cls));
}

saynaa_function(_objDelattr, "Object.delattr(name:String[, skipDelattr: bool]) -> Null",
                "Deletes the named attribute of an object.") {
  if (!CheckArgcRange(vm, ARGC, 1, 2))
    return;

  String* name;
  if (!validateArgString(vm, 1, &name))
    return;

  bool skipDelattr = (ARGC >= 2 ? toBool(ARG(2)) : false);
  varDelAttrib(vm, THIS, name, skipDelattr);
}

saynaa_function(
    _numberTimes, "Number.times(f:Closure)",
    "Iterate the function [f] n times. Here n is the integral value of the "
    "number. If the number is not an integer the floor value will be taken.") {
  ASSERT(IS_NUM(THIS), OOPS);
  double n = AS_NUM(THIS);

  Closure* closure;
  if (!validateArgClosure(vm, 1, &closure))
    return;

  for (int64_t i = 0; i < n; i++) {
    Var _i = VAR_NUM((double) i);
    Result result = vmCallFunction(vm, closure, 1, &_i, NULL);
    if (result != RESULT_SUCCESS)
      break;
  }

  RET(VAR_NULL);
}

saynaa_function(_numberIsint, "Number.isint() -> Bool",
                "Returns true if the number"
                " is a whole number, otherwise false.") {
  double n = AS_NUM(THIS);
  RET(VAR_BOOL(floor(n) == n));
}

saynaa_function(_numberIsbyte, "Number.isbyte() -> bool",
                "Returns true if the number"
                " is an integer and is between 0x00 and 0xff.") {
  double n = AS_NUM(THIS);
  RET(VAR_BOOL((floor(n) == n) && (0x00 <= n && n <= 0xff)));
}

saynaa_function(_stringFind, "String.find(sub:String[, start:Number=0]) -> Number", "Returns the first index of the substring [sub] found from the [start] index") {
  if (!CheckArgcRange(vm, ARGC, 1, 2))
    return;

  String* sub;
  if (!validateArgString(vm, 1, &sub))
    return;

  int64_t start = 0;

  if (ARGC == 2) {
    if (!validateInteger(vm, ARG(2), &start, "Argument 2"))
      return;
  }

  String* thiz = (String*) AS_OBJ(THIS);

  int char_length = utf8_length(thiz->data);

  if (char_length < 0) {
    RET_ERR(newString(vm, "Invalid UTF-8 string."));
  }

  if (start < 0)
    start = char_length + start;

  if (start < 0)
    start = 0;

  if (start > char_length)
    RET(VAR_NUM(-1));

  int start_byte = utf8_byteOffset(thiz->data, (size_t) start);

  if (start_byte < 0)
    RET_ERR(newString(vm, "Invalid UTF-8 string."));

  // Search bytes, but return a Unicode character index.
  const char* match = (const char*) utilMemMem(thiz->data + start_byte,
                                               thiz->length - (size_t) start_byte,
                                               sub->data, sub->length);

  if (match == NULL)
    RET(VAR_NUM(-1));

  size_t match_byte = (size_t) (match - thiz->data);

  int result = utf8_length(thiz->data);

  if (result < 0)
    RET_ERR(newString(vm, "Invalid UTF-8 string."));

  // Count characters before the match.
  size_t pos = 0;
  int char_index = 0;

  while (pos < match_byte) {
    int value;

    int bytes = utf8_decodeBytes((const uint8_t*) (thiz->data + pos), &value);

    if (bytes <= 0)
      RET_ERR(newString(vm, "Invalid UTF-8 string."));

    pos += bytes;
    char_index++;
  }

  RET(VAR_NUM((double) char_index));
}

saynaa_function(_stringRFind, "String.rfind(sub:String[, start:Number=0]) -> Number",
                "Returns the last index of the "
                "substring [sub] found from the "
                "[start] index") {
  if (!CheckArgcRange(vm, ARGC, 1, 2))
    return;

  String* sub;

  if (!validateArgString(vm, 1, &sub))
    return;

  int64_t start = 0;

  if (ARGC == 2) {
    if (!validateInteger(vm, ARG(2), &start, "Argument 2"))
      return;
  }

  String* thiz = (String*) AS_OBJ(THIS);

  /*
   * String indexes are Unicode character indexes.
   *
   * Example:
   *
   * "Hello 😀"
   *
   * character indexes:
   *
   * 0 1 2 3 4 5 6
   * H e l l o   😀
   */
  int char_length = utf8_length(thiz->data);

  if (char_length < 0) {
    RET_ERR(newString(vm, "Invalid UTF-8 string."));
  }

  /*
   * Normalize negative start.
   */
  if (start < 0)
    start = (int64_t) char_length + start;

  if (start < 0)
    start = 0;

  /*
   * start points to the first character where
   * searching is allowed.
   */
  if (start >= char_length) {
    RET(VAR_NUM(-1));
  }

  /*
   * Convert Unicode character index to byte offset.
   */
  int start_byte = utf8_byteOffset(thiz->data, (size_t) start);

  if (start_byte < 0) {
    RET_ERR(newString(vm, "Invalid UTF-8 string."));
  }

  /*
   * Empty substring.
   *
   * The last possible position from the search range
   * is the end of the string.
   */
  if (sub->length == 0) {
    RET(VAR_NUM((double) char_length));
  }

  /*
   * We search bytes internally because UTF-8 is variable-length.
   */
  const char* haystack = thiz->data + start_byte;

  size_t haystack_len = thiz->length - (size_t) start_byte;

  size_t needle_len = sub->length;

  if (needle_len > haystack_len) {
    RET(VAR_NUM(-1));
  }

  /*
   * Search backwards.
   */
  for (size_t i = haystack_len - needle_len + 1; i > 0; i--) {
    size_t offset = i - 1;

    if (memcmp(haystack + offset, sub->data, needle_len) != 0) {
      continue;
    }

    /*
     * We found a byte match.
     *
     * Make sure the match starts on a UTF-8
     * character boundary.
     */
    size_t match_byte = (size_t) (haystack - thiz->data) + offset;

    int result = utf8_charIndexAtByteOffset(thiz->data, match_byte);

    if (result < 0) {
      /*
       * The byte sequence matched in the middle
       * of a UTF-8 character. Do not treat it as
       * a valid String match.
       */
      continue;
    }

    /*
     * Also make sure the substring ends on a
     * UTF-8 character boundary.
     */
    int end_result = utf8_charIndexAtByteOffset(thiz->data, match_byte + needle_len);

    if (end_result < 0)
      continue;

    RET(VAR_NUM((double) result));
  }

  RET(VAR_NUM(-1));
}

saynaa_function(
    _stringSub, "String.sub(start:Number[, end:Number]) -> String",
    "Returns the substring from [start] (inclusive) to [end] (exclusive).") {
  if (!CheckArgcRange(vm, ARGC, 1, 2))
    return;

  int64_t start;

  if (!validateInteger(vm, ARG(1), &start, "Argument 1"))
    return;

  String* thiz = (String*) AS_OBJ(THIS);

  int char_length = utf8_length(thiz->data);

  if (char_length < 0) {
    RET_ERR(newString(vm, "Invalid UTF-8 string."));
  }

  int64_t end = char_length;

  if (ARGC == 2) {
    if (!validateInteger(vm, ARG(2), &end, "Argument 2"))
      return;
  }

  if (start < 0)
    start = char_length + start;

  if (end < 0)
    end = char_length + end;

  if (start < 0)
    start = 0;

  if (end > char_length)
    end = char_length;

  if (start >= end)
    RET(VAR_OBJ(newStringLength(vm, NULL, 0)));

  int start_byte = utf8_byteOffset(thiz->data, (size_t) start);

  int end_byte = utf8_byteOffset(thiz->data, (size_t) end);

  if (start_byte < 0 || end_byte < 0) {
    RET_ERR(newString(vm, "Invalid UTF-8 string."));
  }

  RET(VAR_OBJ(newStringLength(vm, thiz->data + start_byte, (uint32_t) (end_byte - start_byte))));
}

saynaa_function(_stringReverse, "String.reverse() -> String", "Returns a copy of the string with reversed Unicode characters.") {
  String* thiz = (String*) AS_OBJ(THIS);

  if (thiz->length == 0)
    RET(THIS);

  char* buff = (char*) Realloc(vm, NULL, thiz->length);
  memcpy(buff, thiz->data, thiz->length);
  utf8_reverse(buff, thiz->length);

  String* out = newStringLength(vm, buff, thiz->length);
  Realloc(vm, buff, 0);
  RET(VAR_OBJ(out));
}

saynaa_function(_stringRep, "String.rep(count:Number) -> String",
                "Returns a new string repeated [count] times.") {
  int64_t count = 0;
  if (!validateInteger(vm, ARG(1), &count, "Argument 1"))
    return;
  if (count < 0) {
    RET_ERR(newString(vm, "count should be >= 0"));
  }

  String* thiz = (String*) AS_OBJ(THIS);
  if (count == 0 || thiz->length == 0)
    RET(VAR_OBJ(newStringLength(vm, NULL, 0)));

  uint64_t total = (uint64_t) thiz->length * (uint64_t) count;
  if (total > UINT32_MAX) {
    RET_ERR(newString(vm, "Resulting string is too large."));
  }

  char* buff = (char*) Realloc(vm, NULL, (size_t) total);
  char* dst = buff;
  for (int64_t i = 0; i < count; i++) {
    memcpy(dst, thiz->data, thiz->length);
    dst += thiz->length;
  }
  String* out = newStringLength(vm, buff, (uint32_t) total);
  Realloc(vm, buff, 0);
  RET(VAR_OBJ(out));
}

saynaa_function(_stringByte, "String.byte(index:Number) -> Number",
                " Returns the UTF - 8 byte value at[index].") {
  int64_t index = 0;
  if (!validateInteger(vm, ARG(1), &index, "Argument 1"))
    return;

  String* thiz = (String*) AS_OBJ(THIS);
  if (index < 0)
    index = (int64_t) thiz->length + index;
  if (index < 0 || index >= (int64_t) thiz->length) {
    RET_ERR(newString(vm, "String.byte index out of bounds."));
  }

  RET(VAR_NUM((double) (uint8_t) thiz->data[index]));
}

saynaa_function(_stringFormat, "String.format(...args) -> String",
                "Formats the string using printf-style specifiers.") {
  String* thiz = (String*) AS_OBJ(THIS);
  List* args = newList(vm, (uint32_t) ARGC);
  vmPushTempRef(vm, &args->_super); // args.
  for (int i = 0; i < ARGC; i++) {
    listAppend(vm, args, ARG(i + 1));
  }
  Var ret = varSprintf(vm, thiz, args);
  vmPopTempRef(vm); // args.
  RET(ret);
}

saynaa_function(_stringMatch, "String.match(sub:String[, start:Number=0]) -> String|Null",
                "Returns the first match of [sub] starting at [start].") {
  if (!CheckArgcRange(vm, ARGC, 1, 2))
    return;

  String* sub;

  if (!validateArgString(vm, 1, &sub))
    return;

  int64_t start = 0;

  if (ARGC == 2) {
    if (!validateInteger(vm, ARG(2), &start, "Argument 2"))
      return;
  }

  String* thiz = (String*) AS_OBJ(THIS);

  int char_length = utf8_length(thiz->data);

  if (char_length < 0) {
    RET_ERR(newString(vm, "Invalid UTF-8 string."));
  }

  // Normalize negative index.
  if (start < 0)
    start = (int64_t) char_length + start;

  if (start < 0)
    start = 0;

  if (start >= char_length) {
    RET(VAR_NULL);
  }

  // Convert character index -> byte offset.
  int start_byte = utf8_byteOffset(thiz->data, (size_t) start);

  if (start_byte < 0) {
    RET_ERR(newString(vm, "Invalid UTF-8 string."));
  }

  const char* match = (const char*) utilMemMem(thiz->data + start_byte,
                                               thiz->length - (size_t) start_byte,
                                               sub->data, sub->length);

  if (match == NULL)
    RET(VAR_NULL);

  RET(VAR_OBJ(newStringLength(vm, match, sub->length)));
}

saynaa_function(_stringGSub, "String.gsub(old:String, new:String[, count:Number=-1]) -> String",
                "Replace occurrences of [old] with [new].") {
  if (!CheckArgcRange(vm, ARGC, 2, 3))
    return;

  String *old, *new_;
  if (!validateArgString(vm, 1, &old))
    return;
  if (!validateArgString(vm, 2, &new_))
    return;

  int64_t count = -1;
  if (ARGC == 3) {
    if (!validateInteger(vm, ARG(3), &count, "Argument 3"))
      return;
    if (count < 0 && count != -1) {
      RET_ERR(newString(vm, "count should either be >= 0 or -1"));
    }
  }

  String* thiz = (String*) AS_OBJ(THIS);

  String* result = stringReplace(vm, thiz, old, new_, (int32_t) count);
  if (result == NULL) {
    RET_ERR(newString(vm, "Failed to replace string."));
  }

  String* out = newStringLength(vm, result->data, result->length);
  RET(VAR_OBJ(out));
}

saynaa_function(_stringGMatch, "String.gmatch(sub:String) -> List",
                "Returns a list of all matches of [sub].") {
  String* sub;
  if (!validateArgString(vm, 1, &sub))
    return;
  if (sub->length == 0) {
    RET_ERR(newString(vm, "sub must not be empty."));
  }

  String* thiz = (String*) AS_OBJ(THIS);
  List* list = newList(vm, 0);
  vmPushTempRef(vm, &list->_super); // list.

  const char* cursor = thiz->data;
  size_t remaining = thiz->length;
  while (remaining >= sub->length) {
    const char* match = (const char*) utilMemMem(cursor, remaining, sub->data, sub->length);
    if (match == NULL)
      break;
    String* m = newStringLength(vm, match, sub->length);
    vmPushTempRef(vm, &m->_super); // m.
    listAppend(vm, list, VAR_OBJ(m));
    vmPopTempRef(vm); // m.

    size_t consumed = (size_t) (match - cursor) + sub->length;
    cursor += consumed;
    remaining -= consumed;
  }

  vmPopTempRef(vm); // list.
  RET(VAR_OBJ(list));
}

saynaa_function(
    _stringReplace, "String.replace(old:Sttring, new:String[, count:Number=-1]) -> String",
    "Returns a copy of the string where [count] occurrence of the substring "
    "[old] will be replaced with [new]. If [count] == -1 all the occurrence "
    "will be replaced.") {
  if (!CheckArgcRange(vm, ARGC, 2, 3))
    return;

  String *old, *new_;
  if (!validateArgString(vm, 1, &old))
    return;
  if (!validateArgString(vm, 2, &new_))
    return;

  String* thiz = (String*) AS_OBJ(THIS);

  int64_t count = -1;
  if (ARGC == 3) {
    if (!validateInteger(vm, ARG(3), &count, "Argument 3"))
      return;
    if (count < 0 && count != -1) {
      RET_ERR(newString(vm, "count should either be >= 0 or -1"));
    }
  }

  String* result = stringReplace(vm, thiz, old, new_, (int32_t) count);
  if (result == NULL) {
    RET_ERR(newString(vm, "Failed to replace string."));
  }

  String* out = newStringLength(vm, result->data, result->length);
  RET(VAR_OBJ(out));
}

saynaa_function(_stringSplit, "String.split([sep:String]) -> List",
                "Split the string into a list"
                " of string separated by [sep] delimiter.") {
  if (!CheckArgcRange(vm, ARGC, 0, 1))
    return;
  String* sep = NULL;

  if (ARGC == 1)
    sep = varToString(vm, ARG(1), false);
  RET(VAR_OBJ(stringSplit(vm, (String*) AS_OBJ(THIS), sep)));
}

saynaa_function(
    _stringStrip, "String.strip() -> String",
    "Returns a copy of the string where the leading and trailing whitespace "
    "removed.") {
  RET(VAR_OBJ(stringStrip(vm, (String*) AS_OBJ(THIS))));
}

saynaa_function(
    _stringLower, "String.lower() -> String",
    "Returns a copy of the string where all the characters are converted to "
    "lower case letters.") {
  RET(VAR_OBJ(stringLower(vm, (String*) AS_OBJ(THIS))));
}

saynaa_function(
    _stringUpper, "String.upper() -> String",
    "Returns a copy of the string where all the characters are converted to "
    "upper case letters.") {
  RET(VAR_OBJ(stringUpper(vm, (String*) AS_OBJ(THIS))));
}

saynaa_function(_stringStartswith,
                "String.startswith(prefix: String | List) -> Bool", "Returns true if the string starts with the specified prefix.") {
  if (!CheckArgcRange(vm, ARGC, 1, 1))
    return;

  Var prefix = ARG(1);
  String* thiz = (String*) AS_OBJ(THIS);

  /*
   * String prefix.
   *
   * UTF-8 is stored as bytes, so comparing the complete
   * UTF-8 byte sequence is safe here.
   */
  if (IS_OBJ_TYPE(prefix, OBJ_STRING)) {
    String* pre = (String*) AS_OBJ(prefix);

    if (pre->length > thiz->length)
      RET(VAR_FALSE);

    if (pre->length == 0)
      RET(VAR_TRUE);

    RET(VAR_BOOL(memcmp(thiz->data, pre->data, pre->length) == 0));
  }

  /*
   * List of prefixes.
   */
  if (IS_OBJ_TYPE(prefix, OBJ_LIST)) {
    List* prefixes = (List*) AS_OBJ(prefix);

    for (uint32_t i = 0; i < prefixes->elements.count; i++) {
      Var pre_var = prefixes->elements.data[i];

      if (!IS_OBJ_TYPE(pre_var, OBJ_STRING)) {
        RET_ERR(newString(vm, "Expected a String for prefix."));
      }

      String* pre = (String*) AS_OBJ(pre_var);

      /*
       * This prefix cannot match, but another prefix
       * in the list might.
       */
      if (pre->length > thiz->length)
        continue;

      /*
       * Empty string always matches.
       */
      if (pre->length == 0)
        RET(VAR_TRUE);

      if (memcmp(thiz->data, pre->data, pre->length) == 0) {
        RET(VAR_TRUE);
      }
    }

    RET(VAR_FALSE);
  }

  RET_ERR(newString(vm, "Expected a String or a List of prefixes."));
}

saynaa_function(_stringEndswith, "String.endswith(suffix: String | List) -> Bool",
                "Returns true if the string ends with the specified suffix.") {
  if (!CheckArgcRange(vm, ARGC, 1, 1))
    return;

  Var suffix = ARG(1);
  String* thiz = (String*) AS_OBJ(THIS);

  /*
   * String suffix.
   */
  if (IS_OBJ_TYPE(suffix, OBJ_STRING)) {
    String* suf = (String*) AS_OBJ(suffix);

    if (suf->length > thiz->length)
      RET(VAR_FALSE);

    if (suf->length == 0)
      RET(VAR_TRUE);

    const char* start = thiz->data + (thiz->length - suf->length);

    RET(VAR_BOOL(memcmp(start, suf->data, suf->length) == 0));
  }

  /*
   * List of suffixes.
   */
  if (IS_OBJ_TYPE(suffix, OBJ_LIST)) {
    List* suffixes = (List*) AS_OBJ(suffix);

    for (uint32_t i = 0; i < suffixes->elements.count; i++) {
      Var suff_var = suffixes->elements.data[i];

      if (!IS_OBJ_TYPE(suff_var, OBJ_STRING)) {
        RET_ERR(newString(vm, "Expected a String for suffix."));
      }

      String* suf = (String*) AS_OBJ(suff_var);

      /*
       * This suffix cannot match, but another suffix
       * in the list might.
       */
      if (suf->length > thiz->length)
        continue;

      /*
       * Empty string always matches.
       */
      if (suf->length == 0)
        RET(VAR_TRUE);

      const char* start = thiz->data + (thiz->length - suf->length);

      if (memcmp(start, suf->data, suf->length) == 0) {
        RET(VAR_TRUE);
      }
    }

    RET(VAR_FALSE);
  }

  RET_ERR(newString(vm, "Expected a String or a List of suffixes."));
}

saynaa_function(_listAppend, "List.append(value:Var) -> List",
                "Append the [value] to the list and return the List.") {
  ASSERT(IS_OBJ_TYPE(THIS, OBJ_LIST), OOPS);

  listAppend(vm, ((List*) AS_OBJ(THIS)), ARG(1));
  RET(THIS);
}

saynaa_function(_listInsert, "List.insert(index:Number, value:Var) -> Null",
                "Insert the element at the given index. The index should be "
                "0 <= index <= list.length.") {
  List* thiz = (List*) AS_OBJ(THIS);

  int64_t index;
  if (!validateInteger(vm, ARG(1), &index, "Argument 1"))
    return;

  if (index < 0 || index > thiz->elements.count) {
    RET_ERR(newString(vm, "List.insert index out of bounds."));
  }

  listInsert(vm, thiz, (uint32_t) index, ARG(2));
}

saynaa_function(_listPop, "List.pop(index:Number=-1) -> Var",
                "Removes the last element of the list and return it.") {
  ASSERT(IS_OBJ_TYPE(THIS, OBJ_LIST), OOPS);
  List* thiz = (List*) AS_OBJ(THIS);

  if (!CheckArgcRange(vm, ARGC, 0, 1))
    return;

  if (thiz->elements.count == 0) {
    RET_ERR(newString(vm, "Cannot pop from an empty list."));
  }

  int64_t index = -1;
  if (ARGC == 1) {
    if (!validateInteger(vm, ARG(1), &index, "Argument 1"))
      return;
  }
  if (index < 0)
    index = thiz->elements.count + index;

  if (index < 0 || index >= thiz->elements.count) {
    RET_ERR(newString(vm, "List.pop index out of bounds."));
  }
  RET(listRemoveAt(vm, thiz, (uint32_t) index));
}

saynaa_function(_listFind, "List.find(value:Var) -> Number",
                "Find the value and return its index. If the vlaue not exists "
                "it'll return -1.") {
  ASSERT(IS_OBJ_TYPE(THIS, OBJ_LIST), OOPS);
  List* thiz = (List*) AS_OBJ(THIS);

  Var* it = thiz->elements.data;
  if (it == NULL)
    RET(VAR_NUM(-1)); // Empty list.

  for (; it < thiz->elements.data + thiz->elements.count; it++) {
    if (isValuesEqual(*it, ARG(1))) {
      RET(VAR_NUM((double) (it - thiz->elements.data)));
    }
  }

  RET(VAR_NUM(-1));
}

saynaa_function(
    _listJoin,
    "List.join([sep:String="
    "]) -> String",
    "Concatinate the elements of the list and return as a string.") {
  ASSERT(IS_OBJ_TYPE(THIS, OBJ_LIST), OOPS);
  List* list = (List*) AS_OBJ(THIS);
  String* sep = NULL;

  if (!CheckArgcRange(vm, ARGC, 0, 1))
    return;
  if (ARGC == 1)
    sep = varToString(vm, ARG(1), false);

  _listJoinImpl(vm, list, sep);
}

saynaa_function(_listClear, "List.clear() -> Null", "Removes all the entries in the list.") {
  listClear(vm, (List*) AS_OBJ(THIS));
}

saynaa_function(_mapClear, "Map.clear() -> Null", "Removes all the entries in the map.") {
  Map* thiz = (Map*) AS_OBJ(THIS);
  mapClear(vm, thiz);
}

saynaa_function(_listResize, "List.resize(length:Number) -> List",
                "Resize a list to length and return the List.") {
  ASSERT(IS_OBJ_TYPE(THIS, OBJ_LIST), OOPS);
  List* thiz = (List*) AS_OBJ(THIS);

  int64_t len;
  if (!validateInteger(vm, ARG(1), &len, "Argument 1"))
    return;

  if (len < 0) { // negative value to reduce the size.
    len = thiz->elements.count + len;
  }
  if (len < 0) {
    RET_ERR(newString(vm, "List.resize index out of bounds."));
  }

  if (len == 0) {
    listClear(vm, thiz);

  } else if (len > thiz->elements.count) {
    VarBufferFill(&thiz->elements, vm, VAR_NULL, len - thiz->elements.count);

  } else if (len < thiz->elements.count) {
    thiz->elements.count = len;
    listShrink(vm, thiz);
  }

  RET(THIS);
}

saynaa_function(
    _mapSet, "Map.set([key:Var,] value:Var) -> Map",
    "Sets the value at the key in the map."
    " If the key is not provided it'll use the next available index.") {
  if (!CheckArgcRange(vm, ARGC, 1, 2))
    return;

  ASSERT(IS_OBJ_TYPE(THIS, OBJ_MAP), OOPS);

  Map* thiz = (Map*) AS_OBJ(THIS);
  Var key = (ARGC == 1) ? VAR_NUM((double) thiz->next_index) : ARG(1);

  mapSet(vm, thiz, key, ARG(ARGC == 1 ? 1 : 2));
  RET(THIS);
}

saynaa_function(
    _mapGet, "Map.get(key:Var, default=Null) -> Var",
    "Returns the key if its in the map, otherwise the default value will "
    "be returned.") {
  if (!CheckArgcRange(vm, ARGC, 1, 2))
    return;

  Var default_ = (ARGC == 1) ? VAR_NULL : ARG(2);

  Map* thiz = (Map*) AS_OBJ(THIS);

  Var value = mapGet(thiz, ARG(1));
  if (IS_UNDEF(value))
    RET(default_);
  RET(value);
}

saynaa_function(_mapHas, "Map.has(key:Var) -> Bool", "Returns true if the key exists.") {
  Map* thiz = (Map*) AS_OBJ(THIS);
  Var value = mapGet(thiz, ARG(1));
  RET(VAR_BOOL(!IS_UNDEF(value)));
}

saynaa_function(_mapPop, "Map.pop(key:Var) -> Var", "Pops the value at the key and return it.") {
  Map* thiz = (Map*) AS_OBJ(THIS);
  Var value = mapRemoveKey(vm, thiz, ARG(1));
  if (IS_UNDEF(value)) {
    RET_ERR(stringFormat(vm, "Key '@' does not exists.", toRepr(vm, ARG(1))));
  }
  RET(value);
}

saynaa_function(
    _methodBindBind, "MethodBind.bind(instance:Var) -> MethodBind",
    "Bind the method to the instance and the method bind will be returned. The "
    "method should be a valid method of the instance. ie. the instance's "
    "interitance tree should contain the method.") {
  MethodBind* thiz = (MethodBind*) AS_OBJ(THIS);

  // We can only bind the method if the instance has that method.
  String* method_name = newString(vm, thiz->method->fn->name);
  vmPushTempRef(vm, &method_name->_super); // method_name.

  Var instance = ARG(1);

  Closure* method;
  if (!hasMethod(vm, instance, method_name, &method) || method != thiz->method) {
    VM_SET_ERROR(vm, newString(vm, "Cannot bind method, instance and method "
                                   "types miss-match."));
    return;
  }

  thiz->instance = instance;
  vmPopTempRef(vm); // method_name.

  RET(THIS);
}

saynaa_function(_classMethods, "Class.methods() -> List",
                "Returns a list of unbound MethodBind of the class.") {
  Class* thiz = (Class*) AS_OBJ(THIS);

  List* list = newList(vm, thiz->methods.count);
  vmPushTempRef(vm, &list->_super); // list.
  for (int i = 0; i < (int) thiz->methods.count; i++) {
    Closure* method = thiz->methods.data[i];
    ASSERT(method->fn->name, OOPS);
    if (method->fn->name[0] == SPECIAL_NAME_CHAR)
      continue;
    MethodBind* mb = newMethodBind(vm, method);
    vmPushTempRef(vm, &mb->_super); // mb.
    listAppend(vm, list, VAR_OBJ(mb));
    vmPopTempRef(vm); // mb.
  }
  vmPopTempRef(vm); // list.

  RET(VAR_OBJ(list));
}

saynaa_function(_moduleDefine, "Module.define(variable:String, value:Var) -> Null",
                "Define a global variable in the module."
                " with the name [variable] and value [value]") {
  String* variable;
  if (!validateArgString(vm, 1, &variable))
    return;

  Var valua = ARG(2);

  Module* thiz = (Module*) AS_OBJ(THIS);

  moduleSetGlobal(vm, thiz, variable->data, variable->length, valua);

  RET(VAR_NULL);
}

saynaa_function(_moduleDelete, "Module.delete(variable:String) -> Null",
                "Delete a global variable in the module with the name "
                "[variable].") {
  String* variable;
  if (!validateArgString(vm, 1, &variable))
    return;

  Module* thiz = (Module*) AS_OBJ(THIS);
  if (!moduleDeleteGlobal(vm, thiz, variable->data, variable->length)) {
    VM_SET_ERROR(vm, stringFormat(vm, "Name '@' is not defined.", variable));
  }

  RET(VAR_NULL);
}

saynaa_function(
    _fiberRun, "Fiber.run(...) -> Var",
    "Runs the fiber's function with the provided arguments and returns it's "
    "return value or the yielded value if it's yielded.") {
  ASSERT(IS_OBJ_TYPE(THIS, OBJ_FIBER), OOPS);
  Fiber* thiz = (Fiber*) AS_OBJ(THIS);

  // Switch fiber and start execution. New fibers are marked as running in
  // either it's stats running with vmRunFiber() or here -- inserting a
  // fiber over a running (callee) fiber.
  if (vmPrepareFiber(vm, thiz, ARGC, &ARG(1))) {
    thiz->caller = vm->fiber;
    vm->fiber = thiz;
    thiz->state = FIBER_RUNNING;
  }
}

saynaa_function(
    _fiberResume, "Fiber.resume() -> Var",
    "Resumes a yielded function from a previous call of fiber_run() function. "
    "Return it's return value or the yielded value if it's yielded.") {
  ASSERT(IS_OBJ_TYPE(THIS, OBJ_FIBER), OOPS);
  Fiber* thiz = (Fiber*) AS_OBJ(THIS);

  if (!CheckArgcRange(vm, ARGC, 0, 1))
    return;

  Var value = (ARGC == 1) ? ARG(1) : VAR_NULL;

  // Switch fiber and resume execution.
  if (vmSwitchFiber(vm, thiz, &value)) {
    thiz->state = FIBER_RUNNING;
  }
}

#undef THIS

/*****************************************************************************/
/* BUILTIN CLASS INITIALIZATION                                              */
/*****************************************************************************/

void initializeBuiltinClasses(VM* vm) {
  for (int i = 0; i < vINSTANCE; i++) {
    Class* super = NULL;
    if (i != 0)
      super = vm->builtin_classes[vOBJECT];
    const char* name = getVarTypeName((VarType) i);
    Class* cls = newClass(vm, name, (int) strlen(name), super, NULL, NULL, NULL);
    vm->builtin_classes[i] = cls;
    cls->class_of = (VarType) i;
  }

#define ADD_CTOR(type, name, ptr, arity_) \
  do { \
    Function* fn = newFunction(vm, name, (int) strlen(name), NULL, true, NULL, NULL); \
    fn->native = ptr; \
    fn->arity = arity_; \
    vmPushTempRef(vm, &fn->_super); /* fn. */ \
    vm->builtin_classes[type]->magic_methods[METHOD_INIT] = newClosure(vm, fn); \
    vmPopTempRef(vm); /* fn. */ \
  } while (false)

  ADD_CTOR(vNULL, "@ctorNull", _ctorNull, 0);
  ADD_CTOR(vBOOL, "@ctorBool", _ctorBool, 1);
  ADD_CTOR(vNUMBER, "@ctorNumber", _ctorNumber, 1);
  ADD_CTOR(vSTRING, "@ctorString", _ctorString, -1);
  ADD_CTOR(vRANGE, "@ctorRange", _ctorRange, 2);
  ADD_CTOR(vLIST, "@ctorList", _ctorList, -1);
  ADD_CTOR(vMAP, "@ctorMap", _ctorMap, 0);
  ADD_CTOR(vFIBER, "@ctorFiber", _ctorFiber, 1);
  ADD_CTOR(vPOINTER, "@ctorPointer", _ctorPointer, 1);
#undef ADD_CTOR

#define ADD_METHOD(type, name, ptr, arity_) \
  do { \
    Function* fn = newFunction(vm, name, (int) strlen(name), NULL, true, \
                               DOCSTRING(ptr), NULL); \
    fn->is_method = true; \
    fn->native = ptr; \
    fn->arity = arity_; \
    vmPushTempRef(vm, &fn->_super); /* fn. */ \
    Closure* method = newClosure(vm, fn); \
    vmPushTempRef(vm, &method->_super); /* method. */ \
    ClosureBufferWrite(&vm->builtin_classes[type]->methods, vm, method); \
    if (vm->builtin_classes[type]->method_lookup == NULL) { \
      vm->builtin_classes[type]->method_lookup = newMap(vm); \
    } \
    String* method_name = newInternedString(vm, name); \
    vmPushTempRef(vm, &method_name->_super); /* method_name. */ \
    mapSet(vm, vm->builtin_classes[type]->method_lookup, VAR_OBJ(method_name), \
           VAR_OBJ(method)); \
    vmPopTempRef(vm); /* method_name. */ \
    vmPopTempRef(vm); /* method. */ \
    vmPopTempRef(vm); /* fn. */ \
  } while (false)

  // TODO: write docs.
  ADD_METHOD(vOBJECT, "typename", _objTypeName, 0);
  ADD_METHOD(vOBJECT, "_repr", _objRepr, 0);
  ADD_METHOD(vOBJECT, "_new", _objNew, -1);

  ADD_METHOD(vOBJECT, "getattr", _objGetattr, -1);
  ADD_METHOD(vOBJECT, "setattr", _objSetattr, -1);
  ADD_METHOD(vOBJECT, "delattr", _objDelattr, -1);

  ADD_METHOD(vNUMBER, "times", _numberTimes, 1);
  ADD_METHOD(vNUMBER, "isint", _numberIsint, 0);
  ADD_METHOD(vNUMBER, "isbyte", _numberIsbyte, 0);

  ADD_METHOD(vSTRING, "strip", _stringStrip, 0);
  ADD_METHOD(vSTRING, "lower", _stringLower, 0);
  ADD_METHOD(vSTRING, "upper", _stringUpper, 0);
  ADD_METHOD(vSTRING, "find", _stringFind, -1);
  ADD_METHOD(vSTRING, "rfind", _stringRFind, -1);
  ADD_METHOD(vSTRING, "replace", _stringReplace, -1);
  ADD_METHOD(vSTRING, "split", _stringSplit, -1);
  ADD_METHOD(vSTRING, "startswith", _stringStartswith, 1);
  ADD_METHOD(vSTRING, "endswith", _stringEndswith, 1);
  ADD_METHOD(vSTRING, "sub", _stringSub, -1);
  ADD_METHOD(vSTRING, "reverse", _stringReverse, 0);
  ADD_METHOD(vSTRING, "rep", _stringRep, 1);
  ADD_METHOD(vSTRING, "byte", _stringByte, 1);
  ADD_METHOD(vSTRING, "format", _stringFormat, -1);
  ADD_METHOD(vSTRING, "match", _stringMatch, -1);
  ADD_METHOD(vSTRING, "gsub", _stringGSub, -1);
  ADD_METHOD(vSTRING, "gmatch", _stringGMatch, 1);

  ADD_METHOD(vLIST, "clear", _listClear, 0);
  ADD_METHOD(vLIST, "find", _listFind, 1);
  ADD_METHOD(vLIST, "append", _listAppend, 1);
  ADD_METHOD(vLIST, "pop", _listPop, -1);
  ADD_METHOD(vLIST, "insert", _listInsert, 2);
  ADD_METHOD(vLIST, "join", _listJoin, -1);
  ADD_METHOD(vLIST, "resize", _listResize, 1);

  ADD_METHOD(vMAP, "clear", _mapClear, 0);
  ADD_METHOD(vMAP, "set", _mapSet, -1);
  ADD_METHOD(vMAP, "get", _mapGet, -1);
  ADD_METHOD(vMAP, "has", _mapHas, 1);
  ADD_METHOD(vMAP, "pop", _mapPop, 1);

  ADD_METHOD(vMETHOD_BIND, "bind", _methodBindBind, 1);

  ADD_METHOD(vCLASS, "methods", _classMethods, 0);

  ADD_METHOD(vMODULE, "define", _moduleDefine, 2);
  ADD_METHOD(vMODULE, "delete", _moduleDelete, 1);

  ADD_METHOD(vFIBER, "run", _fiberRun, -1);
  ADD_METHOD(vFIBER, "resume", _fiberResume, -1);

#undef ADD_METHOD
}