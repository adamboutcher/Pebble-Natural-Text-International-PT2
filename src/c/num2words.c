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

static size_t interpolate_and_append(char* buffer, const size_t length,
    const char* parent_str, const char* first_placeholder_str, const char* second_placeholder_str) {
  const char* placeholder_str;
  char* insert_ptr = strstr(parent_str, "$1");

  if (insert_ptr) {
    placeholder_str = first_placeholder_str;
  }
  else {
    insert_ptr = strstr(parent_str, "$2");
    placeholder_str = second_placeholder_str;
  }

  size_t parent_len = strlen(parent_str);
  size_t insert_offset = insert_ptr ? (size_t) insert_ptr - (size_t) parent_str : parent_len;

  size_t remaining = length;

  remaining -= append_string(buffer, min(insert_offset, remaining), parent_str);
  remaining -= append_string(buffer, remaining, placeholder_str);
  if (insert_ptr) {
    remaining -= append_string(buffer, remaining, insert_ptr + 2);
  }

  return remaining;
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

  /* move number of required chars and null terminate */
  int shift = value;
  do {
    ++p;
    shift /= 10;
  } while (shift);
  *p = '\0';

  /* populate result in reverse order */
  do {
    *--p = digit [value % 10];
    value /= 10;
  } while (value);

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

const char* get_rel(Language lang, int index) {
  return lang_strings[lang].rels[index];
}

static const char* const ONES_EN[] = {
  "", "one", "two", "three", "four", "five",
  "six", "seven", "eight", "nine"
};

static const char* const TEENS_EN[] = {
  "ten", "eleven", "twelve", "thirteen", "fourteen", "fifteen",
  "sixteen", "seventeen", "eighteen", "nineteen"
};

static const char* const TENS_EN[] = {
  "", "", "twenty", "thirty", "forty", "fifty"
};

// Appends n (1-59) as written English words, returns bytes written.
static size_t append_number_as_words(char* buffer, size_t available, int n) {
  size_t written = 0;
  if (n < 10) {
    written += append_string(buffer, available - written, ONES_EN[n]);
  } else if (n < 20) {
    written += append_string(buffer, available - written, TEENS_EN[n - 10]);
  } else {
    written += append_string(buffer, available - written, TENS_EN[n / 10]);
    if (n % 10 != 0) {
      written += append_string(buffer, available - written, " ");
      written += append_string(buffer, available - written, ONES_EN[n % 10]);
    }
  }
  return written;
}

void time_to_words(Language lang, int hours, int minutes, int seconds, char* words, size_t buffer_size) {

  size_t remaining = buffer_size;
  memset(words, 0, buffer_size);

  if (lang == EN_US || lang == EN_GB) {
    // Exact spoken English time: "twenty four past *seven", "*eight o'clock", "twelve to *two"
    int hour_index = hours % 24;
    const char* hour      = get_hour(lang, hour_index);
    const char* next_hour = get_hour(lang, (hour_index + 1) % 24);

    if (minutes == 0) {
      remaining -= append_string(words, remaining, "*");
      remaining -= append_string(words, remaining, hour);
      remaining -= append_string(words, remaining, " o'clock ");
    } else if (minutes <= 30) {
      remaining -= append_number_as_words(words, remaining, minutes);
      remaining -= append_string(words, remaining, " past *");
      remaining -= append_string(words, remaining, hour);
      remaining -= append_string(words, remaining, " ");
    } else {
      remaining -= append_number_as_words(words, remaining, 60 - minutes);
      remaining -= append_string(words, remaining, " to *");
      remaining -= append_string(words, remaining, next_hour);
      remaining -= append_string(words, remaining, " ");
    }
  } else {
    // Fuzzy five-minute interval time for all other languages
    int half_mins  = (2 * minutes) + (seconds / 30);
    int rel_index  = ((half_mins + 5) / (2 * 5)) % 12;
    int hour_index;

    if (rel_index == 0 && minutes > 30) {
      hour_index = (hours + 1) % 24;
    } else {
      hour_index = hours % 24;
    }

    const char* hour      = get_hour(lang, hour_index);
    const char* next_hour = get_hour(lang, (hour_index + 1) % 24);
    const char* rel       = get_rel(lang, rel_index);

    remaining -= interpolate_and_append(words, remaining, rel, hour, next_hour);
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

// Replace $1, $2, $3 in fmt with s1, s2, s3 respectively.
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
        while (*sub && pos < length - 1) {
          buffer[pos++] = *sub++;
        }
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