#include "num2words.h"
#include "strings-ca.h"
#include "strings-de.h"
#include "strings-en_GB.h"
#include "strings-en_US.h"
#include "strings-es.h"
#include "strings-fr.h"
#include "strings-no.h"
#include "strings-sv.h"
#include "strings-nl.h"
#include "strings-pt.h"
#include "string.h"

size_t min(const size_t a, const size_t b) {
  return a < b ? a : b;
}

static size_t append_string(char* buffer, const size_t length, const char* str) {
  strncat(buffer, str, length);
  size_t written = strlen(str);
  return (length > written) ? written : length;
}

/* simple base 10 only itoa, found: http://stackoverflow.com/questions/20435527 */
char * itoa10(int value, char *result)
{
  char const digit[] = "0123456789";
  char *p = result;
  if (value < 0) {
    *p++ = '-';
    value *= -1;
  }
  int shift = value;
  do { ++p; shift /= 10; } while (shift);
  *p = '\0';
  do { *--p = digit[value % 10]; value /= 10; } while (value);
  return result;
}

typedef struct {
  const char* const* hours;
  const char* const* rels;
} LangStrings;

static const LangStrings lang_strings[] = {
  #define X(ENUM, VAL) [ENUM] = { HOURS_##ENUM, RELS_##ENUM },
  ALL_LANGUAGES
  #undef X
};

const char* get_hour(Language lang, int index) {
  return lang_strings[lang].hours[index];
}

// ── Spoken time: number-word tables ─────────────────────────────────────────

typedef struct {
  const char* const* ones;   // [0]="" [1]-[9]
  const char* const* teens;  // [0]="ten" [1]-[9]
  const char* const* tens;   // [2]-[5]
  const char* past;
  const char* to;
  const char* oclock;
  const char* half;  // "" = use "thirty past"; non-empty = half-past phrase
  int half_next;   // 1 = "half *NEXT_HOUR" (DE/NO/SV/NL); 0 = "half *HOUR" (EN)
  int hour_first;  // 0: NUMBER past *HOUR  /  1: *HOUR past NUMBER
} SpeakFormat;

// English
static const char* const ONES_EN[]  = {"","one","two","three","four","five","six","seven","eight","nine"};
static const char* const TEENS_EN[] = {"ten","eleven","twelve","thirteen","fourteen","fifteen","sixteen","seventeen","eighteen","nineteen"};
static const char* const TENS_EN[]  = {"","","twenty","thirty","forty","fifty"};

// German — "fünf zehn" splits the 9-byte UTF-8 "fünfzehn" into two tokens
static const char* const ONES_DE[]  = {"","ein","zwei","drei","vier","fünf","sechs","sieben","acht","neun"};
static const char* const TEENS_DE[] = {"zehn","elf","zwölf","dreizehn","vierzehn","fünf zehn","sechzehn","siebzehn","achtzehn","neunzehn"};
static const char* const TENS_DE[]  = {"","","zwanzig","dreißig","vierzig","fünfzig"};

// French — 17-19 use hyphenated forms (≤8 bytes each)
static const char* const ONES_FR[]  = {"","un","deux","trois","quatre","cinq","six","sept","huit","neuf"};
static const char* const TEENS_FR[] = {"dix","onze","douze","treize","quatorze","quinze","seize","dix-sept","dix-huit","dix-neuf"};
static const char* const TENS_FR[]  = {"","","vingt","trente","quarante","cinquante"};

// Spanish — 16-19 split into two tokens to avoid overlong compound forms
static const char* const ONES_ES[]  = {"","uno","dos","tres","cuatro","cinco","seis","siete","ocho","nueve"};
static const char* const TEENS_ES[] = {"diez","once","doce","trece","catorce","quince","diez seis","diez siete","diez ocho","diez nueve"};
static const char* const TENS_ES[]  = {"","","veinte","treinta","cuarenta","cincuenta"};

// Norwegian
static const char* const ONES_NO[]  = {"","en","to","tre","fire","fem","seks","sju","åtte","ni"};
static const char* const TEENS_NO[] = {"ti","elleve","tolv","tretten","fjorten","femten","seksten","sytten","atten","nitten"};
static const char* const TENS_NO[]  = {"","","tjue","tretti","førti","femti"};

// Swedish
static const char* const ONES_SV[]  = {"","ett","två","tre","fyra","fem","sex","sju","åtta","nio"};
static const char* const TEENS_SV[] = {"tio","elva","tolv","tretton","fjorton","femton","sexton","sjutton","arton","nitton"};
static const char* const TENS_SV[]  = {"","","tjugo","trettio","fyrtio","femtio"};

// Dutch — 17 and 19 split to avoid 9-byte overflow
static const char* const ONES_NL[]  = {"","één","twee","drie","vier","vijf","zes","zeven","acht","negen"};
static const char* const TEENS_NL[] = {"tien","elf","twaalf","dertien","veertien","vijftien","zestien","zeven tien","achttien","negen tien"};
static const char* const TENS_NL[]  = {"","","twintig","dertig","veertig","vijftig"};

// Catalan
static const char* const ONES_CA[]  = {"","un","dos","tres","quatre","cinc","sis","set","vuit","nou"};
static const char* const TEENS_CA[] = {"deu","onze","dotze","tretze","catorze","quinze","setze","disset","divuit","dinou"};
static const char* const TENS_CA[]  = {"","","vint","trenta","quaranta","cinquanta"};

// Portuguese — 16-17 split to avoid 9-byte overflow
static const char* const ONES_PT[]  = {"","um","dois","três","quatro","cinco","seis","sete","oito","nove"};
static const char* const TEENS_PT[] = {"dez","onze","doze","treze","catorze","quinze","dez seis","dez sete","dezoito","dezanove"};
static const char* const TENS_PT[]  = {"","","vinte","trinta","quarenta","cinquenta"};

static const SpeakFormat speak_formats[] = {
  [CA]    = { ONES_CA, TEENS_CA, TENS_CA, "i",     "menys", "en punt",  "",          0, 1 },
  [DE]    = { ONES_DE, TEENS_DE, TENS_DE, "nach",  "vor",   "Uhr",      "halb",      1, 0 },
  [EN_GB] = { ONES_EN, TEENS_EN, TENS_EN, "past",  "to",    "o'clock",  "half", 0, 0 },
  [EN_US] = { ONES_EN, TEENS_EN, TENS_EN, "past",  "to",    "o'clock",  "half", 0, 0 },
  [ES]    = { ONES_ES, TEENS_ES, TENS_ES, "y",     "menos", "en punto", "",          0, 1 },
  [FR]    = { ONES_FR, TEENS_FR, TENS_FR, "passé", "avant", "heures",   "",          0, 0 },
  [NO]    = { ONES_NO, TEENS_NO, TENS_NO, "over",  "på",    "",         "halv",      1, 0 },
  [SV]    = { ONES_SV, TEENS_SV, TENS_SV, "över",  "i",     "",         "halv",      1, 0 },
  [NL]    = { ONES_NL, TEENS_NL, TENS_NL, "over",  "voor",  "uur",      "half",      1, 0 },
  [PT]    = { ONES_PT, TEENS_PT, TENS_PT, "e",     "menos", "em ponto", "",          0, 1 },
};

// Appends n (1–59) as spoken words; returns bytes written.
static size_t append_number_spoken(char* buffer, size_t avail, int n, const SpeakFormat* f) {
  size_t w = 0;
  if (n < 10) {
    w += append_string(buffer, avail - w, f->ones[n]);
  } else if (n < 20) {
    w += append_string(buffer, avail - w, f->teens[n - 10]);
  } else {
    w += append_string(buffer, avail - w, f->tens[n / 10]);
    if (n % 10) {
      w += append_string(buffer, avail - w, " ");
      w += append_string(buffer, avail - w, f->ones[n % 10]);
    }
  }
  return w;
}

void time_to_words(Language lang, int hours, int minutes, char* words, size_t buffer_size) {
  size_t remaining = buffer_size;
  memset(words, 0, buffer_size);

  const SpeakFormat* f = &speak_formats[lang];
  const char* hour      = get_hour(lang, hours % 24);
  const char* next_hour = get_hour(lang, (hours % 24 + 1) % 24);

  if (minutes == 0) {
    // e.g. "*eight o'clock" / "*acht Uhr" / "*sju" (NO/SV have empty oclock)
    remaining -= append_string(words, remaining, "*");
    remaining -= append_string(words, remaining, hour);
    if (f->oclock[0]) {
      remaining -= append_string(words, remaining, " ");
      remaining -= append_string(words, remaining, f->oclock);
    }
    remaining -= append_string(words, remaining, " ");

  } else if (minutes == 30 && f->half[0]) {
    if (f->half_next) {
      // e.g. "halb *zwei" / "halv *to" / "half *twee"
      remaining -= append_string(words, remaining, f->half);
      remaining -= append_string(words, remaining, " *");
      remaining -= append_string(words, remaining, next_hour);
    } else {
      // e.g. "half past *seven" — "half" substitutes for "thirty" in the normal past stream
      remaining -= append_string(words, remaining, f->half);
      remaining -= append_string(words, remaining, " ");
      remaining -= append_string(words, remaining, f->past);
      remaining -= append_string(words, remaining, " *");
      remaining -= append_string(words, remaining, hour);
    }
    remaining -= append_string(words, remaining, " ");

  } else if (minutes <= 30) {
    if (f->hour_first) {
      // e.g. "*siete y veinte cuatro"
      remaining -= append_string(words, remaining, "*");
      remaining -= append_string(words, remaining, hour);
      remaining -= append_string(words, remaining, " ");
      remaining -= append_string(words, remaining, f->past);
      remaining -= append_string(words, remaining, " ");
      remaining -= append_number_spoken(words, remaining, minutes, f);
    } else {
      // e.g. "twenty four past *seven"
      remaining -= append_number_spoken(words, remaining, minutes, f);
      remaining -= append_string(words, remaining, " ");
      remaining -= append_string(words, remaining, f->past);
      remaining -= append_string(words, remaining, " *");
      remaining -= append_string(words, remaining, hour);
    }
    remaining -= append_string(words, remaining, " ");

  } else {
    int to_mins = 60 - minutes;
    if (f->hour_first) {
      // e.g. "*dos menos doce"
      remaining -= append_string(words, remaining, "*");
      remaining -= append_string(words, remaining, next_hour);
      remaining -= append_string(words, remaining, " ");
      remaining -= append_string(words, remaining, f->to);
      remaining -= append_string(words, remaining, " ");
      remaining -= append_number_spoken(words, remaining, to_mins, f);
    } else {
      // e.g. "twelve to *two"
      remaining -= append_number_spoken(words, remaining, to_mins, f);
      remaining -= append_string(words, remaining, " ");
      remaining -= append_string(words, remaining, f->to);
      remaining -= append_string(words, remaining, " *");
      remaining -= append_string(words, remaining, next_hour);
    }
    remaining -= append_string(words, remaining, " ");
  }
}

const char* get_day(Language lang, int index) {
  switch (lang) {
    case EN_GB: return DAYS_EN_GB[index];
    default:    return DAYS_EN_US[index];
  }
}

const char* get_month(Language lang, int index) {
  switch (lang) {
    case EN_GB: return MONTHS_EN_GB[index];
    default:    return MONTHS_EN_US[index];
  }
}

const char* get_date_format(Language lang) {
  switch (lang) {
    case EN_GB: return DATE_FORMAT_EN_GB;
    default:    return DATE_FORMAT_EN_US;
  }
}

const char* get_date_suffix(Language lang, int date) {
  switch (lang) {
    #define X(ENUM, VAL) case ENUM: return date_suffix_##ENUM(date);
    ALL_LANGUAGES
    #undef X
    default: return "";
  }
}

static void format_date_string(char* buffer, size_t length,
                                const char* fmt,
                                const char* s1, const char* s2, const char* s3) {
  memset(buffer, 0, length);
  size_t pos = 0;
  const char* src = fmt;
  while (*src && pos < length - 1) {
    if (*src == '$' && *(src + 1) >= '1' && *(src + 1) <= '3') {
      const char* sub = NULL;
      switch (*(src + 1)) {
        case '1': sub = s1; break;
        case '2': sub = s2; break;
        case '3': sub = s3; break;
      }
      src += 2;
      if (sub) {
        while (*sub && pos < length - 1) buffer[pos++] = *sub++;
      }
    } else {
      buffer[pos++] = *src++;
    }
  }
}

void date_to_words(Language lang, int day, int date, int month, char* words, size_t buffer_size) {
  char date_str[6];
  itoa10(date, date_str);
  strncat(date_str, get_date_suffix(lang, date), sizeof(date_str) - strlen(date_str) - 1);
  format_date_string(words, buffer_size,
                     get_date_format(lang),
                     get_day(lang, day),
                     get_month(lang, month),
                     date_str);
}
