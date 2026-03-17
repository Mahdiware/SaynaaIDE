/*
 * Copyright (c) 2022-2026 Mohamed Abdifatah. All rights reserved.
 * Distributed Under The MIT License
 */

#include "saynaa_optionals.h"

#ifdef _WIN32
#include <fcntl.h>
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

#include "term/saynaa_term.h"

// A reference to the event class, to check is instance of.
static Handle* _cls_term_event = NULL;
static Handle* _cls_term_config = NULL;

#define SLOT(n) (vm->fiber->ret[n])
#define IS_CLOSURE(v) IS_OBJ_TYPE(v, OBJ_CLOSURE)
#define AS_CLOSURE(v) ((Closure*) AS_OBJ(v))

typedef struct {
  char* data;
  uint32_t count;
  uint32_t capacity;
  bool done;
} SaynaaTermContext;

static SaynaaTermContext _sTermCtx;

static void _termCtxInit() {
  _sTermCtx.data = NULL;
  _sTermCtx.count = 0;
  _sTermCtx.capacity = 0;
  _sTermCtx.done = false;
}

static void _termCtxFree(VM* vm) {
  if (_sTermCtx.data != NULL) {
    Realloc(vm, _sTermCtx.data, 0);
    _sTermCtx.data = NULL;
  }
  _sTermCtx.capacity = 0;
  _sTermCtx.count = 0;
}

static void _termCtxWrite(VM* vm, const char* str, size_t len) {
  if (_sTermCtx.count + len > _sTermCtx.capacity) {
    uint32_t new_cap = (_sTermCtx.capacity == 0) ? 8 : _sTermCtx.capacity * 2;
    while (new_cap < _sTermCtx.count + len)
      new_cap *= 2;
    _sTermCtx.data = (char*) Realloc(vm, _sTermCtx.data, new_cap);
    _sTermCtx.capacity = new_cap;
  }
  memcpy(_sTermCtx.data + _sTermCtx.count, str, len);
  _sTermCtx.count += len;
}

static void _termCtxWriteFmt(VM* vm, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  int len = vsnprintf(NULL, 0, fmt, args);
  va_end(args);

  if (len < 0)
    return;

  if (_sTermCtx.count + len + 1 > _sTermCtx.capacity) {
    uint32_t new_cap = (_sTermCtx.capacity == 0) ? 8 : _sTermCtx.capacity * 2;
    while (new_cap < _sTermCtx.count + len + 1)
      new_cap *= 2;
    _sTermCtx.data = (char*) Realloc(vm, _sTermCtx.data, new_cap);
    _sTermCtx.capacity = new_cap;
  }

  va_start(args, fmt);
  vsnprintf(_sTermCtx.data + _sTermCtx.count, len + 1, fmt, args);
  va_end(args);
  _sTermCtx.count += len;
}

// ----------------------------------------------------------------------------

static void _setSlotVector(VM* vm, int slot, int tmp, double x, double y) {
  if (!ImportModule(vm, "types", slot))
    return;
  if (!GetAttribute(vm, slot, "Vector", slot))
    return;

  setSlotNumber(vm, tmp, x);
  setSlotNumber(vm, tmp + 1, y);
  if (!NewInstance(vm, slot, slot, 2, tmp))
    return;
}

void* _termEventNew(VM* vm) {
  term_Event* event = Realloc(vm, NULL, sizeof(term_Event));
  event->type = TERM_ET_UNKNOWN;
  return event;
}

void _termEventDelete(VM* vm, void* event) {
  Realloc(vm, event, 0);
}

saynaa_function(_termEventGetter, "term.Event_getter() -> Var", "") {
  const char* name;
  if (!ValidateSlotString(vm, 1, &name, NULL))
    return;

  term_Event* event = GetThis(vm);

  if (strcmp(name, "type") == 0) {
    setSlotNumber(vm, 0, (double) event->type);

  } else if (strcmp(name, "keycode") == 0) {
    setSlotNumber(vm, 0, (double) event->key.code);

  } else if (strcmp(name, "ascii") == 0) {
    setSlotNumber(vm, 0, (double) event->key.ascii);

  } else if (strcmp(name, "modifiers") == 0) {
    if (event->type == TERM_ET_KEY_DOWN) {
      setSlotNumber(vm, 0, (double) event->key.modifiers);
    } else {
      setSlotNumber(vm, 0, (double) event->mouse.modifiers);
    }

  } else if (strcmp(name, "button") == 0) {
    setSlotNumber(vm, 0, (double) event->mouse.button);

  } else if (strcmp(name, "pos") == 0) {
    reserveSlots(vm, 2);
    _setSlotVector(vm, 0, 1, event->mouse.pos.x, event->mouse.pos.y);

  } else if (strcmp(name, "scroll") == 0) {
    setSlotBool(vm, 0, event->mouse.scroll);
  }
}

void _registerEnums(VM* vm, Handle* term) {
  reserveSlots(vm, 1);
  setSlotHandle(vm, 0, term);

  setSlotNumber(vm, 1, TERM_KEY_UNKNOWN);
  setAttribute(vm, 0, "KEY_UNKNOWN", 1);
  setSlotNumber(vm, 1, TERM_KEY_0);
  setAttribute(vm, 0, "KEY_0", 1);
  setSlotNumber(vm, 1, TERM_KEY_1);
  setAttribute(vm, 0, "KEY_1", 1);
  setSlotNumber(vm, 1, TERM_KEY_2);
  setAttribute(vm, 0, "KEY_2", 1);
  setSlotNumber(vm, 1, TERM_KEY_3);
  setAttribute(vm, 0, "KEY_3", 1);
  setSlotNumber(vm, 1, TERM_KEY_4);
  setAttribute(vm, 0, "KEY_4", 1);
  setSlotNumber(vm, 1, TERM_KEY_5);
  setAttribute(vm, 0, "KEY_5", 1);
  setSlotNumber(vm, 1, TERM_KEY_6);
  setAttribute(vm, 0, "KEY_6", 1);
  setSlotNumber(vm, 1, TERM_KEY_7);
  setAttribute(vm, 0, "KEY_7", 1);
  setSlotNumber(vm, 1, TERM_KEY_8);
  setAttribute(vm, 0, "KEY_8", 1);
  setSlotNumber(vm, 1, TERM_KEY_9);
  setAttribute(vm, 0, "KEY_9", 1);
  setSlotNumber(vm, 1, TERM_KEY_A);
  setAttribute(vm, 0, "KEY_A", 1);
  setSlotNumber(vm, 1, TERM_KEY_B);
  setAttribute(vm, 0, "KEY_B", 1);
  setSlotNumber(vm, 1, TERM_KEY_C);
  setAttribute(vm, 0, "KEY_C", 1);
  setSlotNumber(vm, 1, TERM_KEY_D);
  setAttribute(vm, 0, "KEY_D", 1);
  setSlotNumber(vm, 1, TERM_KEY_E);
  setAttribute(vm, 0, "KEY_E", 1);
  setSlotNumber(vm, 1, TERM_KEY_F);
  setAttribute(vm, 0, "KEY_F", 1);
  setSlotNumber(vm, 1, TERM_KEY_G);
  setAttribute(vm, 0, "KEY_G", 1);
  setSlotNumber(vm, 1, TERM_KEY_H);
  setAttribute(vm, 0, "KEY_H", 1);
  setSlotNumber(vm, 1, TERM_KEY_I);
  setAttribute(vm, 0, "KEY_I", 1);
  setSlotNumber(vm, 1, TERM_KEY_J);
  setAttribute(vm, 0, "KEY_J", 1);
  setSlotNumber(vm, 1, TERM_KEY_K);
  setAttribute(vm, 0, "KEY_K", 1);
  setSlotNumber(vm, 1, TERM_KEY_L);
  setAttribute(vm, 0, "KEY_L", 1);
  setSlotNumber(vm, 1, TERM_KEY_M);
  setAttribute(vm, 0, "KEY_M", 1);
  setSlotNumber(vm, 1, TERM_KEY_N);
  setAttribute(vm, 0, "KEY_N", 1);
  setSlotNumber(vm, 1, TERM_KEY_O);
  setAttribute(vm, 0, "KEY_O", 1);
  setSlotNumber(vm, 1, TERM_KEY_P);
  setAttribute(vm, 0, "KEY_P", 1);
  setSlotNumber(vm, 1, TERM_KEY_Q);
  setAttribute(vm, 0, "KEY_Q", 1);
  setSlotNumber(vm, 1, TERM_KEY_R);
  setAttribute(vm, 0, "KEY_R", 1);
  setSlotNumber(vm, 1, TERM_KEY_S);
  setAttribute(vm, 0, "KEY_S", 1);
  setSlotNumber(vm, 1, TERM_KEY_T);
  setAttribute(vm, 0, "KEY_T", 1);
  setSlotNumber(vm, 1, TERM_KEY_U);
  setAttribute(vm, 0, "KEY_U", 1);
  setSlotNumber(vm, 1, TERM_KEY_V);
  setAttribute(vm, 0, "KEY_V", 1);
  setSlotNumber(vm, 1, TERM_KEY_W);
  setAttribute(vm, 0, "KEY_W", 1);
  setSlotNumber(vm, 1, TERM_KEY_X);
  setAttribute(vm, 0, "KEY_X", 1);
  setSlotNumber(vm, 1, TERM_KEY_Y);
  setAttribute(vm, 0, "KEY_Y", 1);
  setSlotNumber(vm, 1, TERM_KEY_Z);
  setAttribute(vm, 0, "KEY_Z", 1);
  setSlotNumber(vm, 1, TERM_KEY_ESC);
  setAttribute(vm, 0, "KEY_ESC", 1);
  setSlotNumber(vm, 1, TERM_KEY_ENTER);
  setAttribute(vm, 0, "KEY_ENTER", 1);
  setSlotNumber(vm, 1, TERM_KEY_SPACE);
  setAttribute(vm, 0, "KEY_SPACE", 1);
  setSlotNumber(vm, 1, TERM_KEY_HOME);
  setAttribute(vm, 0, "KEY_HOME", 1);
  setSlotNumber(vm, 1, TERM_KEY_END);
  setAttribute(vm, 0, "KEY_END", 1);
  setSlotNumber(vm, 1, TERM_KEY_PAGEUP);
  setAttribute(vm, 0, "KEY_PAGEUP", 1);
  setSlotNumber(vm, 1, TERM_KEY_PAGEDOWN);
  setAttribute(vm, 0, "KEY_PAGEDOWN", 1);
  setSlotNumber(vm, 1, TERM_KEY_LEFT);
  setAttribute(vm, 0, "KEY_LEFT", 1);
  setSlotNumber(vm, 1, TERM_KEY_UP);
  setAttribute(vm, 0, "KEY_UP", 1);
  setSlotNumber(vm, 1, TERM_KEY_RIGHT);
  setAttribute(vm, 0, "KEY_RIGHT", 1);
  setSlotNumber(vm, 1, TERM_KEY_DOWN);
  setAttribute(vm, 0, "KEY_DOWN", 1);
  setSlotNumber(vm, 1, TERM_KEY_INSERT);
  setAttribute(vm, 0, "KEY_INSERT", 1);
  setSlotNumber(vm, 1, TERM_KEY_DELETE);
  setAttribute(vm, 0, "KEY_DELETE", 1);
  setSlotNumber(vm, 1, TERM_KEY_BACKSPACE);
  setAttribute(vm, 0, "KEY_BACKSPACE", 1);
  setSlotNumber(vm, 1, TERM_KEY_TAB);
  setAttribute(vm, 0, "KEY_TAB", 1);
  setSlotNumber(vm, 1, TERM_KEY_F1);
  setAttribute(vm, 0, "KEY_F1", 1);
  setSlotNumber(vm, 1, TERM_KEY_F2);
  setAttribute(vm, 0, "KEY_F2", 1);
  setSlotNumber(vm, 1, TERM_KEY_F3);
  setAttribute(vm, 0, "KEY_F3", 1);
  setSlotNumber(vm, 1, TERM_KEY_F4);
  setAttribute(vm, 0, "KEY_F4", 1);
  setSlotNumber(vm, 1, TERM_KEY_F5);
  setAttribute(vm, 0, "KEY_F5", 1);
  setSlotNumber(vm, 1, TERM_KEY_F6);
  setAttribute(vm, 0, "KEY_F6", 1);
  setSlotNumber(vm, 1, TERM_KEY_F7);
  setAttribute(vm, 0, "KEY_F7", 1);
  setSlotNumber(vm, 1, TERM_KEY_F8);
  setAttribute(vm, 0, "KEY_F8", 1);
  setSlotNumber(vm, 1, TERM_KEY_F9);
  setAttribute(vm, 0, "KEY_F9", 1);
  setSlotNumber(vm, 1, TERM_KEY_F10);
  setAttribute(vm, 0, "KEY_F10", 1);
  setSlotNumber(vm, 1, TERM_KEY_F11);
  setAttribute(vm, 0, "KEY_F11", 1);
  setSlotNumber(vm, 1, TERM_KEY_F12);
  setAttribute(vm, 0, "KEY_F12", 1);

  setSlotNumber(vm, 1, TERM_MB_UNKNOWN);
  setAttribute(vm, 0, "BUTTON_UNKNOWN", 1);
  setSlotNumber(vm, 1, TERM_MB_LEFT);
  setAttribute(vm, 0, "BUTTON_LEFT", 1);
  setSlotNumber(vm, 1, TERM_MB_MIDDLE);
  setAttribute(vm, 0, "BUTTON_MIDDLE", 1);
  setSlotNumber(vm, 1, TERM_MB_RIGHT);
  setAttribute(vm, 0, "BUTTON_RIGHT", 1);

  setSlotNumber(vm, 1, TERM_MD_NONE);
  setAttribute(vm, 0, "MD_NONE", 1);
  setSlotNumber(vm, 1, TERM_MD_CTRL);
  setAttribute(vm, 0, "MD_CTRL", 1);
  setSlotNumber(vm, 1, TERM_MD_ALT);
  setAttribute(vm, 0, "MD_ALT", 1);
  setSlotNumber(vm, 1, TERM_MD_SHIFT);
  setAttribute(vm, 0, "MD_SHIFT", 1);

  setSlotNumber(vm, 1, TERM_ET_UNKNOWN);
  setAttribute(vm, 0, "EVENT_UNKNOWN", 1);
  setSlotNumber(vm, 1, TERM_ET_KEY_DOWN);
  setAttribute(vm, 0, "EVENT_KEY_DOWN", 1);
  setSlotNumber(vm, 1, TERM_ET_RESIZE);
  setAttribute(vm, 0, "EVENT_RESIZE", 1);
  setSlotNumber(vm, 1, TERM_ET_DOUBLE_CLICK);
  setAttribute(vm, 0, "EVENT_DOUBLE_CLICK", 1);
  setSlotNumber(vm, 1, TERM_ET_MOUSE_DOWN);
  setAttribute(vm, 0, "EVENT_MOUSE_DOWN", 1);
  setSlotNumber(vm, 1, TERM_ET_MOUSE_UP);
  setAttribute(vm, 0, "EVENT_MOUSE_UP", 1);
  setSlotNumber(vm, 1, TERM_ET_MOUSE_MOVE);
  setAttribute(vm, 0, "EVENT_MOUSE_MOVE", 1);
  setSlotNumber(vm, 1, TERM_ET_MOUSE_DRAG);
  setAttribute(vm, 0, "EVENT_MOUSE_DRAG", 1);
  setSlotNumber(vm, 1, TERM_ET_MOUSE_SCROLL);
  setAttribute(vm, 0, "EVENT_MOUSE_SCROLL", 1);
}

saynaa_function(_termInit, "term.init(capture_events:Bool) -> Null",
                "Initialize terminal with raw mode for tui applications, set "
                "[capture_events] true to enable event handling.") {
  bool capture_events;
  if (!ValidateSlotBool(vm, 1, &capture_events))
    return;
  term_init(capture_events);
  _termCtxInit();
}

saynaa_function(_termCleanup, "term.cleanup() -> Null",
                "Cleanup and resotre the last terminal state.") {
  term_cleanup();
  _sTermCtx.done = false;
  _termCtxFree(vm);
}

saynaa_function(_termFlush, "term.flush() -> Null", "Flush the internal buffer to stdout.") {
  if (_sTermCtx.count > 0) {
    fwrite(_sTermCtx.data, 1, _sTermCtx.count, stdout);
    fflush(stdout);
    _sTermCtx.count = 0;
  }
}

saynaa_function(_termStop, "term.stop() -> Null", "Stop the main event loop.") {
  _sTermCtx.done = true;
}

saynaa_function(_termWrite, "term.write(s:String) -> Null", "Write string to buffer.") {
  const char* s;
  uint32_t len;
  if (!ValidateSlotString(vm, 1, &s, &len))
    return;
  _termCtxWrite(vm, s, len);
}

// -----------------------------------------------------------------------------
// ANSI HELPERS
// -----------------------------------------------------------------------------

saynaa_function(_termSetTitle, "term.set_title(title:String) -> Null", "Set terminal title.") {
  const char* title;
  if (!ValidateSlotString(vm, 1, &title, NULL))
    return;
  // This one writes directly to stdout in original, but we can write to stdout too
  fprintf(stdout, "\x1b]0;%s\x07", title);
  // No flush? .sa uses io.write which might be buffered or not.
  // Let's flush to ensure title sets.
  fflush(stdout);
}

saynaa_function(_termHideCursor, "term.hide_cursor() -> Null", "Hide cursor.") {
  fprintf(stdout, "\x1b[?25l");
  fflush(stdout);
}

saynaa_function(_termShowCursor, "term.show_cursor() -> Null", "Show cursor.") {
  fprintf(stdout, "\x1b[?25h");
  fflush(stdout);
}

saynaa_function(_termSetPosition, "term.setposition(x:Number, y:Number) -> Null",
                "Set cursor position (0-based).") {
  double x, y;
  int argc = GetArgc(vm);
  if (argc == 2) {
    if (!ValidateSlotNumber(vm, 1, &x))
      return;
    if (!ValidateSlotNumber(vm, 2, &y))
      return;
  } else if (argc == 1) { // Vector or map support?
    // For simplicity matching .sa: setposition(x, y)
    // But wait, existing C supported vector. Let's keep vector support but write to BUFFER.
    if (IS_NUM(SLOT(1))) { /* Is number, assume 2 args */
      if (!ValidateSlotNumber(vm, 1, &x))
        return;
      if (!ValidateSlotNumber(vm, 2, &y))
        return;
    } else {
      reserveSlots(vm, 3);
      if (!GetAttribute(vm, 1, "x", 2))
        return;
      if (!ValidateSlotNumber(vm, 2, &x))
        return;
      if (!GetAttribute(vm, 1, "y", 2))
        return;
      if (!ValidateSlotNumber(vm, 2, &y))
        return;
    }

  } else {
    SetRuntimeError(vm, "Invalid arguments.");
    return;
  }
  _termCtxWriteFmt(vm, "\x1b[%d;%dH", (int) y + 1, (int) x + 1);
}

saynaa_function(_termStartBoxChar, "term.start_boxchar() -> Null", "") {
  _termCtxWrite(vm, "\x1b(0", 3);
}
saynaa_function(_termEndBoxChar, "term.end_boxchar() -> Null", "") {
  _termCtxWrite(vm, "\x1b(B", 3);
}
saynaa_function(_termClear, "term.clear() -> Null", "Clear screen.") {
  _termCtxWrite(vm, "\x1b[H\x1b[J", 6);
}
saynaa_function(_termClearEol, "term.clear_eol() -> Null", "Clear to end of line.") {
  _termCtxWrite(vm, "\x1b[K", 3);
}
saynaa_function(_termClearEof, "term.clear_eof() -> Null", "Clear to end of file (screen).") {
  _termCtxWrite(vm, "\x1b[J", 3);
}
saynaa_function(_termReset, "term.reset() -> Null", "Reset attributes.") {
  _termCtxWrite(vm, "\x1b[0m", 4);
}

static bool _isValidColor(VM* vm, double r, double g, double b) {
  return true; // Simple check
}

saynaa_function(_termStartColor, "term.start_color(r, g, b) -> Null", "") {
  int argc = GetArgc(vm);
  if (argc == 1) {
    double c;
    if (!ValidateSlotNumber(vm, 1, &c))
      return;
    _termCtxWriteFmt(vm, "\x1b[38;5;%dm", (int) c);
    return;
  }
  double r, g, b;
  if (!ValidateSlotNumber(vm, 1, &r))
    return;
  if (!ValidateSlotNumber(vm, 2, &g))
    return;
  if (!ValidateSlotNumber(vm, 3, &b))
    return;
  _termCtxWriteFmt(vm, "\x1b[38;2;%d;%d;%dm", (int) r, (int) g, (int) b);
}
saynaa_function(_termEndColor, "term.end_color() -> Null", "") {
  _termCtxWrite(vm, "\x1b[39m", 5);
}
saynaa_function(_termStartBg, "term.start_bg(r, g, b) -> Null", "") {
  int argc = GetArgc(vm);
  if (argc == 1) {
    double c;
    if (!ValidateSlotNumber(vm, 1, &c))
      return;
    _termCtxWriteFmt(vm, "\x1b[48;5;%dm", (int) c);
    return;
  }
  double r, g, b;
  if (!ValidateSlotNumber(vm, 1, &r))
    return;
  if (!ValidateSlotNumber(vm, 2, &g))
    return;
  if (!ValidateSlotNumber(vm, 3, &b))
    return;
  _termCtxWriteFmt(vm, "\x1b[48;2;%d;%d;%dm", (int) r, (int) g, (int) b);
}
saynaa_function(_termEndBg, "term.end_bg() -> Null", "") {
  _termCtxWrite(vm, "\x1b[49m", 5);
}

// Styling wrappers
#define TERM_GEN_STYLE(name, start_code, end_code) \
  saynaa_function(_termStart##name, "term.start_" #name "() -> Null", "") { \
    _termCtxWrite(vm, "\x1b[" #start_code "m", 0); \
    _ctx.count += strlen("\x1b[" #start_code "m"); \
  } \
  saynaa_function(_termEnd##name, "term.end_" #name "() -> Null", "") { \
    _termCtxWrite(vm, "\x1b[" #end_code "m", 0); \
    _ctx.count += strlen("\x1b[" #end_code "m"); \
  }
// Wait, _termCtxWrite uses explicit len but logic in macro is broken.
// Let's do implicit length if 0? No, unsafe. using Fmt is easier.
#undef TERM_GEN_STYLE

#define TERM_STYLE(name, code) \
  saynaa_function(_termStart##name, "term.start_" #name "() -> Null", "") { \
    _termCtxWrite(vm, "\x1b[" #code "m", sizeof("\x1b[" #code "m") - 1); \
  }

TERM_STYLE(Bold, 1)
TERM_STYLE(Dim, 2)
TERM_STYLE(Italic, 3)
TERM_STYLE(Underline, 4)
TERM_STYLE(Inverse, 7)
TERM_STYLE(Hidden, 8)
TERM_STYLE(Strikethrough, 9)

saynaa_function(_termEndBold, "term.end_bold() -> Null", "") {
  _termCtxWrite(vm, "\x1b[22m", 5);
}
saynaa_function(_termEndDim, "term.end_dim() -> Null", "") {
  _termCtxWrite(vm, "\x1b[22m", 5);
}
saynaa_function(_termEndItalic, "term.end_italic() -> Null", "") {
  _termCtxWrite(vm, "\x1b[23m", 5);
}
saynaa_function(_termEndUnderline, "term.end_underline() -> Null", "") {
  _termCtxWrite(vm, "\x1b[24m", 5);
}
saynaa_function(_termEndInverse, "term.end_inverse() -> Null", "") {
  _termCtxWrite(vm, "\x1b[27m", 5);
}
saynaa_function(_termEndHidden, "term.end_hidden() -> Null", "") {
  _termCtxWrite(vm, "\x1b[28m", 5);
}
saynaa_function(_termEndStrikethrough, "term.end_strikethrough() -> Null", "") {
  _termCtxWrite(vm, "\x1b[29m", 5);
}

// Colors
#define TERM_COLOR(name, val) \
  saynaa_function(_termColor##name, "term.start_color_" #name "() -> Null", "") { \
    _termCtxWrite(vm, "\x1b[3" #val "m", 5); \
  } \
  saynaa_function(_termColorEnd##name, "term.end_color_" #name "() -> Null", "") { \
    _termCtxWrite(vm, "\x1b[4" #val "m", 5); \
  }

TERM_COLOR(Black, 0)
TERM_COLOR(Red, 1)
TERM_COLOR(Green, 2)
TERM_COLOR(Yellow, 3)
TERM_COLOR(Blue, 4)
TERM_COLOR(Magenta, 5)
TERM_COLOR(Cyan, 6)
TERM_COLOR(White, 7)

saynaa_function(_termColorDefault, "term.start_color_default() -> Null", "") {
  _termCtxWrite(vm, "\x1b[39m", 5);
}
saynaa_function(_termEndColorDefault, "term.end_color_default() -> Null", "") {
  _termCtxWrite(vm, "\x1b[49m", 5);
}

// -----------------------------------------------------------------------------
// MAIN LOOP
// -----------------------------------------------------------------------------

static void _callConfigFn(VM* vm, Handle* config, const char* name, int argc) {
  // config is implicitly at slot 0 (if we pushed it) or we need to put it there
  // We need to resolve function from config instance
  reserveSlots(vm, argc + 2); // 0: config, 1..argc: args
  setSlotHandle(vm, 0, config);

  // Get property
  if (!GetAttribute(vm, 0, name, argc + 1))
    return; // Value at argc+1

  Var fnVar = SLOT(argc + 1);
  if (IS_NULL(fnVar))
    return;

  if (IS_CLOSURE(fnVar)) {
    // Call it. Arguments are at 1..argc
    // vmCallMethod requires 'thiz'. If it's a method of config, thiz is config.
    // If it's a closure assigned to a field, thiz depends.
    // Assuming simple closure.
    // We'll use vmCallMethod(vm, VAR_UNDEFINED, AS_CLOSURE(fnVar), argc, argc >
    // 0 ? &SLOT(1) : NULL, &ret); But SLOT(1) is start of args.
    Var ret;
    vmCallMethod(vm, VAR_UNDEFINED, AS_CLOSURE(fnVar), argc,
                 argc > 0 ? &SLOT(1) : NULL, &ret);
  }
}

saynaa_function(_termRun, "term.run(config:Config) -> Null", "Run the main loop.") {
  reserveSlots(vm, 3);
  // Expect config at slot 1
  setSlotHandle(vm, 2, _cls_term_config);
  if (!ValidateSlotInstanceOf(vm, 1, 2))
    return;
  // Or just Duck typing? .sa code expect config object with fields.
  // We can just use the handle.

  Handle* config = vmNewHandle(vm, SLOT(1));

  // Read config
  setSlotHandle(vm, 0, config);

  bool capture_events = false;
  if (GetAttribute(vm, 0, "capture_events", 1))
    ValidateSlotBool(vm, 1, &capture_events);

  bool hide_cursor = false;
  if (GetAttribute(vm, 0, "hide_cursor", 1))
    ValidateSlotBool(vm, 1, &hide_cursor);

  bool new_buffer = false;
  if (GetAttribute(vm, 0, "new_buffer", 1))
    ValidateSlotBool(vm, 1, &new_buffer);

  double fps = 60.0;
  if (GetAttribute(vm, 0, "fps", 1))
    ValidateSlotNumber(vm, 1, &fps);

  _termCtxInit();
  term_init(capture_events);

  if (new_buffer)
    term_new_screen_buffer();
  if (hide_cursor) {
    fprintf(stdout, "\x1b[?25l");
    fflush(stdout);
  }

  // init_fn
  _callConfigFn(vm, config, "init_fn", 0);

  reserveSlots(vm, 5);
  // Ensure stack pointer covers the reserved slots, and initialize them to NULL
  // so the GC can trace them properly. This is critical because SLOT(3) will
  // hold the event object which must be kept alive.
  while (vm->fiber->sp < vm->fiber->ret + 5) {
    *vm->fiber->sp = VAR_NULL;
    vm->fiber->sp++;
  }
  // Slot layout:
  // 0: temp / config (in callConfigFn)
  // 1: passed to event_fn as arg
  // 2: used by callConfigFn to store the function
  // 3: Event instance

  // Create Saynaa Event instance
  setSlotHandle(vm, 3, _cls_term_event);
  NewInstance(vm, 3, 3, 0, 0);
  // Now slot 3 has Event instance.
  // We also need the native event pointer.
  term_Event* native_event = GetSlotNativeInstance(vm, 3);

  _sTermCtx.done = false;

  while (!_sTermCtx.done) {
    double start = (double) nanotime() / 1e9;

    while (term_read_event(native_event)) {
      // Dispatch implicit resize
      if (native_event->type == TERM_ET_RESIZE) {
        if (hide_cursor) {
          fprintf(stdout, "\x1b[?25l");
          fflush(stdout);
        }
      }

      // Call event_fn(event)
      // event is at slot 3.
      // We need to pass it to _callConfigFn
      SLOT(1) = SLOT(3); // Copy event to slot 1 (arg 0 of fn)

      _callConfigFn(vm, config, "event_fn", 1);

      if (_sTermCtx.done)
        break;
    }

    if (_sTermCtx.done)
      break;

    _callConfigFn(vm, config, "frame_fn", 0);

    double end = (double) nanotime() / 1e9;
    double et = end - start;
    double ft = 1.0 / fps;

    if (ft > et) {
#if defined(_WIN32)
      DWORD sleep_ms = (DWORD) ((ft - et) * 1e3);
      if (sleep_ms > 0)
        Sleep(sleep_ms);
#else
      useconds_t sleep_us = (useconds_t) ((ft - et) * 1e6);
      if (sleep_us > 0)
        usleep(sleep_us);
#endif
    }
  }

  // Cleanup
  _callConfigFn(vm, config, "cleanup_fn", 0);
  if (hide_cursor) {
    fprintf(stdout, "\x1b[?25h");
    fflush(stdout);
  }
  if (new_buffer)
    term_restore_screen_buffer();

  term_cleanup();
  _termCtxFree(vm);

  releaseHandle(vm, config);
}

// Config class methods
saynaa_function(_termConfigInit, "Config._init() -> Null", "Initialize config.") {
  // Set default values to 'this'
  // this.title = null, etc.
  // This is called when Config() is instantiated.
  Var thiz = SLOT(0);
  setAttribute(vm, 0, "title", 0); // null
  setSlotNumber(vm, 1, 60);
  setAttribute(vm, 0, "fps", 1);
  setSlotBool(vm, 1, false);
  setAttribute(vm, 0, "hide_cursor", 1);
  setSlotBool(vm, 1, false);
  setAttribute(vm, 0, "capture_events", 1);
  setSlotBool(vm, 1, false);
  setAttribute(vm, 0, "new_buffer", 1);
  SLOT(1) = VAR_NULL;
  setAttribute(vm, 0, "init_fn", 1);
  setAttribute(vm, 0, "event_fn", 1);
  setAttribute(vm, 0, "frame_fn", 1);
  setAttribute(vm, 0, "cleanup_fn", 1);
}

saynaa_function(_termIsatty, "term.isatty() -> Bool",
                "Returns true if both stdin and stdout are tty.") {
  setSlotBool(vm, 0, term_isatty());
}

saynaa_function(_termNewScreenBuffer, "term.new_screen_buffer() -> Null",
                "Switch to an alternative screen buffer.") {
  term_new_screen_buffer();
}

saynaa_function(_termRestoreScreenBuffer, "term.restore_screen_buffer() -> Null",
                "Restore the alternative buffer which was created with "
                "term.new_screen_buffer()") {
  term_restore_screen_buffer();
}

// Helper function removed

saynaa_function(_termGetSize, "term.getsize() -> types.Vector", "Returns the screen size.") {
  reserveSlots(vm, 4);
  term_Vec size = term_getsize();
  _setSlotVector(vm, 0, 1, size.x, size.y);
}

saynaa_function(_termGetPosition, "term.getposition() -> types.Vector", "Returns the cursor position in the screen on a zero based coordinate.") {
  reserveSlots(vm, 4);
  term_Vec pos = term_getposition();
  _setSlotVector(vm, 0, 1, pos.x, pos.y);
}

saynaa_function(_termReadEvent, "term.read_event(event:term.Event) -> Bool",
                "Read an event and update the argument [event] and return true."
                "If no event was read it'll return false.") {
  reserveSlots(vm, 3);
  setSlotHandle(vm, 2, _cls_term_event);
  if (!ValidateSlotInstanceOf(vm, 1, 2))
    return;

  term_Event* event = GetSlotNativeInstance(vm, 1);
  setSlotBool(vm, 0, term_read_event(event));
}

saynaa_function(
    _termBinaryMode, "term.binary_mode() -> Null",
    "On windows it'll set stdout to binary mode, on other platforms this "
    "function won't make make any difference.") {
#ifdef _WIN32
  (void) _setmode(_fileno(stdout), _O_BINARY);
#endif
}

/*****************************************************************************/
/* MODULE REGISTER                                                           */
/*****************************************************************************/

void registerModuleTerm(VM* vm) {
  Handle* term = NewModule(vm, "term");

  _registerEnums(vm, term);
  REGISTER_FN(term, "init", _termInit, 1);
  REGISTER_FN(term, "cleanup", _termCleanup, 0);
  REGISTER_FN(term, "isatty", _termIsatty, 0);
  REGISTER_FN(term, "new_screen_buffer", _termNewScreenBuffer, 0);
  REGISTER_FN(term, "restore_screen_buffer", _termRestoreScreenBuffer, 0);
  REGISTER_FN(term, "getsize", _termGetSize, 0);
  REGISTER_FN(term, "getposition", _termGetPosition, 0);
  REGISTER_FN(term, "setposition", _termSetPosition, -1);
  REGISTER_FN(term, "read_event", _termReadEvent, 1);
  REGISTER_FN(term, "binary_mode", _termBinaryMode, 0);

  // Buffer APIs
  REGISTER_FN(term, "flush", _termFlush, 0);
  REGISTER_FN(term, "write", _termWrite, 1);
  REGISTER_FN(term, "stop", _termStop, 0);
  REGISTER_FN(term, "set_title", _termSetTitle, 1);
  REGISTER_FN(term, "hide_cursor", _termHideCursor, 0);
  REGISTER_FN(term, "show_cursor", _termShowCursor, 0);
  REGISTER_FN(term, "clear", _termClear, 0);
  REGISTER_FN(term, "clear_eol", _termClearEol, 0);
  REGISTER_FN(term, "clear_eof", _termClearEof, 0);
  REGISTER_FN(term, "reset", _termReset, 0);
  REGISTER_FN(term, "start_boxchar", _termStartBoxChar, 0);
  REGISTER_FN(term, "end_boxchar", _termEndBoxChar, 0);

  REGISTER_FN(term, "start_color", _termStartColor, -1);
  REGISTER_FN(term, "end_color", _termEndColor, 0);
  REGISTER_FN(term, "start_bg", _termStartBg, -1);
  REGISTER_FN(term, "end_bg", _termEndBg, 0);

  // Styles
  REGISTER_FN(term, "start_bold", _termStartBold, 0);
  REGISTER_FN(term, "end_bold", _termEndBold, 0);
  REGISTER_FN(term, "start_dim", _termStartDim, 0);
  REGISTER_FN(term, "end_dim", _termEndDim, 0);
  REGISTER_FN(term, "start_italic", _termStartItalic, 0);
  REGISTER_FN(term, "end_italic", _termEndItalic, 0);
  REGISTER_FN(term, "start_underline", _termStartUnderline, 0);
  REGISTER_FN(term, "end_underline", _termEndUnderline, 0);
  REGISTER_FN(term, "start_inverse", _termStartInverse, 0);
  REGISTER_FN(term, "end_inverse", _termEndInverse, 0);
  REGISTER_FN(term, "start_hidden", _termStartHidden, 0);
  REGISTER_FN(term, "end_hidden", _termEndHidden, 0);
  REGISTER_FN(term, "start_strikethrough", _termStartStrikethrough, 0);
  REGISTER_FN(term, "end_strikethrough", _termEndStrikethrough, 0);

  // Colors helpers
  REGISTER_FN(term, "start_color_black", _termColorBlack, 0);
  REGISTER_FN(term, "end_color_black", _termColorEndBlack, 0);
  REGISTER_FN(term, "start_color_red", _termColorRed, 0);
  REGISTER_FN(term, "end_color_red", _termColorEndRed, 0);
  REGISTER_FN(term, "start_color_green", _termColorGreen, 0);
  REGISTER_FN(term, "end_color_green", _termColorEndGreen, 0);
  REGISTER_FN(term, "start_color_yellow", _termColorYellow, 0);
  REGISTER_FN(term, "end_color_yellow", _termColorEndYellow, 0);
  REGISTER_FN(term, "start_color_blue", _termColorBlue, 0);
  REGISTER_FN(term, "end_color_blue", _termColorEndBlue, 0);
  REGISTER_FN(term, "start_color_magenta", _termColorMagenta, 0);
  REGISTER_FN(term, "end_color_magenta", _termColorEndMagenta, 0);
  REGISTER_FN(term, "start_color_cyan", _termColorCyan, 0);
  REGISTER_FN(term, "end_color_cyan", _termColorEndCyan, 0);
  REGISTER_FN(term, "start_color_white", _termColorWhite, 0);
  REGISTER_FN(term, "end_color_white", _termColorEndWhite, 0);
  REGISTER_FN(term, "start_color_functionault", _termColorDefault, 0);
  REGISTER_FN(term, "end_color_functionault", _termEndColorDefault, 0);

  REGISTER_FN(term, "run", _termRun, 1);

  _cls_term_event = NewClass(vm, "Event", NULL, term, _termEventNew, _termEventDelete,
                             "The terminal event type, that'll be used at "
                             "term.read_event function to "
                             "fetch events.");
  ADD_METHOD(_cls_term_event, "_getter", _termEventGetter, 1);

  _cls_term_config = NewClass(vm, "Config", NULL, term, NULL, NULL, "Configuration for term.run.");
  ADD_METHOD(_cls_term_config, "_init", _termConfigInit, 0);

  registerModule(vm, term);
  releaseHandle(vm, term);
}

void cleanupModuleTerm(VM* vm) {
  if (_cls_term_event)
    releaseHandle(vm, _cls_term_event);
  if (_cls_term_config)
    releaseHandle(vm, _cls_term_config);
  _termCtxFree(vm);
}
