#pragma once
#include "string.h"
#include <stdbool.h>

/*
 * To add a new language:
 *   1. Add a row here: X(ENUM, id, value)
 *      ENUM  = C enum name / array prefix (e.g. EN_GB)
 *      id    = lowercase id / function suffix (e.g. en_GB)
 *      value = integer sent from JS, must match pebble-js-app.js langs map
 *   2. Add #include "strings-id.h" in num2words.c
 *   3. Create src/c/strings-id.h and strings-id.c
 *   4. Add the language to pebble-js-app.js and config-html.js
 */
#define ALL_LANGUAGES  \
  X(CA,    0x0)        \
  X(DE,    0x1)        \
  X(EN_GB, 0x2)        \
  X(EN_US, 0x3)        \
  X(ES,    0x4)        \
  X(FR,    0x5)        \
  X(NO,    0x6)        \
  X(SV,    0x7)        \
  X(NL,    0x8)        \
  X(PT,    0x9)

typedef enum {
  #define X(ENUM, VAL) ENUM = VAL,
  ALL_LANGUAGES
  #undef X
} Language;

void time_to_words(Language lang, int hours, int minutes, char* words, size_t length);
void date_to_words(Language lang, int day, int date, int month, char* words, size_t length);
const char* get_date_suffix(Language lang, int date);

char * itoa10(int value, char *result);
