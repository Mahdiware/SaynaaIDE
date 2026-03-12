/*
 * Copyright (c) 2022-2026 Mohamed Abdifatah. All rights reserved.
 * Distributed Under The MIT License
 */

#include "saynaa_optionals.h"

#include "json/saynaa_json.h"
#include <math.h>

// TODO:
// Convert a saynaa_json object into variable. Note that the depth shouldn't
// exceed MAX_TEMP_REFERENCE of the VM and we should throw error if the dept
// is too much (saynaa_json complain at depth 1000), add depth as a parameter.
static Var _cJsonToSaynaa(VM* vm, saynaa_json* item) {
  switch ((item->type) & 0xFF) {
    case SAYNAA_JSON_NULL:
      return VAR_NULL;

    case SAYNAA_JSON_FALSE:
      return VAR_FALSE;

    case SAYNAA_JSON_TRUE:
      return VAR_TRUE;

    case SAYNAA_JSON_NUMBER:
      return VAR_NUM(item->valuedouble);

    case SAYNAA_JSON_STRING:
      {
        const char* s = item->valuestring ? item->valuestring : "";
        return VAR_OBJ(newString(vm, s));
      }

    case SAYNAA_JSON_ARRAY:
      {
        List* list = newList(vm, 8);
        vmPushTempRef(vm, &list->_super); // list.

        saynaa_json* elem = item->child;
        while (elem != NULL) {
          Var v = _cJsonToSaynaa(vm, elem);
          if (IS_OBJ(v))
            vmPushTempRef(vm, AS_OBJ(v)); // v.
          listAppend(vm, list, v);
          if (IS_OBJ(v))
            vmPopTempRef(vm); // v.
          elem = elem->next;
        }
        vmPopTempRef(vm); // list.
        return VAR_OBJ(list);
      }

    case SAYNAA_JSON_OBJECT:
      {
        Map* map = newMap(vm);
        vmPushTempRef(vm, &map->_super); // map.
        saynaa_json* elem = item->child;
        while (elem != NULL) {
          String* key = newString(vm, elem->key);
          vmPushTempRef(vm, &key->_super); // key.
          {
            Var value = _cJsonToSaynaa(vm, elem);
            if (IS_OBJ(value))
              vmPushTempRef(vm, AS_OBJ(value)); // value.
            mapSet(vm, map, VAR_OBJ(key), value);
            if (IS_OBJ(value))
              vmPopTempRef(vm); // value.
          }
          vmPopTempRef(vm); // key.
          elem = elem->next;
        }
        vmPopTempRef(vm); // map.
        return VAR_OBJ(map);
      }

    default:
      UNREACHABLE();
  }

  UNREACHABLE();
}

static saynaa_json* _saynaaToCJson(VM* vm, Var item) {
  VarType vt = getVarType(item);
  switch (vt) {
    case vNULL:
      return saynaa_json_create_null();

    case vBOOL:
      return saynaa_json_create_bool(AS_BOOL(item));

    case vNUMBER:
      return saynaa_json_create_number(AS_NUM(item));

    case vSTRING:
      return saynaa_json_create_string(((String*) AS_OBJ(item))->data);

    case vLIST:
      {
        List* list = (List*) AS_OBJ(item);
        saynaa_json* arr = saynaa_json_create_array();

        bool err = false;
        for (uint32_t i = 0; i < list->elements.count; i++) {
          saynaa_json* elem = _saynaaToCJson(vm, list->elements.data[i]);
          if (elem == NULL) {
            err = true;
            break;
          };
          saynaa_json_add_item_to_array(arr, elem);
        }

        if (err) {
          saynaa_json_delete(arr);
          return NULL;
        }

        return arr;
      }

    case vMAP:
      {
        Map* map = (Map*) AS_OBJ(item);
        saynaa_json* obj = saynaa_json_create_object();

        bool err = false;
        MapEntry* e = map->entries;
        for (; e < map->entries + map->capacity; e++) {
          if (IS_UNDEF(e->key))
            continue;

          if (!IS_OBJ_TYPE(e->key, OBJ_STRING)) {
            SetRuntimeErrorFmt(vm,
                               "Expected string as json object key, "
                               "instead got type '%s'.",
                               varTypeName(e->key));
            err = true;
            break;
          }

          saynaa_json* value = _saynaaToCJson(vm, e->value);
          if (value == NULL) {
            err = true;
            break;
          }

          saynaa_json_add_item_to_object(obj, ((String*) AS_OBJ(e->key))->data, value);
        }

        if (err) {
          saynaa_json_delete(obj);
          return NULL;
        }

        return obj;
      }

    default:
      {
        SetRuntimeErrorFmt(vm,
                           "Object of type '%s' cannot be serialized "
                           "to json.",
                           varTypeName(item));
        return NULL;
      }
  }

  UNREACHABLE();
  return NULL;
}

saynaa_function(_jsonParse, "json.parse(json_str:String) -> Var",
                "Parse a json string into language object.") {
  const char* string;
  if (!ValidateSlotString(vm, 1, &string, NULL))
    return;

  saynaa_json* tree = saynaa_json_parse(string);

  if (tree == NULL) {
    // TODO: Print the position.
    // const char* position = cJSON_GetErrorPtr();
    // if (position != NULL) ...
    SetRuntimeError(vm, "Invalid json string");
    return;
  }

  Var obj = _cJsonToSaynaa(vm, tree);
  saynaa_json_delete(tree);

  // Json is a standard libray "std_json" and has the direct access
  // to the vm's internal stack.
  vm->fiber->ret[0] = obj;
}

saynaa_function(
    _jsonPrint, "json.print(value:Var, pretty:Bool=false)",
    "Render a value into text. Takes an optional argument pretty, if "
    "true it'll pretty print the output.") {
  int argc = GetArgc(vm);
  if (!CheckArgcRange(vm, argc, 1, 2))
    return;

  bool pretty = false;
  if (argc == 2) {
    if (!ValidateSlotBool(vm, 2, &pretty))
      return;
  }

  Var value = vm->fiber->ret[1];
  saynaa_json* json = _saynaaToCJson(vm, value);

  // A runtime error already set.
  if (json == NULL)
    return;

  char* string = (pretty) ? saynaa_json_print(json) : saynaa_json_print_unformatted(json);
  saynaa_json_delete(json);

  if (string == NULL) {
    SetRuntimeError(vm, "Failed to print json.");
    return;
  }

  setSlotString(vm, 0, string);
  free(string);
}

/*****************************************************************************/
/* MODULE REGISTER                                                           */
/*****************************************************************************/

void registerModuleJson(VM* vm) {
  Handle* json = NewModule(vm, "json");

  REGISTER_FN(json, "parse", _jsonParse, 1);
  REGISTER_FN(json, "print", _jsonPrint, -1);

  registerModule(vm, json);
  releaseHandle(vm, json);
}