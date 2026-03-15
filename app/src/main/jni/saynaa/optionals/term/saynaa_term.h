/*
 * Copyright (c) 2022-2026 Mohamed Abdifatah. All rights reserved.
 * Distributed Under The MIT License
 */

#pragma once

#include <stdbool.h>

/*
 * A generic Vector type to pass size, position data around.
 */
typedef struct {
  int x;
  int y;
} term_Vec;

/* A macro function to create a vector. */
#define term_vec(x, y) \
  (term_Vec) { \
    (x), (y) \
  }

/*
 * List of keycodes.
 */
typedef enum {

  TERM_KEY_UNKNOWN = 0,

  TERM_KEY_0 = '0',
  TERM_KEY_1 = '1',
  TERM_KEY_2 = '2',
  TERM_KEY_3 = '3',
  TERM_KEY_4 = '4',
  TERM_KEY_5 = '5',
  TERM_KEY_6 = '6',
  TERM_KEY_7 = '7',
  TERM_KEY_8 = '8',
  TERM_KEY_9 = '9',

  TERM_KEY_A = 'A',
  TERM_KEY_B = 'B',
  TERM_KEY_C = 'C',
  TERM_KEY_D = 'D',
  TERM_KEY_E = 'E',
  TERM_KEY_F = 'F',
  TERM_KEY_G = 'G',
  TERM_KEY_H = 'H',
  TERM_KEY_I = 'I',
  TERM_KEY_J = 'J',
  TERM_KEY_K = 'K',
  TERM_KEY_L = 'L',
  TERM_KEY_M = 'M',
  TERM_KEY_N = 'N',
  TERM_KEY_O = 'O',
  TERM_KEY_P = 'P',
  TERM_KEY_Q = 'Q',
  TERM_KEY_R = 'R',
  TERM_KEY_S = 'S',
  TERM_KEY_T = 'T',
  TERM_KEY_U = 'U',
  TERM_KEY_V = 'V',
  TERM_KEY_W = 'W',
  TERM_KEY_X = 'X',
  TERM_KEY_Y = 'Y',
  TERM_KEY_Z = 'Z',

  TERM_KEY_ESC,
  TERM_KEY_ENTER,
  TERM_KEY_SPACE,
  TERM_KEY_HOME,
  TERM_KEY_END,
  TERM_KEY_PAGEUP,
  TERM_KEY_PAGEDOWN,
  TERM_KEY_LEFT,
  TERM_KEY_UP,
  TERM_KEY_RIGHT,
  TERM_KEY_DOWN,
  TERM_KEY_INSERT,
  TERM_KEY_DELETE,
  TERM_KEY_BACKSPACE,
  TERM_KEY_TAB,

  TERM_KEY_F1,
  TERM_KEY_F2,
  TERM_KEY_F3,
  TERM_KEY_F4,
  TERM_KEY_F5,
  TERM_KEY_F6,
  TERM_KEY_F7,
  TERM_KEY_F8,
  TERM_KEY_F9,
  TERM_KEY_F10,
  TERM_KEY_F11,
  TERM_KEY_F12,

} term_KeyCode;

/*
 * Event type.
 */
typedef enum {
  TERM_ET_UNKNOWN = 0,
  TERM_ET_KEY_DOWN,
  TERM_ET_DOUBLE_CLICK,
  TERM_ET_MOUSE_DOWN,
  TERM_ET_MOUSE_UP,
  TERM_ET_MOUSE_MOVE,
  TERM_ET_MOUSE_DRAG,
  TERM_ET_MOUSE_SCROLL,
  TERM_ET_RESIZE,
} term_EventType;

/*
 * Key event modifier flags.
 */
typedef enum {
  TERM_MD_NONE = 0x0,
  TERM_MD_CTRL = (1 << 1),
  TERM_MD_ALT = (1 << 2),
  TERM_MD_SHIFT = (1 << 3),
} term_Modifiers;

/*
 * Mouse button event's button index.
 */
typedef enum {
  TERM_MB_UNKNOWN = 0,
  TERM_MB_LEFT = 1,
  TERM_MB_MIDDLE = 2,
  TERM_MB_RIGHT = 3,
} term_MouseBtn;

/*
 * Key event.
 */
typedef struct {
  term_KeyCode code;
  char ascii;
  term_Modifiers modifiers;
} term_EventKey;

/*
 * Mouse event.
 */
typedef struct {
  term_MouseBtn button;
  term_Vec pos;
  bool scroll; /* If true down otherwise up. */
  term_Modifiers modifiers;
} term_EventMouse;

/*
 * Event.
 */
typedef struct {
  term_EventType type;
  union {
    term_EventKey key;
    term_EventMouse mouse;
    term_Vec resize;
  };
} term_Event;

/* Returns true if both stdin and stdout are tty like device. */
bool term_isatty();

/* Initialize the terminal. */
void term_init(bool capture_events);

/* Cleans up all the internals. */
void term_cleanup(void);

/* Reads an event. */
bool term_read_event(term_Event* event);

/* Create an alternative screen buffer. */
void term_new_screen_buffer();

/* Restore the screen buffer. */
void term_restore_screen_buffer();

/* Returns the screen size. */
term_Vec term_getsize();

/* Returns the cursor position. */
term_Vec term_getposition();

/* Sets the cursor position. */
void term_setposition(term_Vec pos);
