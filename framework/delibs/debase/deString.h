#ifndef _DESTRING_H
#define _DESTRING_H
/*-------------------------------------------------------------------------
 * drawElements Base Portability Library
 * -------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Basic string operations.
 *//*--------------------------------------------------------------------*/

#include "deDefs.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

DE_BEGIN_EXTERN_C

uint32_t deStringHash(const char *str);
uint32_t deStringHashLeading(const char *str, int numLeadingChars);
bool deStringEqual(const char *a, const char *b);
bool deStringBeginsWith(const char *str, const char *leading);

uint32_t deMemoryHash(const void *ptr, size_t numBytes);
bool deMemoryEqual(const void *ptr, const void *cmp, size_t numBytes);

int32_t deVsprintf(char *string, size_t size, const char *format, va_list args);
int32_t deSprintf(char *string, size_t size, const char *format, ...) DE_PRINTF_FUNC_ATTR(3, 4);
char *deStrcpy(char *dst, size_t size, const char *src);
char *deStrcat(char *s1, size_t size, const char *s2);
size_t deStrnlen(const char *string, size_t maxSize);

DE_END_EXTERN_C

#endif /* _DESTRING_H */
