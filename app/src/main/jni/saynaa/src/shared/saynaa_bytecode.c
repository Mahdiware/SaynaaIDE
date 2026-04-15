/*
 * Copyright (c) 2022-2026 Mohamed Abdifatah. All rights reserved.
 * Distributed Under The MIT License
 */

#include "saynaa_bytecode.h"

#include "../runtime/saynaa_core.h"
#include "../runtime/saynaa_vm.h"
#include "../utils/saynaa_utils.h"

#include <stdio.h>

static const uint8_t kSaynaaMagic[SAYNAA_BYTECODE_MAGIC_SIZE] = {
  'S', 'A', 'Y', 'N', 'A', 'A', '1', 0
};

void saynaa_bytecode_init(SaynaaBytecode* bytecode) {
  if (bytecode == NULL)
    return;
  bytecode->data = NULL;
  bytecode->size = 0;
  bytecode->flags = 0;
  bytecode->checksum = 0;
  bytecode->timestamp = 0;
}

void saynaa_bytecode_clear(VM* vm, SaynaaBytecode* bytecode) {
  if (bytecode == NULL)
    return;
  if (bytecode->data != NULL) {
    Realloc(vm, bytecode->data, 0);
  }
  saynaa_bytecode_init(bytecode);
}

Result saynaa_bytecode_set_payload(VM* vm, SaynaaBytecode* bytecode,
                                   const uint8_t* payload,
                                   size_t payload_size,
                                   uint8_t flags,
                                   uint64_t timestamp) {
  if (bytecode == NULL || payload == NULL || payload_size == 0)
    return RESULT_BYTECODE_INVALID_ARGUMENT;

  saynaa_bytecode_clear(vm, bytecode);
  bytecode->data = (uint8_t*) Realloc(vm, NULL, payload_size);
  if (bytecode->data == NULL)
    return RESULT_BYTECODE_INVALID_ARGUMENT;
  memcpy(bytecode->data, payload, payload_size);
  bytecode->size = payload_size;
  bytecode->flags = flags;
  bytecode->timestamp = timestamp;
  bytecode->checksum = saynaa_bytecode_crc32(payload, payload_size);
  return RESULT_SUCCESS;
}

Result saynaa_bytecode_save(const SaynaaBytecode* bytecode,
                            const char* path) {
  if (bytecode == NULL || path == NULL || bytecode->data == NULL)
    return RESULT_BYTECODE_INVALID_ARGUMENT;
  return saynaa_bytecode_write_file(path, bytecode->data, bytecode->size,
                                    bytecode->flags, bytecode->timestamp);
}

Result saynaa_bytecode_run(VM* vm, const SaynaaBytecode* bytecode) {
  if (vm == NULL || bytecode == NULL || bytecode->data == NULL
      || bytecode->size == 0) {
    return RESULT_RUNTIME_ERROR;
  }

  Module* module = newModule(vm);
  vmPushTempRef(vm, &module->_super); // module.

  module->path = newString(vm, "@(Bytecode)");
  Result status = saynaa_bytecode_deserialize_module(
      vm, module, bytecode->data, bytecode->size);
  if (status != RESULT_SUCCESS) {
    if (!VM_HAS_ERROR(vm)) {
      VM_SET_ERROR(vm,
                   stringFormat(vm, "Bytecode deserialize failed: $",
                                saynaa_status_message(status), false));
    }
    vmPopTempRef(vm); // module.
    return RESULT_COMPILE_ERROR;
  }

  initializeModule(vm, module, true);
  vmRegisterModule(vm, module, module->path);
  module->initialized = true;

  Result result = vmCallFunction(vm, module->body, 0, NULL, NULL);
  vmPopTempRef(vm); // module.
  return result;
}

/*****************************************************************************/
/* PATH HELPERS                                                              */
/*****************************************************************************/

static bool has_suffix(const char* value, const char* suffix) {
  size_t value_len = strlen(value);
  size_t suffix_len = strlen(suffix);
  if (suffix_len > value_len)
    return false;
  return strcmp(value + (value_len - suffix_len), suffix) == 0;
}

char* saynaa_bytecode_build_path(VM* vm, const char* input_path) {
  if (vm == NULL || input_path == NULL)
    return NULL;

  size_t input_len = strlen(input_path);
  size_t ext_len = strlen(SAYNAA_FILE_EXT);
  size_t out_len = 0;
  bool has_sa_ext = has_suffix(input_path, SAYNAA_FILE_EXT) && input_len > ext_len;

  if (has_sa_ext) {
    out_len = input_len - ext_len + strlen(SAYNAA_BYTECODE_EXT);
  } else {
    out_len = input_len + strlen(SAYNAA_BYTECODE_EXT);
  }

  char* out_path = (char*) Realloc(vm, NULL, out_len + 1);
  if (out_path == NULL)
    return NULL;

  if (has_sa_ext) {
    memcpy(out_path, input_path, input_len - ext_len);
    memcpy(out_path + (input_len - ext_len), SAYNAA_BYTECODE_EXT,
           strlen(SAYNAA_BYTECODE_EXT) + 1);
  } else {
    memcpy(out_path, input_path, input_len);
    memcpy(out_path + input_len, SAYNAA_BYTECODE_EXT,
           strlen(SAYNAA_BYTECODE_EXT) + 1);
  }

  return out_path;
}

/*****************************************************************************/
/* HEADER IO                                                                 */
/*****************************************************************************/

static void write_u32_le(uint8_t* out, uint32_t value) {
  out[0] = (uint8_t) (value & 0xff);
  out[1] = (uint8_t) ((value >> 8) & 0xff);
  out[2] = (uint8_t) ((value >> 16) & 0xff);
  out[3] = (uint8_t) ((value >> 24) & 0xff);
}

static void write_u64_le(uint8_t* out, uint64_t value) {
  out[0] = (uint8_t) (value & 0xff);
  out[1] = (uint8_t) ((value >> 8) & 0xff);
  out[2] = (uint8_t) ((value >> 16) & 0xff);
  out[3] = (uint8_t) ((value >> 24) & 0xff);
  out[4] = (uint8_t) ((value >> 32) & 0xff);
  out[5] = (uint8_t) ((value >> 40) & 0xff);
  out[6] = (uint8_t) ((value >> 48) & 0xff);
  out[7] = (uint8_t) ((value >> 56) & 0xff);
}

static uint32_t read_u32_le(const uint8_t* data) {
  return (uint32_t) data[0]
         | ((uint32_t) data[1] << 8)
         | ((uint32_t) data[2] << 16)
         | ((uint32_t) data[3] << 24);
}

static uint64_t read_u64_le(const uint8_t* data) {
  return (uint64_t) data[0]
         | ((uint64_t) data[1] << 8)
         | ((uint64_t) data[2] << 16)
         | ((uint64_t) data[3] << 24)
         | ((uint64_t) data[4] << 32)
         | ((uint64_t) data[5] << 40)
         | ((uint64_t) data[6] << 48)
         | ((uint64_t) data[7] << 56);
}

void saynaa_bytecode_init_header(SaynaaBytecodeHeader* header, uint8_t flags,
                                 uint32_t bytecode_size, uint32_t checksum,
                                 uint64_t timestamp) {
  if (header == NULL)
    return;

  memcpy(header->magic, kSaynaaMagic, SAYNAA_BYTECODE_MAGIC_SIZE);
  header->version_major = (uint8_t) VERSION_MAJOR;
  header->version_minor = (uint8_t) VERSION_MINOR;
  header->version_patch = (uint8_t) VERSION_PATCH;
  header->flags = flags;
  header->bytecode_size = bytecode_size;
  header->checksum = checksum;
  header->timestamp = timestamp;
}

Result saynaa_bytecode_encode_header(const SaynaaBytecodeHeader* header,
                                     uint8_t* out, size_t out_size) {
  if (header == NULL || out == NULL)
    return RESULT_BYTECODE_INVALID_ARGUMENT;
  if (out_size < SAYNAA_BYTECODE_HEADER_SIZE)
    return RESULT_BYTECODE_INCOMPLETE_HEADER;

  memcpy(out, header->magic, SAYNAA_BYTECODE_MAGIC_SIZE);
  out[8] = header->version_major;
  out[9] = header->version_minor;
  out[10] = header->version_patch;
  out[11] = header->flags;
  write_u32_le(out + 12, header->bytecode_size);
  write_u32_le(out + 16, header->checksum);
  write_u64_le(out + 20, header->timestamp);

  return RESULT_SUCCESS;
}

Result saynaa_bytecode_decode_header(const uint8_t* data, size_t data_size,
                                     SaynaaBytecodeHeader* out) {
  if (data == NULL || out == NULL)
    return RESULT_BYTECODE_INVALID_ARGUMENT;
  if (data_size < SAYNAA_BYTECODE_HEADER_SIZE)
    return RESULT_BYTECODE_INCOMPLETE_HEADER;

  memcpy(out->magic, data, SAYNAA_BYTECODE_MAGIC_SIZE);
  out->version_major = data[8];
  out->version_minor = data[9];
  out->version_patch = data[10];
  out->flags = data[11];
  out->bytecode_size = read_u32_le(data + 12);
  out->checksum = read_u32_le(data + 16);
  out->timestamp = read_u64_le(data + 20);

  return RESULT_SUCCESS;
}

Result saynaa_bytecode_validate_header(const SaynaaBytecodeHeader* header,
                                       size_t total_size) {
  if (header == NULL)
    return RESULT_BYTECODE_INVALID_ARGUMENT;

  if (memcmp(header->magic, kSaynaaMagic, SAYNAA_BYTECODE_MAGIC_SIZE) != 0)
    return RESULT_BYTECODE_INVALID_MAGIC;

  if (header->version_major != (uint8_t) VERSION_MAJOR)
    return RESULT_BYTECODE_VERSION_MISMATCH;

  if (total_size > 0) {
    if (total_size < SAYNAA_BYTECODE_HEADER_SIZE)
      return RESULT_BYTECODE_INCOMPLETE_HEADER;
    if (header->bytecode_size > total_size - SAYNAA_BYTECODE_HEADER_SIZE)
      return RESULT_BYTECODE_SIZE_MISMATCH;
  }

  return RESULT_SUCCESS;
}

uint32_t saynaa_bytecode_crc32(const uint8_t* data, size_t size) {
  static uint32_t table[256];
  static bool table_ready = false;

  if (!table_ready) {
    for (uint32_t i = 0; i < 256; i++) {
      uint32_t c = i;
      for (int j = 0; j < 8; j++) {
        if (c & 1)
          c = 0xedb88320u ^ (c >> 1);
        else
          c = c >> 1;
      }
      table[i] = c;
    }
    table_ready = true;
  }

  uint32_t crc = 0xffffffffu;
  for (size_t i = 0; i < size; i++) {
    uint8_t byte = data[i];
    crc = table[(crc ^ byte) & 0xff] ^ (crc >> 8);
  }

  return crc ^ 0xffffffffu;
}

Result saynaa_bytecode_validate_checksum(const SaynaaBytecodeHeader* header,
                                         const uint8_t* bytecode,
                                         size_t bytecode_size) {
  if (header == NULL || bytecode == NULL)
    return RESULT_BYTECODE_INVALID_ARGUMENT;

  if (bytecode_size != header->bytecode_size)
    return RESULT_BYTECODE_SIZE_MISMATCH;

  uint32_t computed = saynaa_bytecode_crc32(bytecode, bytecode_size);
  if (computed != header->checksum)
    return RESULT_BYTECODE_CHECKSUM_MISMATCH;

  return RESULT_SUCCESS;
}

Result saynaa_bytecode_validate_payload(const uint8_t* payload,
                                        size_t payload_size) {
  if (payload == NULL)
    return RESULT_BYTECODE_INVALID_ARGUMENT;
  if (payload_size < SAYNAA_BYTECODE_PAYLOAD_MAGIC_SIZE + 1)
    return RESULT_BYTECODE_TRUNCATED;
  if (memcmp(payload, SAYNAA_BYTECODE_PAYLOAD_MAGIC,
             SAYNAA_BYTECODE_PAYLOAD_MAGIC_SIZE) != 0) {
    return RESULT_BYTECODE_INVALID_FORMAT;
  }
  uint8_t version = payload[SAYNAA_BYTECODE_PAYLOAD_MAGIC_SIZE];
  if (version < SAYNAA_BYTECODE_PAYLOAD_MIN_VERSION
      || version > SAYNAA_BYTECODE_PAYLOAD_VERSION) {
    return RESULT_BYTECODE_VERSION_MISMATCH;
  }
  return RESULT_SUCCESS;
}

Result saynaa_bytecode_write_file(const char* path,
                                  const uint8_t* bytecode,
                                  size_t bytecode_size,
                                  uint8_t flags,
                                  uint64_t timestamp) {
  if (path == NULL || bytecode == NULL)
    return RESULT_BYTECODE_INVALID_ARGUMENT;

  uint32_t checksum = saynaa_bytecode_crc32(bytecode, bytecode_size);

  SaynaaBytecodeHeader header;
  saynaa_bytecode_init_header(&header, flags, (uint32_t) bytecode_size, checksum,
                              timestamp);

  uint8_t header_buf[SAYNAA_BYTECODE_HEADER_SIZE];
  Result status = saynaa_bytecode_encode_header(&header, header_buf, sizeof(header_buf));
  if (status != RESULT_SUCCESS)
    return status;

  FILE* file = fopen(path, "wb");
  if (file == NULL)
    return RESULT_BYTECODE_IO_ERROR;

  size_t written = fwrite(header_buf, 1, sizeof(header_buf), file);
  if (written != sizeof(header_buf)) {
    fclose(file);
    return RESULT_BYTECODE_IO_ERROR;
  }

  if (bytecode_size > 0) {
    written = fwrite(bytecode, 1, bytecode_size, file);
    if (written != bytecode_size) {
      fclose(file);
      return RESULT_BYTECODE_IO_ERROR;
    }
  }

  fclose(file);
  return RESULT_SUCCESS;
}

/*****************************************************************************/
/* PAYLOAD IO                                                                */
/*****************************************************************************/

typedef enum {
  SAYNAA_BC_CONST_NULL = 0,
  SAYNAA_BC_CONST_BOOL = 1,
  SAYNAA_BC_CONST_NUMBER = 2,
  SAYNAA_BC_CONST_STRING = 3,
  SAYNAA_BC_CONST_FUNCTION = 4,
  SAYNAA_BC_CONST_CLASS = 5,
} SaynaaBytecodeConstTag;

static void bc_write_u8(ByteBuffer* out, VM* vm, uint8_t value) {
  ByteBufferWrite(out, vm, value);
}

#define SAYNAA_BC_VARINT_MAX_BYTES 10

// MSB-varint (same direction as Lua's dump/load): each byte stores 7 bits,
// and all but the last byte have the continuation flag set.
static void bc_write_varu(ByteBuffer* out, VM* vm, uint64_t value) {
  uint8_t buff[SAYNAA_BC_VARINT_MAX_BYTES];
  uint32_t n = 1;

  buff[SAYNAA_BC_VARINT_MAX_BYTES - 1] = (uint8_t) (value & 0x7fu);
  while ((value >>= 7) != 0) {
    buff[SAYNAA_BC_VARINT_MAX_BYTES - (++n)] =
        (uint8_t) ((value & 0x7fu) | 0x80u);
  }

  ByteBufferAddString(out, vm,
                      (const char*) (buff + SAYNAA_BC_VARINT_MAX_BYTES - n),
                      n);
}

static void bc_write_vari32(ByteBuffer* out, VM* vm, int32_t value) {
  // ZigZag encoding keeps small negative/positive values compact.
  uint32_t uv = ((uint32_t) value << 1) ^ (uint32_t) (value >> 31);
  bc_write_varu(out, vm, uv);
}

static void bc_write_double(ByteBuffer* out, VM* vm, double value) {
  uint64_t bits = utilDoubleToBits(value);
  uint8_t bytes[8];
  write_u64_le(bytes, bits);
  ByteBufferAddString(out, vm, (const char*) bytes, 8);
}

typedef struct {
  const uint8_t* data;
  size_t size;
  size_t offset;
} BytecodeReader;

static bool bc_read_bytes(BytecodeReader* reader, uint8_t* out, size_t length) {
  if (reader->offset + length > reader->size)
    return false;
  if (out != NULL) {
    memcpy(out, reader->data + reader->offset, length);
  }
  reader->offset += length;
  return true;
}

static bool bc_read_u8(BytecodeReader* reader, uint8_t* out) {
  return bc_read_bytes(reader, out, 1);
}

static Result bc_read_varu(BytecodeReader* reader, uint64_t limit, uint64_t* out) {
  uint64_t value = 0;
  uint64_t shifted_limit = limit >> 7;

  uint8_t b = 0;
  do {
    if (!bc_read_u8(reader, &b))
      return RESULT_BYTECODE_TRUNCATED;

    if (value > shifted_limit)
      return RESULT_BYTECODE_INVALID_FORMAT;

    value = (value << 7) | (uint64_t) (b & 0x7fu);
  } while ((b & 0x80u) != 0);

  if (value > limit)
    return RESULT_BYTECODE_INVALID_FORMAT;

  *out = value;
  return RESULT_SUCCESS;
}

static Result bc_read_vari32(BytecodeReader* reader, int32_t* out) {
  uint64_t raw = 0;
  Result status = bc_read_varu(reader, UINT32_MAX, &raw);
  if (status != RESULT_SUCCESS)
    return status;

  uint32_t uv = (uint32_t) raw;
  uint32_t decoded = (uv >> 1) ^ (0u - (uv & 1u));
  *out = (int32_t) decoded;
  return RESULT_SUCCESS;
}

static bool bc_read_u32(BytecodeReader* reader, uint32_t* out) {
  uint8_t bytes[4];
  if (!bc_read_bytes(reader, bytes, sizeof(bytes)))
    return false;
  *out = read_u32_le(bytes);
  return true;
}

static bool bc_read_i32(BytecodeReader* reader, int32_t* out) {
  uint32_t value = 0;
  if (!bc_read_u32(reader, &value))
    return false;
  *out = (int32_t) value;
  return true;
}

static bool bc_read_double(BytecodeReader* reader, double* out) {
  uint8_t bytes[8];
  if (!bc_read_bytes(reader, bytes, sizeof(bytes)))
    return false;
  uint64_t bits = read_u64_le(bytes);
  *out = utilDoubleFromBits(bits);
  return true;
}

static int find_constant_index(Module* module, Var value) {
  for (uint32_t i = 0; i < module->constants.count; i++) {
    if (isValuesSame(module->constants.data[i], value))
      return (int) i;
  }
  return -1;
}

static void bc_write_string_nullable(ByteBuffer* out, VM* vm,
                                     const char* text, uint32_t length) {
  if (text == NULL) {
    bc_write_varu(out, vm, 0);
    return;
  }

  // Store length as (len + 1) so zero is reserved for NULL.
  bc_write_varu(out, vm, (uint64_t) length + 1u);
  if (length > 0) {
    ByteBufferAddString(out, vm, text, length);
  }
}

static void bc_write_string_obj_nullable(ByteBuffer* out, VM* vm, String* value) {
  if (value == NULL) {
    bc_write_varu(out, vm, 0);
    return;
  }
  bc_write_string_nullable(out, vm, value->data, value->length);
}

Result saynaa_bytecode_serialize_module(VM* vm, Module* module,
                                        ByteBuffer* out) {
  if (vm == NULL || module == NULL || out == NULL)
    return RESULT_BYTECODE_INVALID_ARGUMENT;
  if (module->body == NULL || module->body->fn == NULL)
    return RESULT_BYTECODE_INVALID_ARGUMENT;

  ByteBufferAddString(out, vm, SAYNAA_BYTECODE_PAYLOAD_MAGIC,
                      SAYNAA_BYTECODE_PAYLOAD_MAGIC_SIZE);
  bc_write_u8(out, vm, SAYNAA_BYTECODE_PAYLOAD_VERSION);

  bc_write_varu(out, vm, module->constants.count);

  for (uint32_t i = 0; i < module->constants.count; i++) {
    Var constant = module->constants.data[i];

    if (IS_NULL(constant)) {
      bc_write_u8(out, vm, SAYNAA_BC_CONST_NULL);
      continue;
    }

    if (IS_BOOL(constant)) {
      bc_write_u8(out, vm, SAYNAA_BC_CONST_BOOL);
      bc_write_u8(out, vm, AS_BOOL(constant) ? 1 : 0);
      continue;
    }

    if (IS_NUM(constant)) {
      bc_write_u8(out, vm, SAYNAA_BC_CONST_NUMBER);
      bc_write_double(out, vm, AS_NUM(constant));
      continue;
    }

    if (!IS_OBJ(constant))
      return RESULT_BYTECODE_UNSUPPORTED_CONST;

    Object* obj = AS_OBJ(constant);
    switch (obj->type) {
      case OBJ_STRING:
        {
          String* str = (String*) obj;
          bc_write_u8(out, vm, SAYNAA_BC_CONST_STRING);
          bc_write_varu(out, vm, str->length);
          if (str->length > 0) {
            ByteBufferAddString(out, vm, str->data, str->length);
          }
        }
        break;

      case OBJ_FUNC:
        {
          Function* fn = (Function*) obj;
          if (fn->is_native || fn->fn == NULL)
            return RESULT_BYTECODE_UNSUPPORTED_CONST;

          if (fn->name == NULL)
            return RESULT_BYTECODE_INVALID_FORMAT;
          if (fn->upvalue_count < 0)
            return RESULT_BYTECODE_INVALID_FORMAT;

          bc_write_u8(out, vm, SAYNAA_BC_CONST_FUNCTION);
          bc_write_string_nullable(out, vm, fn->name,
                                   (uint32_t) strlen(fn->name));
          if (fn->docstring != NULL) {
            bc_write_string_nullable(out, vm, fn->docstring,
                                     (uint32_t) strlen(fn->docstring));
          } else {
            bc_write_varu(out, vm, 0);
          }
          bc_write_vari32(out, vm, fn->arity);
          bc_write_u8(out, vm, fn->is_method ? 1 : 0);
          bc_write_varu(out, vm, (uint64_t) fn->upvalue_count);
          bc_write_vari32(out, vm, fn->fn->stack_size);

          bc_write_varu(out, vm, fn->fn->opcodes.count);
          if (fn->fn->opcodes.count > 0) {
            ByteBufferAddString(out, vm, (const char*) fn->fn->opcodes.data,
                                fn->fn->opcodes.count);
          }

          bc_write_varu(out, vm, fn->fn->oplines.count);
          for (uint32_t j = 0; j < fn->fn->oplines.count; j++) {
            bc_write_varu(out, vm, fn->fn->oplines.data[j]);
          }
        }
        break;

      case OBJ_CLASS:
        {
          Class* cls = (Class*) obj;
          if (cls->name == NULL)
            return RESULT_BYTECODE_INVALID_FORMAT;

          bc_write_u8(out, vm, SAYNAA_BC_CONST_CLASS);
          bc_write_string_obj_nullable(out, vm, cls->name);
          if (cls->docstring != NULL) {
            bc_write_string_nullable(out, vm, cls->docstring,
                                     (uint32_t) strlen(cls->docstring));
          } else {
            bc_write_varu(out, vm, 0);
          }
          bc_write_varu(out, vm, (uint64_t) cls->class_of);
        }
        break;

      default:
        return RESULT_BYTECODE_UNSUPPORTED_CONST;
    }
  }

  int body_index = find_constant_index(module, VAR_OBJ(module->body->fn));
  if (body_index < 0)
    return RESULT_BYTECODE_INVALID_FORMAT;
  bc_write_varu(out, vm, (uint32_t) body_index);

  return RESULT_SUCCESS;
}

typedef struct {
  uint32_t index;
  uint32_t name_index;
  uint32_t doc_index;
  int32_t arity;
  uint8_t is_method;
  uint32_t upvalue_count;
  int32_t stack_size;
  uint8_t* opcodes;
  uint32_t opcodes_count;
  uint32_t* oplines;
  uint32_t oplines_count;
} PendingFunction;

typedef struct {
  uint32_t index;
  uint32_t name_index;
  uint32_t doc_index;
  uint8_t class_of;
} PendingClass;

static void free_pending_functions(VM* vm, PendingFunction* fns, uint32_t count) {
  if (fns == NULL)
    return;

  for (uint32_t i = 0; i < count; i++) {
    PendingFunction* pending = &fns[i];
    if (pending->opcodes != NULL) {
      vmRealloc(vm, pending->opcodes, pending->opcodes_count, 0);
      pending->opcodes = NULL;
    }
    if (pending->oplines != NULL) {
      size_t size = pending->oplines_count * sizeof(uint32_t);
      vmRealloc(vm, pending->oplines, size, 0);
      pending->oplines = NULL;
    }
  }
}

static Result bc_read_module_string(BytecodeReader* reader, VM* vm,
                                    bool nullable, String** out) {
  uint64_t encoded_len = 0;
  Result status = bc_read_varu(reader, (uint64_t) UINT32_MAX + 1u, &encoded_len);
  if (status != RESULT_SUCCESS)
    return status;

  if (encoded_len == 0) {
    if (!nullable)
      return RESULT_BYTECODE_INVALID_FORMAT;
    *out = NULL;
    return RESULT_SUCCESS;
  }

  uint64_t len64 = encoded_len - 1u;
  if (len64 > reader->size - reader->offset)
    return RESULT_BYTECODE_TRUNCATED;

  uint32_t len = (uint32_t) len64;
  *out = newInternedStringLength(vm,
                                 (const char*) (reader->data + reader->offset),
                                 len);
  reader->offset += len;
  return RESULT_SUCCESS;
}

static Result saynaa_bytecode_deserialize_module_v3(VM* vm, Module* module,
                                                    const uint8_t* data,
                                                    size_t data_size) {
  if (vm == NULL || module == NULL || data == NULL)
    return RESULT_BYTECODE_INVALID_ARGUMENT;

  BytecodeReader reader;
  reader.data = data;
  reader.size = data_size;
  reader.offset = 0;

  uint8_t magic[SAYNAA_BYTECODE_PAYLOAD_MAGIC_SIZE];
  if (!bc_read_bytes(&reader, magic, sizeof(magic)))
    return RESULT_BYTECODE_TRUNCATED;
  if (memcmp(magic, SAYNAA_BYTECODE_PAYLOAD_MAGIC, sizeof(magic)) != 0)
    return RESULT_BYTECODE_INVALID_FORMAT;

  uint8_t version = 0;
  if (!bc_read_u8(&reader, &version))
    return RESULT_BYTECODE_TRUNCATED;
  if (version != SAYNAA_BYTECODE_PAYLOAD_VERSION)
    return RESULT_BYTECODE_VERSION_MISMATCH;

  uint64_t constants_count64 = 0;
  Result status = bc_read_varu(&reader, MAX_CONSTANTS, &constants_count64);
  if (status != RESULT_SUCCESS)
    return status;
  uint32_t constants_count = (uint32_t) constants_count64;

  if (constants_count > 0) {
    VarBufferFill(&module->constants, vm, VAR_NULL, (int) constants_count);
  }

  for (uint32_t i = 0; i < constants_count; i++) {
    uint8_t tag = 0;
    if (!bc_read_u8(&reader, &tag))
      return RESULT_BYTECODE_TRUNCATED;

    switch ((SaynaaBytecodeConstTag) tag) {
      case SAYNAA_BC_CONST_NULL:
        module->constants.data[i] = VAR_NULL;
        break;

      case SAYNAA_BC_CONST_BOOL:
        {
          uint8_t value = 0;
          if (!bc_read_u8(&reader, &value))
            return RESULT_BYTECODE_TRUNCATED;
          module->constants.data[i] = VAR_BOOL(value != 0);
        }
        break;

      case SAYNAA_BC_CONST_NUMBER:
        {
          double value = 0;
          if (!bc_read_double(&reader, &value))
            return RESULT_BYTECODE_TRUNCATED;
          module->constants.data[i] = VAR_NUM(value);
        }
        break;

      case SAYNAA_BC_CONST_STRING:
        {
          uint64_t length64 = 0;
          status = bc_read_varu(&reader, UINT32_MAX, &length64);
          if (status != RESULT_SUCCESS)
            return status;

          if (length64 > reader.size - reader.offset)
            return RESULT_BYTECODE_TRUNCATED;

          uint32_t length = (uint32_t) length64;
          String* str = newInternedStringLength(vm,
                                                (const char*) (reader.data + reader.offset),
                                                length);
          vmPushTempRef(vm, &str->_super); // str.
          module->constants.data[i] = VAR_OBJ(str);
          vmPopTempRef(vm); // str.

          reader.offset += length;
        }
        break;

      case SAYNAA_BC_CONST_FUNCTION:
        {
          String* name = NULL;
          String* doc = NULL;

          status = bc_read_module_string(&reader, vm, false, &name);
          if (status != RESULT_SUCCESS)
            return status;

          status = bc_read_module_string(&reader, vm, true, &doc);
          if (status != RESULT_SUCCESS)
            return status;

          int32_t arity = 0;
          status = bc_read_vari32(&reader, &arity);
          if (status != RESULT_SUCCESS)
            return status;

          uint8_t is_method = 0;
          if (!bc_read_u8(&reader, &is_method))
            return RESULT_BYTECODE_TRUNCATED;
          if (is_method != 0 && is_method != 1)
            return RESULT_BYTECODE_INVALID_FORMAT;

          uint64_t upvalue_count64 = 0;
          status = bc_read_varu(&reader, MAX_UPVALUES, &upvalue_count64);
          if (status != RESULT_SUCCESS)
            return status;

          int32_t stack_size = 0;
          status = bc_read_vari32(&reader, &stack_size);
          if (status != RESULT_SUCCESS)
            return status;
          if (stack_size < 0
              || (size_t) stack_size >= (MAX_STACK_SIZE / sizeof(Var))) {
            return RESULT_BYTECODE_INVALID_FORMAT;
          }

          uint64_t opcodes_count64 = 0;
          status = bc_read_varu(&reader, UINT32_MAX, &opcodes_count64);
          if (status != RESULT_SUCCESS)
            return status;
          if (opcodes_count64 > reader.size - reader.offset)
            return RESULT_BYTECODE_TRUNCATED;

          uint32_t opcodes_count = (uint32_t) opcodes_count64;
          uint32_t oplines_count = 0;
          Function* fn = newFunctionRaw(vm, module, name, doc,
                                        arity, is_method != 0,
                                        (int) upvalue_count64);
          vmPushTempRef(vm, &fn->_super); // fn.

          fn->fn->stack_size = stack_size;

          if (opcodes_count > 0) {
            ByteBufferReserve(&fn->fn->opcodes, vm, opcodes_count);
            memcpy(fn->fn->opcodes.data, reader.data + reader.offset,
                   opcodes_count);
            fn->fn->opcodes.count = opcodes_count;
            reader.offset += opcodes_count;
          }

          uint64_t oplines_count64 = 0;
          status = bc_read_varu(&reader, UINT32_MAX, &oplines_count64);
          if (status != RESULT_SUCCESS) {
            vmPopTempRef(vm); // fn.
            return status;
          }
          if (oplines_count64 > opcodes_count64 + 1u) {
            vmPopTempRef(vm); // fn.
            return RESULT_BYTECODE_INVALID_FORMAT;
          }
          oplines_count = (uint32_t) oplines_count64;

          if (oplines_count > 0) {
            UintBufferReserve(&fn->fn->oplines, vm, oplines_count);
            for (uint32_t j = 0; j < oplines_count; j++) {
              uint64_t line64 = 0;
              status = bc_read_varu(&reader, UINT32_MAX, &line64);
              if (status != RESULT_SUCCESS) {
                vmPopTempRef(vm); // fn.
                return status;
              }
              fn->fn->oplines.data[j] = (uint32_t) line64;
            }
            fn->fn->oplines.count = oplines_count;
          }

          module->constants.data[i] = VAR_OBJ(fn);
          vmPopTempRef(vm); // fn.
        }
        break;

      case SAYNAA_BC_CONST_CLASS:
        {
          String* name = NULL;
          String* doc = NULL;

          status = bc_read_module_string(&reader, vm, false, &name);
          if (status != RESULT_SUCCESS)
            return status;

          status = bc_read_module_string(&reader, vm, true, &doc);
          if (status != RESULT_SUCCESS)
            return status;

          uint64_t class_of64 = 0;
          status = bc_read_varu(&reader, UINT8_MAX, &class_of64);
          if (status != RESULT_SUCCESS)
            return status;

          Class* cls = newClassRaw(vm, module, name, doc);
          vmPushTempRef(vm, &cls->_super); // cls.
          cls->class_of = (VarType) (uint8_t) class_of64;
          module->constants.data[i] = VAR_OBJ(cls);
          vmPopTempRef(vm); // cls.
        }
        break;

      default:
        return RESULT_BYTECODE_UNSUPPORTED_CONST;
    }
  }

  uint64_t body_index64 = 0;
  status = bc_read_varu(&reader, UINT32_MAX, &body_index64);
  if (status != RESULT_SUCCESS)
    return status;

  if (body_index64 >= constants_count64)
    return RESULT_BYTECODE_INVALID_FORMAT;

  Var body_fn = module->constants.data[(uint32_t) body_index64];
  if (!IS_OBJ_TYPE(body_fn, OBJ_FUNC))
    return RESULT_BYTECODE_INVALID_FORMAT;

  Closure* body = newClosure(vm, (Function*) AS_OBJ(body_fn));
  vmPushTempRef(vm, &body->_super); // body.
  module->body = body;
  vmPopTempRef(vm); // body.

  moduleSetGlobal(vm, module, IMPLICIT_MAIN_NAME,
                  (uint32_t) strlen(IMPLICIT_MAIN_NAME), VAR_OBJ(module->body));

  // Keep strict parsing to detect accidental schema mismatches early.
  if (reader.offset != reader.size)
    return RESULT_BYTECODE_INVALID_FORMAT;

  return RESULT_SUCCESS;
}

Result saynaa_bytecode_deserialize_module(VM* vm, Module* module,
                                          const uint8_t* data,
                                          size_t data_size) {
  Result status = RESULT_SUCCESS;
  if (vm == NULL || module == NULL || data == NULL)
    return RESULT_BYTECODE_INVALID_ARGUMENT;

  BytecodeReader reader;
  reader.data = data;
  reader.size = data_size;
  reader.offset = 0;

  uint8_t magic[SAYNAA_BYTECODE_PAYLOAD_MAGIC_SIZE];
  if (!bc_read_bytes(&reader, magic, sizeof(magic))) {
    status = RESULT_BYTECODE_TRUNCATED;
    goto cleanup;
  }
  if (memcmp(magic, SAYNAA_BYTECODE_PAYLOAD_MAGIC, sizeof(magic)) != 0) {
    status = RESULT_BYTECODE_INVALID_FORMAT;
    goto cleanup;
  }

  uint8_t version = 0;
  if (!bc_read_u8(&reader, &version)) {
    status = RESULT_BYTECODE_TRUNCATED;
    goto cleanup;
  }
  if (version == SAYNAA_BYTECODE_PAYLOAD_VERSION) {
    return saynaa_bytecode_deserialize_module_v3(vm, module, data, data_size);
  }
  if (version != SAYNAA_BYTECODE_PAYLOAD_MIN_VERSION) {
    status = RESULT_BYTECODE_VERSION_MISMATCH;
    goto cleanup;
  }

  uint32_t constants_count = 0;
  if (!bc_read_u32(&reader, &constants_count)) {
    status = RESULT_BYTECODE_TRUNCATED;
    goto cleanup;
  }
  if (constants_count > MAX_CONSTANTS) {
    status = RESULT_BYTECODE_INVALID_FORMAT;
    goto cleanup;
  }

  PendingFunction* pending_fns = NULL;
  uint32_t pending_fn_count = 0;
  PendingClass* pending_classes = NULL;
  uint32_t pending_class_count = 0;

  for (uint32_t i = 0; i < constants_count; i++) {
    uint8_t tag = 0;
    if (!bc_read_u8(&reader, &tag)) {
      status = RESULT_BYTECODE_TRUNCATED;
      goto cleanup;
    }

    switch ((SaynaaBytecodeConstTag) tag) {
      case SAYNAA_BC_CONST_NULL:
        VarBufferWrite(&module->constants, vm, VAR_NULL);
        break;

      case SAYNAA_BC_CONST_BOOL:
        {
          uint8_t value = 0;
          if (!bc_read_u8(&reader, &value)) {
            status = RESULT_BYTECODE_TRUNCATED;
            goto cleanup;
          }
          VarBufferWrite(&module->constants, vm, VAR_BOOL(value != 0));
        }
        break;

      case SAYNAA_BC_CONST_NUMBER:
        {
          double value = 0;
          if (!bc_read_double(&reader, &value)) {
            status = RESULT_BYTECODE_TRUNCATED;
            goto cleanup;
          }
          VarBufferWrite(&module->constants, vm, VAR_NUM(value));
        }
        break;

      case SAYNAA_BC_CONST_STRING:
        {
          uint32_t length = 0;
          if (!bc_read_u32(&reader, &length)) {
            status = RESULT_BYTECODE_TRUNCATED;
            goto cleanup;
          }
          if (length > reader.size - reader.offset) {
            status = RESULT_BYTECODE_TRUNCATED;
            goto cleanup;
          }
          String* str = newInternedStringLength(vm,
                                                (const char*) (reader.data + reader.offset),
                                                length);
          vmPushTempRef(vm, &str->_super); // str.
          VarBufferWrite(&module->constants, vm, VAR_OBJ(str));
          vmPopTempRef(vm); // str.
          reader.offset += length;
        }
        break;

      case SAYNAA_BC_CONST_FUNCTION:
        {
          PendingFunction pending;
          pending.index = i;
          if (!bc_read_u32(&reader, &pending.name_index)) {
            status = RESULT_BYTECODE_TRUNCATED;
            goto cleanup;
          }
          if (!bc_read_u32(&reader, &pending.doc_index)) {
            status = RESULT_BYTECODE_TRUNCATED;
            goto cleanup;
          }
          if (!bc_read_i32(&reader, &pending.arity)) {
            status = RESULT_BYTECODE_TRUNCATED;
            goto cleanup;
          }
          if (!bc_read_u8(&reader, &pending.is_method)) {
            status = RESULT_BYTECODE_TRUNCATED;
            goto cleanup;
          }
          if (!bc_read_u32(&reader, &pending.upvalue_count)) {
            status = RESULT_BYTECODE_TRUNCATED;
            goto cleanup;
          }
          if (!bc_read_i32(&reader, &pending.stack_size)) {
            status = RESULT_BYTECODE_TRUNCATED;
            goto cleanup;
          }

          if (!bc_read_u32(&reader, &pending.opcodes_count)) {
            status = RESULT_BYTECODE_TRUNCATED;
            goto cleanup;
          }
          if (pending.opcodes_count > reader.size - reader.offset) {
            status = RESULT_BYTECODE_TRUNCATED;
            goto cleanup;
          }
          pending.opcodes = NULL;
          if (pending.opcodes_count > 0) {
            pending.opcodes = (uint8_t*) vmRealloc(vm, NULL, 0, pending.opcodes_count);
            if (pending.opcodes == NULL) {
              status = RESULT_BYTECODE_IO_ERROR;
              goto cleanup;
            }
            memcpy(pending.opcodes, reader.data + reader.offset, pending.opcodes_count);
            reader.offset += pending.opcodes_count;
          }

          if (!bc_read_u32(&reader, &pending.oplines_count)) {
            status = RESULT_BYTECODE_TRUNCATED;
            goto cleanup;
          }
          pending.oplines = NULL;
          if (pending.oplines_count > 0) {
            size_t max_count = (reader.size - reader.offset) / sizeof(uint32_t);
            if (pending.oplines_count > max_count) {
              status = RESULT_BYTECODE_TRUNCATED;
              goto cleanup;
            }
            size_t size = pending.oplines_count * sizeof(uint32_t);
            pending.oplines = (uint32_t*) vmRealloc(vm, NULL, 0, size);
            if (pending.oplines == NULL) {
              status = RESULT_BYTECODE_IO_ERROR;
              goto cleanup;
            }
            for (uint32_t j = 0; j < pending.oplines_count; j++) {
              uint32_t line = 0;
              if (!bc_read_u32(&reader, &line)) {
                status = RESULT_BYTECODE_TRUNCATED;
                goto cleanup;
              }
              pending.oplines[j] = line;
            }
          }

          pending_fns = (PendingFunction*) vmRealloc(
              vm, pending_fns, pending_fn_count * sizeof(PendingFunction),
              (pending_fn_count + 1) * sizeof(PendingFunction));
          if (pending_fns == NULL) {
            status = RESULT_BYTECODE_IO_ERROR;
            goto cleanup;
          }
          pending_fns[pending_fn_count++] = pending;

          VarBufferWrite(&module->constants, vm, VAR_NULL);
        }
        break;

      case SAYNAA_BC_CONST_CLASS:
        {
          PendingClass pending;
          pending.index = i;
          if (!bc_read_u32(&reader, &pending.name_index)) {
            status = RESULT_BYTECODE_TRUNCATED;
            goto cleanup;
          }
          if (!bc_read_u32(&reader, &pending.doc_index)) {
            status = RESULT_BYTECODE_TRUNCATED;
            goto cleanup;
          }
          if (!bc_read_u8(&reader, &pending.class_of)) {
            status = RESULT_BYTECODE_TRUNCATED;
            goto cleanup;
          }

          pending_classes = (PendingClass*) vmRealloc(
              vm, pending_classes, pending_class_count * sizeof(PendingClass),
              (pending_class_count + 1) * sizeof(PendingClass));
          if (pending_classes == NULL) {
            status = RESULT_BYTECODE_IO_ERROR;
            goto cleanup;
          }
          pending_classes[pending_class_count++] = pending;

          VarBufferWrite(&module->constants, vm, VAR_NULL);
        }
        break;

      default:
          status = RESULT_BYTECODE_UNSUPPORTED_CONST;
        goto cleanup;
    }
  }

  uint32_t body_index = 0;
  if (!bc_read_u32(&reader, &body_index)) {
    status = RESULT_BYTECODE_TRUNCATED;
    goto cleanup;
  }

  for (uint32_t i = 0; i < pending_fn_count; i++) {
    PendingFunction* pending = &pending_fns[i];
    if (pending->name_index >= module->constants.count) {
      status = RESULT_BYTECODE_INVALID_FORMAT;
      goto cleanup;
    }

    String* name = moduleGetStringAt(module, (int) pending->name_index);
    if (name == NULL) {
      status = RESULT_BYTECODE_INVALID_FORMAT;
      goto cleanup;
    }
    String* doc = NULL;
    if (pending->doc_index != UINT32_MAX) {
      doc = moduleGetStringAt(module, (int) pending->doc_index);
      if (doc == NULL) {
        status = RESULT_BYTECODE_INVALID_FORMAT;
        goto cleanup;
      }
    }

    Function* fn = newFunctionRaw(vm, module, name, doc, pending->arity,
                                  pending->is_method != 0,
                                  (int) pending->upvalue_count);
    vmPushTempRef(vm, &fn->_super); // fn.

    fn->fn->stack_size = pending->stack_size;

    if (pending->opcodes_count > 0) {
      ByteBufferReserve(&fn->fn->opcodes, vm, pending->opcodes_count);
      memcpy(fn->fn->opcodes.data, pending->opcodes, pending->opcodes_count);
      fn->fn->opcodes.count = pending->opcodes_count;
      vmRealloc(vm, pending->opcodes, pending->opcodes_count, 0);
      pending->opcodes = NULL;
    }

    if (pending->oplines_count > 0) {
      UintBufferReserve(&fn->fn->oplines, vm, pending->oplines_count);
      memcpy(fn->fn->oplines.data, pending->oplines,
             pending->oplines_count * sizeof(uint32_t));
      fn->fn->oplines.count = pending->oplines_count;
      vmRealloc(vm, pending->oplines, pending->oplines_count * sizeof(uint32_t), 0);
      pending->oplines = NULL;
    }

    module->constants.data[pending->index] = VAR_OBJ(fn);
    vmPopTempRef(vm); // fn.
  }

  for (uint32_t i = 0; i < pending_class_count; i++) {
    PendingClass* pending = &pending_classes[i];
    if (pending->name_index >= module->constants.count) {
      status = RESULT_BYTECODE_INVALID_FORMAT;
      goto cleanup;
    }

    String* name = moduleGetStringAt(module, (int) pending->name_index);
    if (name == NULL) {
      status = RESULT_BYTECODE_INVALID_FORMAT;
      goto cleanup;
    }
    String* doc = NULL;
    if (pending->doc_index != UINT32_MAX) {
      doc = moduleGetStringAt(module, (int) pending->doc_index);
      if (doc == NULL) {
        status = RESULT_BYTECODE_INVALID_FORMAT;
        goto cleanup;
      }
    }

    Class* cls = newClassRaw(vm, module, name, doc);
    vmPushTempRef(vm, &cls->_super); // cls.

    cls->class_of = (VarType) pending->class_of;
    module->constants.data[pending->index] = VAR_OBJ(cls);
    vmPopTempRef(vm); // cls.
  }

  if (body_index >= module->constants.count) {
    status = RESULT_BYTECODE_INVALID_FORMAT;
    goto cleanup;
  }
  Var body_fn = module->constants.data[body_index];
  if (!IS_OBJ_TYPE(body_fn, OBJ_FUNC)) {
    status = RESULT_BYTECODE_INVALID_FORMAT;
    goto cleanup;
  }

  Closure* body = newClosure(vm, (Function*) AS_OBJ(body_fn));
  vmPushTempRef(vm, &body->_super); // body.
  module->body = body;
  vmPopTempRef(vm); // body.

  moduleSetGlobal(vm, module, IMPLICIT_MAIN_NAME,
                  (uint32_t) strlen(IMPLICIT_MAIN_NAME), VAR_OBJ(module->body));

  status = RESULT_SUCCESS;

cleanup:
  free_pending_functions(vm, pending_fns, pending_fn_count);
  if (pending_fns != NULL) {
    vmRealloc(vm, pending_fns, pending_fn_count * sizeof(PendingFunction), 0);
  }
  if (pending_classes != NULL) {
    vmRealloc(vm, pending_classes, pending_class_count * sizeof(PendingClass), 0);
  }
  return status;
}
