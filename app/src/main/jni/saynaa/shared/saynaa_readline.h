/*
 * Copyright (c) 2022-2026 Mohamed Abdifatah. All rights reserved.
 * Distributed Under The MIT License
 */

#pragma once

#include <stdio.h>

typedef struct VM VM;

#if defined(__linux) && defined(READLINE)

// saynaa_readline: how to show a prompt and then read a line from
// the standard input.
char* saynaa_readline(VM* vm, const char* listening);

// saynaa_saveline: how to "save" a input string in a "history".
void saynaa_saveline(const char* input);

#endif
