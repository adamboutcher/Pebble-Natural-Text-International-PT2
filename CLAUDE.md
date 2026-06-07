# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

Requires the Pebble SDK installed externally (not in this repo).

```bash
./waf configure
./waf build
```

Output goes to `build/`. The wscript at the root drives the build; it compiles `src/c/*.c` and bundles the JS from `src/pkjs/`.

## Architecture

### Display constraints (critical)

`TextWatch.c` splits a time string into up to 4 display lines. The constants that govern this:

```c
#define NUM_LINES      4
#define LINE_LENGTH    7   // max chars per line
#define BUFFER_SIZE    9   // LINE_LENGTH + 2
```

Each line holds at most **8 bytes of content** (BUFFER_SIZE - 1). Because the font is UTF-8, multi-byte characters (ü, é, ö, å, ñ, etc.) each count as **2 bytes** toward the 8-byte limit. Any single word that would exceed 8 bytes must be split with an embedded space in the source array so the line parser treats it as two tokens.

Word-combining logic: adjacent words are merged onto one line only when the first word is **< 6 bytes**, the combined span is **≤ 7 bytes**, and **neither word is bold**.

### Bold marker

A `*` prefix in a time string marks that word as bold. The `*` is consumed by the parser and not displayed. Example: `"twenty four past *seven"` → "seven" is bold.

### Time generation

`src/c/num2words.c` contains a `SpeakFormat` struct table that drives all language time output:

```c
typedef struct {
  const char* const* ones;   // [0]="" [1]-[9]
  const char* const* teens;  // [0]="ten" [1]-[9]
  const char* const* tens;   // [2]-[5]
  const char* past;
  const char* to;
  const char* oclock;
  const char* half;  // "" = "thirty past"; non-empty = "halb/halv/half *NEXT_HOUR"
  int hour_first;  // 0: NUMBER past *HOUR  /  1: *HOUR past NUMBER
} SpeakFormat;
```

`hour_first=0` → Germanic/English order ("twenty four past *seven").  
`hour_first=1` → Romance order ("*siete y veinte cuatro").  
`half` non-empty → special 30-minute form ("halb *zwei") used by DE, NO, SV, NL.

### Adding a language

1. Add a row to `ALL_LANGUAGES` in `num2words.h`: `X(ENUM, VALUE)` where VALUE is the integer sent from JS.
2. Add `#include "strings-XX.h"` in `num2words.c`.
3. Create `src/c/strings-XX.h` and `strings-XX.c` — must define `HOURS_XX[]`, `RELS_XX[]`, `DAYS_XX[]`, `MONTHS_XX[]`, `DATE_FORMAT_XX`, and `date_suffix_XX()`. See an existing language file for the pattern.
4. Add number-word arrays (`ONES_XX`, `TEENS_XX`, `TENS_XX`) inline in `num2words.c` and add an entry to `speak_formats[]`.
5. Add the language to `src/pkjs/pebble-js-app.js` (the `langs` map) and `config-html.js`. The integer value in the JS map must match the `ALL_LANGUAGES` value.

### AppMessage keys (JS ↔ C)

```
0 = INVERT
1 = TEXT_ALIGN
2 = LANGUAGE
3 = FONT_SIZE
4 = SHOW_DATE
5 = DATE_TIMEOUT
```

The `langs` map in `pebble-js-app.js` must stay in sync with the `ALL_LANGUAGES` macro order:
`{ca:0, de:1, en_GB:2, en_US:3, es:4, fr:5, no:6, sv:7, nl:8, pt:9}`.

### RELS[] arrays

Each `strings-XX.c` still defines a `RELS_XX[]` template array (legacy fuzzy-time format). These are referenced by `lang_strings[]` in `num2words.c` to avoid header changes, but they are **not used** for time display — `SpeakFormat` drives all output now.
