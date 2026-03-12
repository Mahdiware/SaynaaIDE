/*
 * Copyright (c) 2022-2026 Mohamed Abdifatah. All rights reserved.
 * Distributed Under The MIT License
 */

#include "saynaa_optionals.h"

#include <math.h>
#include <stdlib.h>
#include <sys/stat.h>

#if defined(__EMSCRIPTEN__)
#define _OS_WEB_
#define OS_NAME "web"

#elif defined(_WIN32) || defined(__NT__)
#define _OS_WIN_
#define OS_NAME "windows"

#elif defined(__APPLE__)
#define _OS_APPLE_
#define OS_NAME "apple"

#elif defined(__linux__)
#define _OS_LINUX_
#define OS_NAME "linux"

#else
#define _OS_UNKNOWN_
#define OS_NAME "<?>"
#endif

#if defined(_OS_WIN_)
#include <windows.h>
#endif

#if defined(__linux__) && !defined(NO_DL)
#include <dlfcn.h>
#endif

#if defined(_MSC_VER) || (defined(_WIN32) && defined(__TINYC__))
#include <direct.h>
#include <io.h>

#define getcwd _getcwd
#define chdir _chdir
#define mkdir _mkdir
#define rmdir _rmdir
#define unlink _unlink
#define strdup _strdup
#else
#include <dirent.h>
#include <unistd.h>
#endif

// The maximum path size that default import system supports
// including the null terminator. To be able to support more characters
// override the functions from the host application. Since this is very much
// platform specific we're defining a more general limit.
// See: https://insanecoding.blogspot.com/2007/11/pathmax-simply-isnt.html
#define MAX_PATH_LEN 4096

bool osGetExeFilePath(char* buff, int size) {
#if defined(_OS_WIN_)
  int bytes = GetModuleFileNameA(NULL, buff, size);
  ASSERT(bytes > 0, "GetModuleFileName failed.");
  return true;

#elif defined(_OS_APPLE_)
  unsigned sz = size;
  _NSGetExecutablePath(buff, &sz);
  return true;

#elif defined(_OS_LINUX_)
  char tmp[MAX_PATH_LEN];
  sprintf(tmp, "/proc/%d/exe", getpid());
  int len = readlink(tmp, buff, size);
  buff[len] = '\0';
  return true;

#else
  return false;
#endif
}

// Yes both 'os' and 'path' have getcwd functions.
saynaa_function(_osGetCWD, "os.getcwd() -> String", "Returns the current working directory") {
  char cwd[MAX_PATH_LEN];
  if (getcwd(cwd, sizeof(cwd)) == NULL) {
    // TODO: Handle error.
  }
  setSlotString(vm, 0, cwd);
}

saynaa_function(_osChdir, "os.chdir(path:String)", "Change the current working directory") {
  const char* path;
  if (!ValidateSlotString(vm, 1, &path, NULL))
    return;

  if (chdir(path))
    REPORT_ERRNO(chdir);
}

saynaa_function(_osMkdir, "os.mkdir(path:String)",
                "Creates a directory at the path. The path should be valid.") {
  const char* path;
  if (!ValidateSlotString(vm, 1, &path, NULL))
    return;

#if defined(_OS_WIN_)
  if (mkdir(path)) {
#else
  if (mkdir(path, 0x1ff)) { // TODO: mode flags.
#endif

    // If the directory exists (errno == EEXIST) should I skip it silently ?
    REPORT_ERRNO(mkdir);
  }
}

saynaa_function(_osRmdir, "os.rmdir(path:String)", "Removes an empty directory at the path.") {
  const char* path;
  if (!ValidateSlotString(vm, 1, &path, NULL))
    return;
  if (rmdir(path))
    REPORT_ERRNO(rmdir);
}

saynaa_function(_osUnlink, "os.unlink(path:String)", "Removes a file at the path.") {
  const char* path;
  if (!ValidateSlotString(vm, 1, &path, NULL))
    return;
  if (unlink(path))
    REPORT_ERRNO(unlink);
}

saynaa_function(_osModitime, "os.moditime(path:String) -> Number",
                "Returns the modified timestamp of the file.") {
  const char* path;
  if (!ValidateSlotString(vm, 1, &path, NULL))
    return;

  double mtime = 0;
  struct stat path_stat;
  if (stat(path, &path_stat) == 0)
    mtime = (double) path_stat.st_mtime;
  setSlotNumber(vm, 0, mtime);
}

saynaa_function(_osFileSize, "os.filesize(path:String) -> Number",
                "Returns the file size in bytes.") {
  const char* path;

  if (!ValidateSlotString(vm, 1, &path, NULL))
    return;

  struct stat path_stat;
  if (stat(path, &path_stat) || ((path_stat.st_mode & S_IFMT) != S_IFREG)) {
    SetRuntimeErrorFmt(vm, "Path '%s' wasn't a file.", path);
    return;
  }

  setSlotNumber(vm, 0, path_stat.st_size);
}

#if defined(__linux__)
saynaa_function(_osExec, "os.exec(cmd:String) -> String",
                "Execute the command and return "
                "output.") {
  const char* cmd;
  if (!ValidateSlotString(vm, 1, &cmd, NULL))
    return;

  FILE* fp;
  char* result = NULL;
  char buffer[128];

  fp = popen(cmd, "r"); // Add the mode "r" to open the pipe for reading
  if (fp == NULL) {
    // printf("Failed to execute the command.\n");
    setSlotNull(vm, 0);
    return;
  }

  // Read the output of the command
  if (fgets(buffer, sizeof(buffer), fp) != NULL) {
    // Allocate memory for the result and copy the output
    result = strdup(buffer);
    // Remove the trailing newline character, if present
    size_t len = strlen(result);
    if (len > 0 && result[len - 1] == '\n') {
      result[len - 1] = '\0';
    }
  }

  pclose(fp);
  setSlotString(vm, 0, result);
}
#endif

saynaa_function(
    _osSystem, "os.system(cmd:String) -> Number",
    "Execute the command in a subprocess, Returns the exit code of the child "
    "process.") {
  const char* cmd;

  if (!ValidateSlotString(vm, 1, &cmd, NULL))
    return;

  errno = 0;
  int code = system(cmd);
  if (errno != 0) {
    REPORT_ERRNO(system);
    return;
  }

  setSlotNumber(vm, 0, (double) code);
}

#if defined(__linux__)
saynaa_function(_osSetenv, "os.setenv(name:String, value:String) -> Null",
                "Write an env value and reaturn "
                "null.") {
  const char* name;
  const char* value;
  if (!ValidateSlotString(vm, 1, &name, NULL))
    return;
  if (!ValidateSlotString(vm, 2, &value, NULL))
    return;
  setenv(name, value, 1);

  setSlotNull(vm, 0);
}
#endif

saynaa_function(
    _osGetenv, "os.getenv(name:String) -> String",
    "Returns the environment variable as String if it exists otherwise it'll "
    "return null.") {
  const char* name;
  if (!ValidateSlotString(vm, 1, &name, NULL))
    return;

  const char* output = getenv(name);
  if (output == NULL) {
    setSlotNull(vm, 0);
    return;
  }

  setSlotString(vm, 0, output);
}

saynaa_function(_osExepath, "os.exepath() -> String",
                "Returns the path of the saynaa interpreter executable.") {
  char buff[MAX_PATH_LEN];
  if (!osGetExeFilePath(buff, MAX_PATH_LEN)) {
    SetRuntimeError(vm, "Cannot obtain ececutable path.");
    return;
  }

  setSlotString(vm, 0, buff);
}

List* Arguments(VM* vm) {
  int argc = vm->config.argument.argc;
  const char** argv = vm->config.argument.argv;

  List* list = newList(vm, argc);
  vmPushTempRef(vm, &list->_super); // list.

  for (int i = 0; i < argc; i++) {
    listAppend(vm, list, VAR_OBJ(newString(vm, argv[i])));
  }

  vmPopTempRef(vm); // list.
  return list;
}

/*****************************************************************************/
/* MODULE REGISTER                                                           */
/*****************************************************************************/

void registerModuleOS(VM* vm) {
  Handle* os = NewModule(vm, "os");

  moduleSetGlobal(vm, ((Module*) AS_OBJ(os->value)), "name", 4,
                  VAR_OBJ(newString(vm, OS_NAME)));
  moduleSetGlobal(vm, ((Module*) AS_OBJ(os->value)), "platform", 8,
                  VAR_OBJ(newString(vm, OS_NAME)));

  moduleSetGlobal(vm, ((Module*) AS_OBJ(os->value)), "argv", 4, VAR_OBJ(Arguments(vm)));
  moduleSetGlobal(vm, ((Module*) AS_OBJ(os->value)), "argc", 4,
                  VAR_NUM((double) vm->config.argument.argc));

  REGISTER_FN(os, "getcwd", _osGetCWD, 0);
  REGISTER_FN(os, "chdir", _osChdir, 1);
  REGISTER_FN(os, "mkdir", _osMkdir, 1);
  REGISTER_FN(os, "rmdir", _osRmdir, 1);
  REGISTER_FN(os, "unlink", _osUnlink, 1);
  REGISTER_FN(os, "moditime", _osModitime, 1);
  REGISTER_FN(os, "filesize", _osFileSize, 1);
  REGISTER_FN(os, "system", _osSystem, 1);
#if defined(__linux__)
  REGISTER_FN(os, "exec", _osExec, 1);
  REGISTER_FN(os, "setenv", _osSetenv, 2);
#endif
  REGISTER_FN(os, "getenv", _osGetenv, 1);
  REGISTER_FN(os, "exepath", _osExepath, 0);

  registerModule(vm, os);
  releaseHandle(vm, os);
}

#undef OS_NAME
#undef MAX_PATH_LEN