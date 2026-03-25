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

typedef enum SaynaaBytecodeStatus {
  SAYNAA_BC_OK = 0,
  SAYNAA_BC_INVALID_ARGUMENT,
  SAYNAA_BC_INCOMPLETE_HEADER,
  SAYNAA_BC_INVALID_MAGIC,
  SAYNAA_BC_VERSION_MISMATCH,
  SAYNAA_BC_SIZE_MISMATCH,
  SAYNAA_BC_CHECKSUM_MISMATCH,
  SAYNAA_BC_INVALID_FORMAT,
  SAYNAA_BC_UNSUPPORTED_CONST,
  SAYNAA_BC_TRUNCATED,
} SaynaaBytecodeStatus;

const char* saynaa_bytecode_status_message(SaynaaBytecodeStatus status);

void saynaa_bytecode_init_header(SaynaaBytecodeHeader* header, uint8_t flags,
                                 uint32_t bytecode_size, uint32_t checksum,
                                 uint64_t timestamp);

SaynaaBytecodeStatus saynaa_bytecode_encode_header(const SaynaaBytecodeHeader* header,
                                                   uint8_t* out, size_t out_size);

SaynaaBytecodeStatus saynaa_bytecode_decode_header(const uint8_t* data, size_t data_size,
                                                   SaynaaBytecodeHeader* out);

SaynaaBytecodeStatus saynaa_bytecode_validate_header(const SaynaaBytecodeHeader* header,
                                                     size_t total_size);

uint32_t saynaa_bytecode_crc32(const uint8_t* data, size_t size);

SaynaaBytecodeStatus saynaa_bytecode_validate_checksum(const SaynaaBytecodeHeader* header,
                                                       const uint8_t* bytecode,
                                                       size_t bytecode_size);

SaynaaBytecodeStatus saynaa_bytecode_write_file(const char* path,
                                                const uint8_t* bytecode,
                                                size_t bytecode_size,
                                                uint8_t flags,
                                                uint64_t timestamp);

void saynaa_bytecode_init(SaynaaBytecode* bytecode);

void saynaa_bytecode_clear(VM* vm, SaynaaBytecode* bytecode);

SaynaaBytecodeStatus saynaa_bytecode_set_payload(VM* vm, SaynaaBytecode* bytecode,
                                                 const uint8_t* payload,
                                                 size_t payload_size,
                                                 uint8_t flags,
                                                 uint64_t timestamp);

SaynaaBytecodeStatus saynaa_bytecode_save(const SaynaaBytecode* bytecode,
                                          const char* path);

Result saynaa_bytecode_run(VM* vm, const SaynaaBytecode* bytecode);

// Build a bytecode output path from an input path. Caller must free with Realloc().
char* saynaa_bytecode_build_path(VM* vm, const char* input_path);

// Serialize a compiled module into a bytecode payload (without the header).
SaynaaBytecodeStatus saynaa_bytecode_serialize_module(VM* vm, Module* module,
                                                      ByteBuffer* out);

// Deserialize a bytecode payload into a module (expects module to be allocated).
SaynaaBytecodeStatus saynaa_bytecode_deserialize_module(VM* vm, Module* module,
                                                        const uint8_t* data,
                                                        size_t data_size);
