/*
 * Copyright (c) 2022-2026 Mohamed Abdifatah. All rights reserved.
 * Distributed Under The MIT License
 */

#pragma once

#include "saynaa_internal.h"
#include "saynaa_value.h"

#define SAYNAA_BYTECODE_MAGIC_SIZE 8
#define SAYNAA_BYTECODE_HEADER_SIZE 28

#define SAYNAA_BYTECODE_FLAG_SIGNED 0x01
#define SAYNAA_BYTECODE_FLAG_ENCRYPTED 0x02
#define SAYNAA_BYTECODE_FLAG_DEBUG 0x04

// Payload format magic and version. Bump version when payload layout changes.
#define SAYNAA_BYTECODE_PAYLOAD_MAGIC "SBC1"
#define SAYNAA_BYTECODE_PAYLOAD_MAGIC_SIZE 4
#if defined(SAYNAA_REG_VM)
#define SAYNAA_BYTECODE_PAYLOAD_VERSION 3
#else
#define SAYNAA_BYTECODE_PAYLOAD_VERSION 2
#endif

typedef struct SaynaaBytecodeHeader {
  uint8_t magic[SAYNAA_BYTECODE_MAGIC_SIZE];
  uint8_t version_major;
  uint8_t version_minor;
  uint8_t version_patch;
  uint8_t flags;
  uint32_t bytecode_size;
  uint32_t checksum;
  uint64_t timestamp;
} SaynaaBytecodeHeader;

typedef struct SaynaaBytecode {
  uint8_t* data;
  size_t size;
  uint8_t flags;
  uint32_t checksum;
  uint64_t timestamp;
} SaynaaBytecode;

void saynaa_bytecode_init_header(SaynaaBytecodeHeader* header, uint8_t flags,
                                 uint32_t bytecode_size, uint32_t checksum,
                                 uint64_t timestamp);

Result saynaa_bytecode_encode_header(const SaynaaBytecodeHeader* header,
                                     uint8_t* out, size_t out_size);

Result saynaa_bytecode_decode_header(const uint8_t* data, size_t data_size,
                                     SaynaaBytecodeHeader* out);

Result saynaa_bytecode_validate_header(const SaynaaBytecodeHeader* header,
                                       size_t total_size);

uint32_t saynaa_bytecode_crc32(const uint8_t* data, size_t size);

Result saynaa_bytecode_validate_checksum(const SaynaaBytecodeHeader* header,
                                         const uint8_t* bytecode,
                                         size_t bytecode_size);

// Validate payload magic/version without decoding the entire module.
Result saynaa_bytecode_validate_payload(const uint8_t* payload,
                                        size_t payload_size);

Result saynaa_bytecode_write_file(const char* path,
                                  const uint8_t* bytecode,
                                  size_t bytecode_size,
                                  uint8_t flags,
                                  uint64_t timestamp);

void saynaa_bytecode_init(SaynaaBytecode* bytecode);

void saynaa_bytecode_clear(VM* vm, SaynaaBytecode* bytecode);

Result saynaa_bytecode_set_payload(VM* vm, SaynaaBytecode* bytecode,
                                   const uint8_t* payload,
                                   size_t payload_size,
                                   uint8_t flags,
                                   uint64_t timestamp);

Result saynaa_bytecode_save(const SaynaaBytecode* bytecode,
                            const char* path);

Result saynaa_bytecode_run(VM* vm, const SaynaaBytecode* bytecode);

// Build a bytecode output path from an input path. Caller must free with Realloc().
char* saynaa_bytecode_build_path(VM* vm, const char* input_path);

// Serialize a compiled module into a bytecode payload (without the header).
Result saynaa_bytecode_serialize_module(VM* vm, Module* module,
                                        ByteBuffer* out);

// Deserialize a bytecode payload into a module (expects module to be allocated).
Result saynaa_bytecode_deserialize_module(VM* vm, Module* module,
                                          const uint8_t* data,
                                          size_t data_size);
