/*
 * Copyright (c) 2022-2026 Mohamed Abdifatah. All rights reserved.
 * Distributed Under The MIT License
 */

#include "miniz/miniz.h"
#include "saynaa_optionals.h"

#include <errno.h>
#include <math.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef enum {
  ZIP_STATE_CLOSED = 0,
  ZIP_STATE_READER = 1,
  ZIP_STATE_WRITER = 2,
  ZIP_STATE_EDITOR = 3,
} ZipState;

typedef struct {
  mz_zip_archive archive;
  char* path;
  ZipState state;
  bool finalized;
} Zip;

static char* copyCString(VM* vm, const char* text) {
  size_t length = strlen(text);
  char* copy = (char*) Realloc(vm, NULL, length + 1);
  if (copy == NULL)
    return NULL;

  memcpy(copy, text, length + 1);
  return copy;
}

static void* _zipNew(VM* vm) {
  Zip* zip = (Zip*) Realloc(vm, NULL, sizeof(Zip));
  ASSERT(zip != NULL, "Realloc failed.");
  memset(zip, 0, sizeof(Zip));
  mz_zip_zero_struct(&zip->archive);
  zip->state = ZIP_STATE_CLOSED;
  return zip;
}

static void zipSetMinizError(VM* vm, mz_zip_archive* archive, const char* action) {
  SetRuntimeErrorFmt(vm, "Failed to %s zip archive: %s.", action,
                     mz_zip_get_error_string(mz_zip_get_last_error(archive)));
}

static void zipCloseInternal(VM* vm, Zip* zip, bool report_errors) {
  if (zip == NULL || zip->state == ZIP_STATE_CLOSED)
    return;

  bool ok = true;
  if (zip->state == ZIP_STATE_WRITER) {
    if (!zip->finalized) {
      if (!mz_zip_writer_finalize_archive(&zip->archive)) {
        ok = false;
        if (report_errors) {
          zipSetMinizError(vm, &zip->archive, "finalize");
        }
      } else {
        zip->finalized = true;
      }
    }

    if (!mz_zip_writer_end(&zip->archive)) {
      ok = false;
      if (report_errors && !VM_HAS_ERROR(vm)) {
        zipSetMinizError(vm, &zip->archive, "close");
      }
    }

  } else if (zip->state == ZIP_STATE_READER) {
    if (!mz_zip_reader_end(&zip->archive)) {
      ok = false;
      if (report_errors && !VM_HAS_ERROR(vm)) {
        zipSetMinizError(vm, &zip->archive, "close");
      }
    }
  }

  mz_zip_zero_struct(&zip->archive);
  zip->state = ZIP_STATE_CLOSED;
  zip->finalized = false;

  if (!ok && report_errors && !VM_HAS_ERROR(vm))
    SetRuntimeError(vm, "Failed to close zip archive.");
}

static void _zipDelete(VM* vm, void* ptr) {
  Zip* zip = (Zip*) ptr;
  if (zip == NULL)
    return;

  zipCloseInternal(vm, zip, false);

  if (zip->path != NULL) {
    Realloc(vm, zip->path, 0);
    zip->path = NULL;
  }

  Realloc(vm, zip, 0);
}

static Zip* zipGetThis(VM* vm) {
  Zip* zip = (Zip*) GetThis(vm);
  if (zip == NULL) {
    SetRuntimeError(vm, "Zip archive instance is NULL.");
    return NULL;
  }
  return zip;
}

static bool zipIsWritable(const char* mode) {
  return strcmp(mode, "w") == 0 || strcmp(mode, "a") == 0 || strcmp(mode, "x") == 0;
}

static bool zipIsReadable(const char* mode) {
  return strcmp(mode, "r") == 0 || strcmp(mode, "a") == 0;
}

static const char* zipArcnameFromSource(const char* source) {
  const char* slash = strrchr(source, '/');
  const char* backslash = strrchr(source, '\\');
  const char* base = source;

  if (slash != NULL && backslash != NULL)
    base = (slash > backslash) ? slash + 1 : backslash + 1;
  else if (slash != NULL)
    base = slash + 1;
  else if (backslash != NULL)
    base = backslash + 1;

  return base;
}

static bool zipInitArchive(VM* vm, Zip* zip, const char* path, const char* mode) {
  if (zip->state != ZIP_STATE_CLOSED) {
    SetRuntimeError(vm, "Zip archive is already open.");
    return false;
  }

  char* path_copy = copyCString(vm, path);
  if (path_copy == NULL) {
    SetRuntimeError(vm, "Failed to copy zip path.");
    return false;
  }

  mz_zip_zero_struct(&zip->archive);

  mz_bool status = MZ_FALSE;
  if (zipIsReadable(mode) && !zipIsWritable(mode)) {
    status = mz_zip_reader_init_file(&zip->archive, path_copy, 0);
    if (status) {
      zip->state = ZIP_STATE_READER;
      zip->finalized = false;
    }

  } else if (strcmp(mode, "a") == 0) {
    // Append mode: if file exists, convert reader->writer, otherwise create new writer.
    struct stat st;
    if (stat(path_copy, &st) == 0) {
      // file exists: init reader then convert to writer for in-place append
      status = mz_zip_reader_init_file(&zip->archive, path_copy, MZ_ZIP_FLAG_READ_ALLOW_WRITING);
      if (status) {
        if (!mz_zip_writer_init_from_reader(&zip->archive, path_copy)) {
          status = MZ_FALSE;
        } else {
          status = MZ_TRUE;
          zip->state = ZIP_STATE_WRITER;
          zip->finalized = false;
        }
      }
    } else {
      // file doesn't exist: create new writer
      status = mz_zip_writer_init_file(&zip->archive, path_copy, 0);
      if (status) {
        zip->state = ZIP_STATE_WRITER;
        zip->finalized = false;
      }
    }

  } else if (strcmp(mode, "x") == 0) {
    // Exclusive create: fail if exists
    if (access(path_copy, F_OK) == 0) {
      Realloc(vm, path_copy, 0);
      SetRuntimeErrorFmt(vm, "File '%s' already exists.", path);
      return false;
    }
    status = mz_zip_writer_init_file(&zip->archive, path_copy, 0);
    if (status) {
      zip->state = ZIP_STATE_WRITER;
      zip->finalized = false;
    }

  } else if (zipIsWritable(mode)) {
    status = mz_zip_writer_init_file(&zip->archive, path_copy, 0);
    if (status) {
      zip->state = ZIP_STATE_WRITER;
      zip->finalized = false;
    }

  } else {
    Realloc(vm, path_copy, 0);
    SetRuntimeErrorFmt(vm, "Invalid Zip mode '%s'. Use 'r', 'w', 'a' or 'x'.", mode);
    return false;
  }

  if (!status) {
    Realloc(vm, path_copy, 0);
    zipSetMinizError(vm, &zip->archive, zipIsWritable(mode) ? "create" : "open");
    mz_zip_zero_struct(&zip->archive);
    return false;
  }

  if (zip->path != NULL) {
    Realloc(vm, zip->path, 0);
    zip->path = NULL;
  }

  zip->path = path_copy;
  return true;
}

static bool zipRequireOpen(VM* vm, Zip* zip) {
  if (zip->state == ZIP_STATE_CLOSED) {
    SetRuntimeError(vm, "Zip archive is closed.");
    return false;
  }
  return true;
}

static bool zipRequireReader(VM* vm, Zip* zip) {
  if (!zipRequireOpen(vm, zip))
    return false;
  if (zip->state != ZIP_STATE_READER) {
    SetRuntimeError(vm, "Zip archive is not open for reading.");
    return false;
  }
  return true;
}

static bool zipRequireWriter(VM* vm, Zip* zip) {
  if (!zipRequireOpen(vm, zip))
    return false;
  if (zip->state != ZIP_STATE_WRITER) {
    SetRuntimeError(vm, "Zip archive is not open for writing.");
    return false;
  }
  if (zip->finalized) {
    SetRuntimeError(vm, "Zip archive has already been finalized.");
    return false;
  }
  return true;
}

static bool zipRequireEdit(VM* vm, Zip* zip) {
  if (!zipRequireOpen(vm, zip))
    return false;

  if (zip->state != ZIP_STATE_EDITOR) {
    SetRuntimeError(vm, "Zip archive is not editable.");
    return false;
  }

  return true;
}

static bool zipReadEntryAsString(VM* vm, Zip* zip, const char* name) {
  size_t size = 0;
  void* data = mz_zip_reader_extract_file_to_heap(&zip->archive, name, &size, 0);
  if (data == NULL) {
    zipSetMinizError(vm, &zip->archive, "extract from");
    return false;
  }

  setSlotStringLength(vm, 0, (const char*) data, (uint32_t) size);
  mz_free(data);
  return true;
}

static bool zipReadEntryAtIndex(VM* vm, Zip* zip, mz_uint index) {
  size_t size = 0;
  void* data = mz_zip_reader_extract_to_heap(&zip->archive, index, &size, 0);
  if (data == NULL) {
    zipSetMinizError(vm, &zip->archive, "extract from");
    return false;
  }

  setSlotStringLength(vm, 0, (const char*) data, (uint32_t) size);
  mz_free(data);
  return true;
}

static void zipFillStatMap(VM* vm, const mz_zip_archive_file_stat* stat) {
  reserveSlots(vm, 6);
  NewMap(vm, 0);

  setSlotString(vm, 1, "name");
  setSlotString(vm, 2, stat->m_filename);
  MapSet(vm, 0, 1, 2);

  setSlotString(vm, 1, "comment");
  setSlotString(vm, 2, stat->m_comment);
  MapSet(vm, 0, 1, 2);

  setSlotString(vm, 1, "compressed_size");
  setSlotNumber(vm, 2, (double) stat->m_comp_size);
  MapSet(vm, 0, 1, 2);

  setSlotString(vm, 1, "uncompressed_size");
  setSlotNumber(vm, 2, (double) stat->m_uncomp_size);
  MapSet(vm, 0, 1, 2);

  setSlotString(vm, 1, "directory");
  setSlotBool(vm, 2, stat->m_is_directory != 0);
  MapSet(vm, 0, 1, 2);

  setSlotString(vm, 1, "supported");
  setSlotBool(vm, 2, stat->m_is_supported != 0);
  MapSet(vm, 0, 1, 2);
}

saynaa_function(_zipInit, "Zip.ZipFile._init(path:String, mode:String) -> Null",
                "Initialize a ZIP archive for reading or writing.") {
  int argc = GetArgc(vm);
  if (!CheckArgcRange(vm, argc, 2, 2))
    return;

  const char* path;
  const char* mode;
  if (!ValidateSlotString(vm, 1, &path, NULL))
    return;
  if (!ValidateSlotString(vm, 2, &mode, NULL))
    return;

  Zip* zip = zipGetThis(vm);
  if (zip == NULL)
    return;

  if (!zipInitArchive(vm, zip, path, mode))
    return;
}

saynaa_function(_zipClose, "Zip.ZipFile.close() -> Null",
                "Close the archive and release miniz resources.") {
  Zip* zip = zipGetThis(vm);
  if (zip == NULL)
    return;

  if (!zipRequireOpen(vm, zip))
    return;

  zipCloseInternal(vm, zip, true);
}

saynaa_function(_zipFinalize, "Zip.ZipFile.finalize() -> Bool",
                "Finalize a writable archive before closing it.") {
  Zip* zip = zipGetThis(vm);
  if (zip == NULL)
    return;

  if (!zipRequireWriter(vm, zip))
    return;

  if (!mz_zip_writer_finalize_archive(&zip->archive)) {
    zipSetMinizError(vm, &zip->archive, "finalize");
    return;
  }

  zip->finalized = true;
  setSlotBool(vm, 0, true);
}

saynaa_function(_zipAdd, "Zip.ZipFile.add(name:String, data:String, [level:Number]) -> Bool",
                "Add a string payload to a writable archive.") {
  int argc = GetArgc(vm);
  if (!CheckArgcRange(vm, argc, 2, 3))
    return;

  Zip* zip = zipGetThis(vm);
  if (zip == NULL)
    return;
  if (!zipRequireWriter(vm, zip))
    return;

  const char* name;
  const char* data;
  uint32_t data_len;
  if (!ValidateSlotString(vm, 1, &name, NULL))
    return;
  if (!ValidateSlotString(vm, 2, &data, &data_len))
    return;

  int level = MZ_DEFAULT_COMPRESSION;
  if (argc == 3) {
    double level_number;
    if (!ValidateSlotNumber(vm, 3, &level_number))
      return;
    if (floor(level_number) != level_number) {
      SetRuntimeError(vm, "Expected an integer compression level.");
      return;
    }
    if (level_number < -1 || level_number > 10) {
      SetRuntimeError(vm, "Compression level should be in range -1 to 10.");
      return;
    }
    level = (int) level_number;
  }

  if (!mz_zip_writer_add_mem(&zip->archive, name, data, (size_t) data_len, (mz_uint) level)) {
    zipSetMinizError(vm, &zip->archive, "add to");
    return;
  }

  setSlotBool(vm, 0, true);
}

saynaa_function(_zipAddFile,
                ""
                "Zip.ZipFile.add_file(source:String, [name:String], "
                "[level:Number]) -> Bool",
                "Add a file from disk to a writable archive.") {
  int argc = GetArgc(vm);
  if (!CheckArgcRange(vm, argc, 1, 3))
    return;

  Zip* zip = zipGetThis(vm);
  if (zip == NULL)
    return;
  if (!zipRequireWriter(vm, zip))
    return;

  const char* source;
  if (!ValidateSlotString(vm, 1, &source, NULL))
    return;

  const char* name = zipArcnameFromSource(source);
  if (argc >= 2) {
    if (!ValidateSlotString(vm, 2, &name, NULL))
      return;
  }

  int level = MZ_DEFAULT_COMPRESSION;
  if (argc == 3) {
    double level_number;
    if (!ValidateSlotNumber(vm, 3, &level_number))
      return;
    if (floor(level_number) != level_number) {
      SetRuntimeError(vm, "Expected an integer compression level.");
      return;
    }
    if (level_number < -1 || level_number > 10) {
      SetRuntimeError(vm, "Compression level should be in range -1 to 10.");
      return;
    }
    level = (int) level_number;
  }

  if (!mz_zip_writer_add_file(&zip->archive, name, source, NULL, 0, (mz_uint) level)) {
    zipSetMinizError(vm, &zip->archive, "add file to");
    return;
  }

  setSlotBool(vm, 0, true);
}

saynaa_function(_zipExtract, "Zip.ZipFile.extract(entry:String|Number) -> String",
                "Extract an entry from a readable archive and return it.") {
  int argc = GetArgc(vm);
  if (!CheckArgcRange(vm, argc, 1, 1))
    return;

  Zip* zip = zipGetThis(vm);
  if (zip == NULL)
    return;
  if (!zipRequireReader(vm, zip))
    return;

  VarType type = GetSlotType(vm, 1);
  if (type == vSTRING) {
    const char* name;
    if (!ValidateSlotString(vm, 1, &name, NULL))
      return;
    if (!zipReadEntryAsString(vm, zip, name))
      return;
    return;
  }

  if (type == vNUMBER) {
    int32_t index;
    if (!ValidateSlotInteger(vm, 1, &index))
      return;
    if (index < 0) {
      SetRuntimeError(vm, "Entry index cannot be negative.");
      return;
    }
    if (!zipReadEntryAtIndex(vm, zip, (mz_uint) index))
      return;
    return;
  }

  SetRuntimeError(vm, "Entry must be a string name or number index.");
}

saynaa_function(
    _zipExtractFile,
    ""
    "Zip.ZipFile.extract_file(entry:String|Number, out:String) -> Bool",
    "Extract an entry from a readable archive to a file.") {
  int argc = GetArgc(vm);
  if (!CheckArgcRange(vm, argc, 2, 2))
    return;

  Zip* zip = zipGetThis(vm);
  if (zip == NULL)
    return;
  if (!zipRequireReader(vm, zip))
    return;

  const char* out;
  if (!ValidateSlotString(vm, 2, &out, NULL))
    return;

  mz_bool ok = MZ_FALSE;
  VarType type = GetSlotType(vm, 1);
  if (type == vSTRING) {
    const char* name;
    if (!ValidateSlotString(vm, 1, &name, NULL))
      return;
    ok = mz_zip_reader_extract_file_to_file(&zip->archive, name, out, 0);

  } else if (type == vNUMBER) {
    int32_t index;
    if (!ValidateSlotInteger(vm, 1, &index))
      return;
    if (index < 0) {
      SetRuntimeError(vm, "Entry index cannot be negative.");
      return;
    }
    ok = mz_zip_reader_extract_to_file(&zip->archive, (mz_uint) index, out, 0);

  } else {
    SetRuntimeError(vm, "Entry must be a string name or number index.");
    return;
  }

  if (!ok) {
    zipSetMinizError(vm, &zip->archive, "extract from");
    return;
  }

  setSlotBool(vm, 0, true);
}

saynaa_function(_zipCount, "Zip.ZipFile.count() -> Number",
                "Return the number of files in the archive.") {
  Zip* zip = zipGetThis(vm);
  if (zip == NULL)
    return;
  if (!zipRequireOpen(vm, zip))
    return;

  setSlotNumber(vm, 0, (double) zip->archive.m_total_files);
}

saynaa_function(_zipPath, "Zip.ZipFile.path() -> String",
                "Return the archive path used to open or create it.") {
  Zip* zip = zipGetThis(vm);
  if (zip == NULL)
    return;

  if (zip->path == NULL) {
    setSlotNull(vm, 0);
    return;
  }

  setSlotString(vm, 0, zip->path);
}

saynaa_function(_zipMode, "Zip.ZipFile.mode() -> String", "Return the current archive mode.") {
  Zip* zip = zipGetThis(vm);
  if (zip == NULL)
    return;

  switch (zip->state) {
    case ZIP_STATE_READER:
      setSlotString(vm, 0, "read");
      return;

    case ZIP_STATE_WRITER:
      setSlotString(vm, 0, zip->finalized ? "finalized" : "write");
      return;

    default:
      setSlotString(vm, 0, "closed");
      return;
  }
}

saynaa_function(_zipList, "Zip.ZipFile.list() -> List",
                "Return a list of file names in a readable archive.") {
  Zip* zip = zipGetThis(vm);
  if (zip == NULL)
    return;
  if (!zipRequireReader(vm, zip))
    return;

  reserveSlots(vm, 3);
  NewList(vm, 0);

  mz_uint count = mz_zip_reader_get_num_files(&zip->archive);
  for (mz_uint i = 0; i < count; i++) {
    mz_zip_archive_file_stat stat;
    if (!mz_zip_reader_file_stat(&zip->archive, i, &stat)) {
      zipSetMinizError(vm, &zip->archive, "read from");
      return;
    }

    setSlotString(vm, 1, stat.m_filename);
    if (!ListInsert(vm, 0, -1, 1))
      return;
  }
}

saynaa_function(_zipStat, "Zip.ZipFile.stat(entry:String|Number) -> Map",
                "Return metadata for a readable archive entry.") {
  Zip* zip = zipGetThis(vm);
  if (zip == NULL)
    return;
  if (!zipRequireReader(vm, zip))
    return;

  int32_t index = -1;
  VarType type = GetSlotType(vm, 1);
  if (type == vSTRING) {
    const char* name;
    if (!ValidateSlotString(vm, 1, &name, NULL))
      return;

    int located = mz_zip_reader_locate_file(&zip->archive, name, NULL, 0);
    if (located < 0) {
      SetRuntimeErrorFmt(vm, "File '%s' was not found in the archive.", name);
      return;
    }
    index = (int32_t) located;

  } else if (type == vNUMBER) {
    if (!ValidateSlotInteger(vm, 1, &index))
      return;
    if (index < 0) {
      SetRuntimeError(vm, "Entry index cannot be negative.");
      return;
    }

  } else {
    SetRuntimeError(vm, "Entry must be a string name or number index.");
    return;
  }

  mz_zip_archive_file_stat stat;
  if (!mz_zip_reader_file_stat(&zip->archive, (mz_uint) index, &stat)) {
    zipSetMinizError(vm, &zip->archive, "read from");
    return;
  }

  zipFillStatMap(vm, &stat);
  setSlotString(vm, 1, "index");
  setSlotNumber(vm, 2, (double) index);
  MapSet(vm, 0, 1, 2);
}

saynaa_function(_zipLocate, "Zip.ZipFile.locate(name:String) -> Number",
                "Return the index of a named file or -1 if it is missing.") {
  Zip* zip = zipGetThis(vm);
  if (zip == NULL)
    return;
  if (!zipRequireReader(vm, zip))
    return;

  const char* name;
  if (!ValidateSlotString(vm, 1, &name, NULL))
    return;

  int index = mz_zip_reader_locate_file(&zip->archive, name, NULL, 0);
  setSlotNumber(vm, 0, (double) index);
}

static void ensure_parent_dirs(const char* path) {
  // create parent directories for path, ignoring errors if they exist
  char* copy = strdup(path);
  if (copy == NULL)
    return;
  for (char* p = copy + 1; *p; p++) {
    if (*p == '/' || *p == '\\') {
      char old = *p;
      *p = '\0';
      mkdir(copy, 0755);
      *p = old;
    }
  }
  free(copy);
}

saynaa_function(_zipExtractAll, "Zip.ZipFile.extractall([path:String]) -> Null",
                "Extract all files from archive into a directory.") {
  int argc = GetArgc(vm);
  Zip* zip = zipGetThis(vm);
  if (zip == NULL)
    return;
  if (!zipRequireReader(vm, zip))
    return;

  const char* out = ".";
  if (argc == 1) {
    if (!ValidateSlotString(vm, 1, &out, NULL))
      return;
  }

  mz_uint count = mz_zip_reader_get_num_files(&zip->archive);
  for (mz_uint i = 0; i < count; i++) {
    mz_zip_archive_file_stat stat;
    if (!mz_zip_reader_file_stat(&zip->archive, i, &stat)) {
      zipSetMinizError(vm, &zip->archive, "read from");
      return;
    }

    char outpath[4096];
    snprintf(outpath, sizeof(outpath), "%s/%s", out, stat.m_filename);

    if (stat.m_is_directory) {
      mkdir(outpath, 0755);
      continue;
    }

    ensure_parent_dirs(outpath);
    if (!mz_zip_reader_extract_to_file(&zip->archive, i, outpath, 0)) {
      zipSetMinizError(vm, &zip->archive, "extract to");
      return;
    }
  }

  setSlotNull(vm, 0);
}

saynaa_function(_zipInfolist, "Zip.ZipFile.infolist() -> List",
                "Return a list of maps with info on each archive member.") {
  Zip* zip = zipGetThis(vm);
  if (zip == NULL)
    return;
  if (!zipRequireReader(vm, zip))
    return;

  reserveSlots(vm, 6);
  NewList(vm, 0);

  mz_uint count = mz_zip_reader_get_num_files(&zip->archive);
  for (mz_uint i = 0; i < count; i++) {
    mz_zip_archive_file_stat stat;
    if (!mz_zip_reader_file_stat(&zip->archive, i, &stat)) {
      zipSetMinizError(vm, &zip->archive, "read from");
      return;
    }

    // create a map in slot[1] and populate it
    NewMap(vm, 1);

    setSlotString(vm, 2, "name");
    setSlotString(vm, 3, stat.m_filename);
    MapSet(vm, 1, 2, 3);

    setSlotString(vm, 2, "comment");
    setSlotString(vm, 3, stat.m_comment);
    MapSet(vm, 1, 2, 3);

    setSlotString(vm, 2, "compressed_size");
    setSlotNumber(vm, 3, (double) stat.m_comp_size);
    MapSet(vm, 1, 2, 3);

    setSlotString(vm, 2, "uncompressed_size");
    setSlotNumber(vm, 3, (double) stat.m_uncomp_size);
    MapSet(vm, 1, 2, 3);

    setSlotString(vm, 2, "directory");
    setSlotBool(vm, 3, stat.m_is_directory != 0);
    MapSet(vm, 1, 2, 3);

    setSlotString(vm, 2, "supported");
    setSlotBool(vm, 3, stat.m_is_supported != 0);
    MapSet(vm, 1, 2, 3);

    setSlotString(vm, 2, "index");
    setSlotNumber(vm, 3, (double) i);
    MapSet(vm, 1, 2, 3);

    if (!ListInsert(vm, 0, -1, 1))
      return;
  }
}

saynaa_function(
    _zipOpenEntry, "Zip.ZipFile.open(entry:String|Number[, mode:String]) -> String",
    "Open an archive member for reading. Returns bytes as string.") {
  int argc = GetArgc(vm);
  if (!CheckArgcRange(vm, argc, 1, 2))
    return;

  Zip* zip = zipGetThis(vm);
  if (zip == NULL)
    return;
  if (!zipRequireReader(vm, zip))
    return;

  VarType type = GetSlotType(vm, 1);
  const char* mode = "r";
  if (argc == 2) {
    if (!ValidateSlotString(vm, 2, &mode, NULL))
      return;
  }

  if (type == vSTRING) {
    const char* name;
    if (!ValidateSlotString(vm, 1, &name, NULL))
      return;
    if (!zipReadEntryAsString(vm, zip, name))
      return;
    return;
  }

  if (type == vNUMBER) {
    int32_t index;
    if (!ValidateSlotInteger(vm, 1, &index))
      return;
    if (index < 0) {
      SetRuntimeError(vm, "Entry index cannot be negative.");
      return;
    }
    if (!zipReadEntryAtIndex(vm, zip, (mz_uint) index))
      return;
    return;
  }

  SetRuntimeError(vm, "Entry must be a string name or number index.");
}

saynaa_function(
    _zipTest, "Zip.ZipFile.testzip() -> String|Null",
    ""
    "Read each file and return name of first corrupt file, or null.") {
  Zip* zip = zipGetThis(vm);
  if (zip == NULL)
    return;
  if (!zipRequireReader(vm, zip))
    return;

  mz_uint count = mz_zip_reader_get_num_files(&zip->archive);
  for (mz_uint i = 0; i < count; i++) {
    size_t size = 0;
    void* data = mz_zip_reader_extract_to_heap(&zip->archive, i, &size, 0);
    if (data == NULL) {
      mz_zip_archive_file_stat stat;
      if (mz_zip_reader_file_stat(&zip->archive, i, &stat)) {
        setSlotString(vm, 0, stat.m_filename);
        return;
      }
      setSlotNull(vm, 0);
      return;
    }
    mz_free(data);
  }

  setSlotNull(vm, 0);
}

saynaa_function(_zipPrintDir, "Zip.ZipFile.printdir() -> Null",
                "Print a table of contents for the archive to stdout.") {
  Zip* zip = zipGetThis(vm);
  if (zip == NULL)
    return;
  if (!zipRequireReader(vm, zip))
    return;

  mz_uint count = mz_zip_reader_get_num_files(&zip->archive);
  for (mz_uint i = 0; i < count; i++) {
    mz_zip_archive_file_stat stat;
    if (!mz_zip_reader_file_stat(&zip->archive, i, &stat))
      continue;
    printf("%10zu %10zu %s\n", (size_t) stat.m_comp_size,
           (size_t) stat.m_uncomp_size, stat.m_filename);
  }

  setSlotNull(vm, 0);
}

saynaa_function(_zipIsZipfile, "is_zipfile(path:String) -> Bool",
                "Return true if the given path looks like a ZIP archive.") {
  const char* path;
  if (!ValidateSlotString(vm, 1, &path, NULL))
    return;

  mz_zip_archive tmp;
  mz_zip_zero_struct(&tmp);
  mz_bool ok = mz_zip_reader_init_file(&tmp, path, 0);
  if (ok)
    mz_zip_reader_end(&tmp);

  setSlotBool(vm, 0, ok != 0);
}

void registerModuleZip(VM* vm) {
  Handle* zip = NewModule(vm, "Zip");

  Handle* archive = NewClass(vm, "ZipFile", NULL, zip, _zipNew, _zipDelete,
                             "A ZIP archive object backed by miniz.");

  ADD_METHOD(archive, "_init", _zipInit, 2);
  ADD_METHOD(archive, "close", _zipClose, 0);
  ADD_METHOD(archive, "finalize", _zipFinalize, 0);
  ADD_METHOD(archive, "writestr", _zipAdd, -1);
  ADD_METHOD(archive, "write", _zipAddFile, -1);
  ADD_METHOD(archive, "extract", _zipExtract, 1);
  ADD_METHOD(archive, "extract_file", _zipExtractFile, 2);
  ADD_METHOD(archive, "extractall", _zipExtractAll, -1);
  ADD_METHOD(archive, "count", _zipCount, 0);
  ADD_METHOD(archive, "path", _zipPath, 0);
  ADD_METHOD(archive, "mode", _zipMode, 0);
  ADD_METHOD(archive, "list", _zipList, 0);
  ADD_METHOD(archive, "namelist", _zipList, 0);
  ADD_METHOD(archive, "infolist", _zipInfolist, 0);
  ADD_METHOD(archive, "stat", _zipStat, 1);
  ADD_METHOD(archive, "locate", _zipLocate, 1);
  ADD_METHOD(archive, "read", _zipExtract, 1);
  ADD_METHOD(archive, "open", _zipOpenEntry, -1);
  ADD_METHOD(archive, "testzip", _zipTest, 0);
  ADD_METHOD(archive, "printdir", _zipPrintDir, 0);
  REGISTER_FN(zip, "is_zipfile", _zipIsZipfile, 1);

  /* Module-level compression constants */
  reserveSlots(vm, 2);
  setSlotHandle(vm, 0, zip); // slot[0] = Zip module
  setSlotNumber(vm, 1, 0);   // stored
  setAttribute(vm, 0, "ZIP_STORED", 1);
  setSlotNumber(vm, 1, (double) MZ_DEFLATED); // deflated
  setAttribute(vm, 0, "ZIP_DEFLATED", 1);
  setSlotNumber(vm, 1, (double) MZ_DEFAULT_COMPRESSION);
  setAttribute(vm, 0, "ZIP_DEFAULT_COMPRESSION", 1);

  registerModule(vm, zip);
  releaseHandle(vm, archive);
  releaseHandle(vm, zip);
}
