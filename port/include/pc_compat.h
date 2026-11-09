// Force-included into every translation unit of the PC port (-include pc_compat.h).
//
// The decompiled sources are written so the original GCC 2.95 / CodeWarrior compilers reproduce the
// shipped bytes. Most of what that takes is meaningless on a PC compiler and is removed here:
//
//   asm("");                       scheduling barriers                  -> nothing
//   register T x asm("r9");        register pins                        -> plain variable
//   asm(".section ...");           section / alignment directives        -> nothing
//   asm(".comm common_st1_0,...")  REL COMMON placeholders (unreferenced) -> nothing
//   T x asm("name");               symbol renames                       -> default name
//
// Real PowerPC code in asm() (paired-single loads, frsqrte, mtspr, hand-written memset/Yaz0) would be
// silently dropped by the same macro, so the units that contain it are excluded from the build until
// they have C replacements (port/CMakeLists.txt, PC_ASM_UNITS). Headers with real asm use
// #ifdef TARGET_PC.
#pragma once

#ifndef TARGET_PC
#define TARGET_PC 1
#endif

#define asm(...)
#define __asm__(...)

// The C runtime's C++ headers add const overloads (const char* strstr(const char*, ...)); the game
// was written against C's char* versions. Give C++ code the C semantics.
#ifdef __cplusplus
#include <string.h>
#define strstr(s, f) ((char*)::strstr((s), (f)))
#define strchr(s, c) ((char*)::strchr((s), (c)))
#define strrchr(s, c) ((char*)::strrchr((s), (c)))
#define strpbrk(s, a) ((char*)::strpbrk((s), (a)))
#define memchr(s, c, n) ((void*)::memchr((s), (c), (n)))
#endif

// GCC 2.95 accepted `register` everywhere; C++17 removed it.
#if defined(__cplusplus) && __cplusplus >= 201703L
#define register
#endif
