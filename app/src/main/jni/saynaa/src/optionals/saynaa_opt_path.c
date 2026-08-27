/*
 * Copyright (c) 2022-2026 Mohamed Abdifatah. All rights reserved.
 * Distributed Under The MIT License
 */

#include "../shared/saynaa_path.h"
#include "saynaa_optionals.h"

#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "dirent/saynaa_dirent.h"

#if defined(_MSC_VER) || (defined(_WIN32) && defined(__TINYC__))
#include <direct.h>
#include <io.h>

// access() function flag defines for windows.
#define F_OK 0
#define W_OK 2
#define R_OK 4

#define access _access
#define getcwd _getcwd
#define lstat stat
#define stat _stat

#else
#include <unistd.h>
#endif

#ifdef _WIN32
#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
#endif
#ifndef S_ISREG
#define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
#endif
#else
#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif
#ifndef S_ISREG
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif
#endif

// The maximum path size that default import system supports
// including the null terminator. To be able to support more characters
// override the functions from the host application. Since this is very much
// platform specific we're defining a more general limit.
// See: https://insanecoding.blogspot.com/2007/11/pathmax-simply-isnt.html

// The cstring pointer buffer size used in path.join(p1, p2, ...). Tune this
// value as needed.
#define MAX_JOIN_PATHS 8

/*****************************************************************************/
/* PATH INTERNAL FUNCTIONS                                                   */
/*****************************************************************************/

static inline time_t pathMtime(const char* path) {
  struct stat path_stat;
  if (stat(path, &path_stat))
    return 0; // Error: might be path not exists.
  return path_stat.st_mtime;
}

static inline size_t pathAbs(const char* path, char* buff, size_t buffsz) {
  char cwd[MAX_PATH_LEN];

  if (getcwd(cwd, sizeof(cwd)) == NULL) {
    // TODO: handle error.
  }

  return path_get_absolute(cwd, path, buff, buffsz);
}

/*****************************************************************************/
/* PATH MODULE FUNCTIONS                                                     */
/*****************************************************************************/

saynaa_function(_pathGetCWD, "path.getcwd() -> String", "Returns the current working directory.") {
  char cwd[MAX_PATH_LEN];
  if (getcwd(cwd, sizeof(cwd)) == NULL) {
    // TODO: Handle error.
  }
  setSlotString(vm, 0, cwd);
}

saynaa_function(_pathAbspath, "path.abspath(path:String) -> String",
                "Returns the absolute path of the [path].") {
  const char* path;
  if (!ValidateSlotString(vm, 1, &path, NULL))
    return;

  char abspath[MAX_PATH_LEN];
  uint32_t len = (uint32_t) pathAbs(path, abspath, sizeof(abspath));
  setSlotStringLength(vm, 0, abspath, len);
}

saynaa_function(
    _pathRelpath, "path.relpath(path:String, from:String) -> String",
    "Returns the relative path of the [path] argument from the [from] "
    "directory.") {
  const char *path, *from;
  if (!ValidateSlotString(vm, 1, &path, NULL))
    return;
  if (!ValidateSlotString(vm, 2, &from, NULL))
    return;

  char abs_path[MAX_PATH_LEN];
  pathAbs(path, abs_path, sizeof(abs_path));

  char abs_from[MAX_PATH_LEN];
  pathAbs(from, abs_from, sizeof(abs_from));

  char result[MAX_PATH_LEN];
  uint32_t len = (uint32_t) path_get_relative(abs_from, abs_path, result, sizeof(result));
  setSlotStringLength(vm, 0, result, len);
}

saynaa_function(
    _pathJoin, "path.join(...) -> String",
    "Joins path with path seperator and return it. The maximum count of paths "
    "which can be joined for a call is " TOSTRING(MAX_JOIN_PATHS) ".") {
  const char* paths[MAX_JOIN_PATHS + 1]; // +1 for NULL.
  int argc = GetArgc(vm);

  if (argc > MAX_JOIN_PATHS) {
    SetRuntimeError(
        vm, "Cannot join more than " STRINGIFY(MAX_JOIN_PATHS) "paths.");
    return;
  }

  for (int i = 0; i < argc; i++) {
    ValidateSlotString(vm, i + 1, &paths[i], NULL);
  }
  paths[argc] = NULL;

  char result[MAX_PATH_LEN];
  result[0] = '\0';

  if (argc > 0) {
    strcpy(result, paths[0]);
    path_normalize(result, result, sizeof(result));
    for (int i = 1; i < argc; i++) {
      path_join(result, paths[i], result, sizeof(result));
    }
  }
  setSlotStringLength(vm, 0, result, (uint32_t) strlen(result));
}

saynaa_function(_pathNormpath, "path.normpath(path:String) -> String",
                "Returns the normalized path of the [path].") {
  const char* path;
  if (!ValidateSlotString(vm, 1, &path, NULL))
    return;

  char result[MAX_PATH_LEN];
  uint32_t len = (uint32_t) path_normalize(path, result, sizeof(result));
  setSlotStringLength(vm, 0, result, len);
}

saynaa_function(_pathBaseName, "path.basename(path:String) -> String",
                "Returns the final component for the path") {
  const char* path;
  if (!ValidateSlotString(vm, 1, &path, NULL))
    return;

  const char* base_name;
  size_t length;
  path_basename(path, &base_name, &length);
  setSlotStringLength(vm, 0, base_name, (uint32_t) length);
}

saynaa_function(_pathDirName, "path.dirname(path:String) -> String",
                "Returns the directory of the path.") {
  const char* path;
  if (!ValidateSlotString(vm, 1, &path, NULL))
    return;

  size_t length;
  path_dirname(path, &length);
  setSlotStringLength(vm, 0, path, (uint32_t) length);
}

saynaa_function(_pathIsPathAbs, "path.isabspath(path:String) -> Bool",
                "Returns true if the path is absolute otherwise false.") {
  const char* path;
  if (!ValidateSlotString(vm, 1, &path, NULL))
    return;

  setSlotBool(vm, 0, path_is_absolute(path));
}

saynaa_function(_pathGetExtension, "path.getext(path:String) -> String",
                "Returns the file extension of the path.") {
  const char* path;
  if (!ValidateSlotString(vm, 1, &path, NULL))
    return;

  const char* ext;
  size_t length;
  if (path_extension(path, &ext, &length)) {
    setSlotStringLength(vm, 0, ext, (uint32_t) length);
  } else {
    setSlotStringLength(vm, 0, NULL, 0);
  }
}

saynaa_function(_pathExists, "path.exists(path:String) -> String",
                "Returns true if the file exists.") {
  const char* path;
  if (!ValidateSlotString(vm, 1, &path, NULL))
    return;
  setSlotBool(vm, 0, path_is_exists(path));
}

saynaa_function(_pathIsFile, "path.isfile(path:String) -> Bool",
                "Returns true if the path is a file.") {
  const char* path;
  if (!ValidateSlotString(vm, 1, &path, NULL))
    return;
  setSlotBool(vm, 0, path_is_file(path));
}

saynaa_function(_pathIsDir, "path.isdir(path:String) -> Bool",
                "Returns true if the path is a directory.") {
  const char* path;
  if (!ValidateSlotString(vm, 1, &path, NULL))
    return;
  setSlotBool(vm, 0, path_is_dir(path));
}

saynaa_function(_pathListDir, "path.listdir(path:String='.') -> List",
                "Returns detailed entries in the directory.") {
  int argc = GetArgc(vm);
  if (!CheckArgcRange(vm, argc, 0, 1))
    return;

  const char* path = ".";

  if (argc == 1) {
    if (!ValidateSlotString(vm, 1, &path, NULL))
      return;
  }

  DIR* dirstream = opendir(path);

  if (dirstream == NULL) {
    SetRuntimeErrorFmt(vm, "Cannot open directory '%s'.", path);
    return;
  }

  NewList(vm, 0);

  struct dirent* dir;

  while ((dir = readdir(dirstream)) != NULL) {
    if (!strcmp(dir->d_name, ".") || !strcmp(dir->d_name, ".."))
      continue;

    char fullpath[MAX_PATH_LEN];

    snprintf(fullpath, sizeof(fullpath), "%s/%s", path, dir->d_name);

    struct stat st;

    if (lstat(fullpath, &st) != 0)
      continue;

    const char* type = "unknown";

#ifdef S_ISREG
    if (S_ISREG(st.st_mode))
      type = "file";
#endif

#ifdef S_ISDIR
    else if (S_ISDIR(st.st_mode))
      type = "directory";
#endif

#ifdef S_ISLNK
    else if (S_ISLNK(st.st_mode))
      type = "symlink";
#endif

#ifdef S_ISFIFO
    else if (S_ISFIFO(st.st_mode))
      type = "fifo";
#endif

#ifdef S_ISSOCK
    else if (S_ISSOCK(st.st_mode))
      type = "socket";
#endif

#ifdef S_ISCHR
    else if (S_ISCHR(st.st_mode))
      type = "character";
#endif

#ifdef S_ISBLK
    else if (S_ISBLK(st.st_mode))
      type = "block";
#endif

    bool hidden = dir->d_name[0] == '.';

    bool canRead = false;
    bool canWrite = false;
    bool canExecute = false;

#ifdef _WIN32

    canRead = _access(fullpath, 4) == 0;
    canWrite = _access(fullpath, 2) == 0;
    canExecute = false; // Windows _access doesn't support execution checks (X_OK)

#else

    canRead = access(fullpath, R_OK) == 0;
    canWrite = access(fullpath, W_OK) == 0;
    canExecute = access(fullpath, X_OK) == 0;

#endif

    char target[MAX_PATH_LEN];
    target[0] = '\0';

#ifdef S_ISLNK

    if (strcmp(type, "symlink") == 0) {
      ssize_t len = readlink(fullpath, target, sizeof(target) - 1);

      if (len >= 0)
        target[len] = '\0';
    }

#endif

    Map* map = newMap(vm);

    vmPushTempRef(vm, &map->_super);

    // name
    mapSet(vm, map, VAR_OBJ(newString(vm, "name")), VAR_OBJ(newString(vm, dir->d_name)));

    // path
    mapSet(vm, map, VAR_OBJ(newString(vm, "path")), VAR_OBJ(newString(vm, fullpath)));

    // type
    mapSet(vm, map, VAR_OBJ(newString(vm, "type")), VAR_OBJ(newString(vm, type)));

    // size
    mapSet(vm, map, VAR_OBJ(newString(vm, "size")), VAR_NUM(st.st_size));

    // timestamps
    mapSet(vm, map, VAR_OBJ(newString(vm, "modified")), VAR_NUM(st.st_mtime));

    mapSet(vm, map, VAR_OBJ(newString(vm, "accessed")), VAR_NUM(st.st_atime));

    mapSet(vm, map, VAR_OBJ(newString(vm, "changed")), VAR_NUM(st.st_ctime));

    // permissions
    mapSet(vm, map, VAR_OBJ(newString(vm, "permissions")), VAR_NUM(st.st_mode));

    // flags
    mapSet(vm, map, VAR_OBJ(newString(vm, "hidden")), VAR_BOOL(hidden));
#ifndef _WIN32
    mapSet(vm, map, VAR_OBJ(newString(vm, "readonly")), VAR_BOOL(!(st.st_mode & S_IWUSR)));
#else
    mapSet(vm, map, VAR_OBJ(newString(vm, "readonly")),
           VAR_BOOL(_access(fullpath, 2) != 0));
#endif
    mapSet(vm, map, VAR_OBJ(newString(vm, "canRead")), VAR_BOOL(canRead));

    mapSet(vm, map, VAR_OBJ(newString(vm, "canWrite")), VAR_BOOL(canWrite));

    mapSet(vm, map, VAR_OBJ(newString(vm, "canExecute")), VAR_BOOL(canExecute));

    // Only add target for symlinks
    if (target[0] != '\0') {
      mapSet(vm, map, VAR_OBJ(newString(vm, "target")), VAR_OBJ(newString(vm, target)));
    }

    vm->fiber->ret[1] = VAR_OBJ(map);

    ListInsert(vm, 0, -1, 1);

    vmPopTempRef(vm);
  }

  closedir(dirstream);
}

/*****************************************************************************/
/* MODULE REGISTER                                                           */
/*****************************************************************************/

// Add the executables path and exe_path + 'libs/' as a search path for
// the VM.
void _registerSearchPaths(VM* vm) {
  char sep = path_separator();

  char cwd[MAX_PATH_LEN];
  if (getcwd(cwd, sizeof(cwd)) != NULL) {
    size_t len = strlen(cwd);
    if (len < MAX_PATH_LEN - 1) {
      cwd[len] = sep;
      cwd[++len] = '\0';
      AddSearchPath(vm, cwd);
    }
  }

  char buff[MAX_PATH_LEN];
  if (!osGetExeFilePath(buff, MAX_PATH_LEN))
    return;
  size_t length;
  path_dirname(buff, &length);
  if (length == 0)
    return;

  // Add path separator. Otherwise AddSearchPath will fail an assertion.
  char last = buff[length - 1];
  if (last != '/' && last != '\\') {
    buff[length++] = sep;
  }

  // Append "libs" directory.
  const char* libs_name = "libs";
  size_t libs_len = strlen(libs_name);
  if (length + libs_len + 1 < MAX_PATH_LEN) {
    memcpy(buff + length, libs_name, libs_len);
    length += libs_len;
    buff[length++] = sep;
    buff[length] = '\0';

    AddSearchPath(vm, buff);
  }
}

void registerModulePath(VM* vm) {
  _registerSearchPaths(vm);

  Handle* path = NewModule(vm, "path");

  REGISTER_FN(path, "getcwd", _pathGetCWD, 0);
  REGISTER_FN(path, "abspath", _pathAbspath, 1);
  REGISTER_FN(path, "relpath", _pathRelpath, 2);
  REGISTER_FN(path, "join", _pathJoin, -1);
  REGISTER_FN(path, "normpath", _pathNormpath, 1);
  REGISTER_FN(path, "basename", _pathBaseName, 1);
  REGISTER_FN(path, "dirname", _pathDirName, 1);
  REGISTER_FN(path, "isabspath", _pathIsPathAbs, 1);
  REGISTER_FN(path, "getext", _pathGetExtension, 1);
  REGISTER_FN(path, "exists", _pathExists, 1);
  REGISTER_FN(path, "isfile", _pathIsFile, 1);
  REGISTER_FN(path, "isdir", _pathIsDir, 1);
  REGISTER_FN(path, "listdir", _pathListDir, -1);

  registerModule(vm, path);
  releaseHandle(vm, path);
}

#undef MAX_JOIN_PATHS