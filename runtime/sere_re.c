/// @file sere_re.c
/// Backtracking matcher for Sere backtick regex literals.

#include "sere_rt.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  const char* p;
  int64_t plen;
  const char* t;
  int64_t tlen;
} Re;

static int reDigit(int ch) { return ch >= '0' && ch <= '9'; }

static int reWord(int ch) {
  return reDigit(ch) || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_';
}

static int reSpace(int ch) {
  return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
}

static int reEscaped(int spec, int ch) {
  if (spec == 'd') {
    return reDigit(ch);
  }
  if (spec == 'D') {
    return !reDigit(ch);
  }
  if (spec == 'w') {
    return reWord(ch);
  }
  if (spec == 'W') {
    return !reWord(ch);
  }
  if (spec == 's') {
    return reSpace(ch);
  }
  if (spec == 'S') {
    return !reSpace(ch);
  }
  if (spec == 'n') {
    return ch == '\n';
  }
  if (spec == 't') {
    return ch == '\t';
  }
  return ch == spec;
}

static int64_t reSkipClass(const Re* re, int64_t pi) {
  if (pi >= re->plen || re->p[pi] != '[') {
    return pi;
  }
  ++pi;
  if (pi < re->plen && re->p[pi] == '^') {
    ++pi;
  }
  while (pi < re->plen && re->p[pi] != ']') {
    if (re->p[pi] == '\\' && pi + 1 < re->plen) {
      pi += 2;
    } else {
      ++pi;
    }
  }
  if (pi < re->plen) {
    ++pi;
  }
  return pi;
}

static int64_t reSkipGroup(const Re* re, int64_t pi);

static int64_t reSkipAtom(const Re* re, int64_t pi) {
  if (pi >= re->plen) {
    return pi;
  }
  const char ch = re->p[pi];
  if (ch == '\\') {
    return pi + (pi + 1 < re->plen ? 2 : 1);
  }
  if (ch == '[') {
    return reSkipClass(re, pi);
  }
  if (ch == '(') {
    return reSkipGroup(re, pi);
  }
  return pi + 1;
}

static int64_t reSkipGroup(const Re* re, int64_t pi) {
  if (pi >= re->plen || re->p[pi] != '(') {
    return pi;
  }
  ++pi;
  int depth = 1;
  while (pi < re->plen && depth > 0) {
    if (re->p[pi] == '\\' && pi + 1 < re->plen) {
      pi += 2;
      continue;
    }
    if (re->p[pi] == '[') {
      pi = reSkipClass(re, pi);
      continue;
    }
    if (re->p[pi] == '(') {
      ++depth;
    } else if (re->p[pi] == ')') {
      --depth;
    }
    ++pi;
  }
  return pi;
}

static int reMatchClass(const Re* re, int64_t pi, int ch) {
  int negate = 0;
  ++pi;
  if (pi < re->plen && re->p[pi] == '^') {
    negate = 1;
    ++pi;
  }
  int ok = 0;
  while (pi < re->plen && re->p[pi] != ']') {
    int lo = 0;
    if (re->p[pi] == '\\' && pi + 1 < re->plen) {
      ok = ok || reEscaped((unsigned char)re->p[pi + 1], ch);
      pi += 2;
      continue;
    }
    lo = (unsigned char)re->p[pi];
    ++pi;
    if (pi + 1 < re->plen && re->p[pi] == '-' && re->p[pi + 1] != ']') {
      const int hi = (unsigned char)re->p[pi + 1];
      ok = ok || (ch >= lo && ch <= hi);
      pi += 2;
    } else {
      ok = ok || ch == lo;
    }
  }
  return negate ? !ok : ok;
}

static int reMatchSeq(const Re* re, int64_t pi, int64_t ti, int64_t endPi, int64_t* outTi);

static int reMatchAtom(const Re* re, int64_t pi, int64_t ti, int64_t* nextPi, int64_t* nextTi) {
  if (pi >= re->plen) {
    return 0;
  }
  const char ch = re->p[pi];
  if (ch == '^') {
    *nextPi = pi + 1;
    *nextTi = ti;
    return ti == 0;
  }
  if (ch == '$') {
    *nextPi = pi + 1;
    *nextTi = ti;
    return ti == re->tlen;
  }
  if (ch == '(') {
    const int64_t close = reSkipGroup(re, pi);
    int64_t innerEnd = ti;
    if (!reMatchSeq(re, pi + 1, ti, close - 1, &innerEnd)) {
      return 0;
    }
    *nextPi = close;
    *nextTi = innerEnd;
    return 1;
  }
  if (ti >= re->tlen) {
    return 0;
  }
  const int tch = (unsigned char)re->t[ti];
  if (ch == '.') {
    *nextPi = pi + 1;
    *nextTi = ti + 1;
    return tch != '\n';
  }
  if (ch == '[') {
    *nextPi = reSkipClass(re, pi);
    *nextTi = ti + 1;
    return reMatchClass(re, pi, tch);
  }
  if (ch == '\\' && pi + 1 < re->plen) {
    *nextPi = pi + 2;
    *nextTi = ti + 1;
    return reEscaped((unsigned char)re->p[pi + 1], tch);
  }
  *nextPi = pi + 1;
  *nextTi = ti + 1;
  return tch == (unsigned char)ch;
}

static char reQuant(const Re* re, int64_t pi, int64_t* after) {
  if (pi < re->plen && (re->p[pi] == '*' || re->p[pi] == '+' || re->p[pi] == '?')) {
    *after = pi + 1;
    return re->p[pi];
  }
  *after = pi;
  return 0;
}

static int reMatchGreedy(const Re* re, int64_t pi, int64_t ti, int64_t after, int minNeed,
                         int maxOne, int64_t endPi, int64_t* outTi) {
  int64_t nextPi = pi;
  int64_t nextTi = ti;
  const int matched = reMatchAtom(re, pi, ti, &nextPi, &nextTi) && nextTi > ti;
  if (maxOne) {
    if (matched && reMatchSeq(re, after, nextTi, endPi, outTi)) {
      return 1;
    }
    return minNeed == 0 && reMatchSeq(re, after, ti, endPi, outTi);
  }
  if (matched && reMatchGreedy(re, pi, nextTi, after, minNeed > 0 ? minNeed - 1 : 0, 0, endPi,
                               outTi)) {
    return 1;
  }
  if (minNeed == 0) {
    return reMatchSeq(re, after, ti, endPi, outTi);
  }
  return 0;
}

static int reMatchQuant(const Re* re, int64_t pi, int64_t ti, int64_t endPi, int64_t* outTi) {
  int64_t after = pi;
  const int64_t atomEnd = reSkipAtom(re, pi);
  const char quant = reQuant(re, atomEnd, &after);
  if (quant == 0) {
    int64_t nextPi = pi;
    int64_t nextTi = ti;
    if (!reMatchAtom(re, pi, ti, &nextPi, &nextTi)) {
      return 0;
    }
    return reMatchSeq(re, nextPi, nextTi, endPi, outTi);
  }
  int minNeed = 0;
  if (quant == '+') {
    minNeed = 1;
  }
  const int maxOne = quant == '?';
  return reMatchGreedy(re, pi, ti, after, minNeed, maxOne, endPi, outTi);
}

static int reMatchSeq(const Re* re, int64_t pi, int64_t ti, int64_t endPi, int64_t* outTi) {
  int64_t alt = pi;
  int depth = 0;
  while (alt < endPi) {
    if (re->p[alt] == '\\' && alt + 1 < endPi) {
      alt += 2;
      continue;
    }
    if (re->p[alt] == '[') {
      alt = reSkipClass(re, alt);
      continue;
    }
    if (re->p[alt] == '(') {
      ++depth;
    } else if (re->p[alt] == ')') {
      --depth;
    } else if (re->p[alt] == '|' && depth == 0) {
      int64_t leftEnd = ti;
      if (reMatchSeq(re, pi, ti, alt, &leftEnd)) {
        *outTi = leftEnd;
        return 1;
      }
      return reMatchSeq(re, alt + 1, ti, endPi, outTi);
    }
    ++alt;
  }
  if (pi >= endPi) {
    *outTi = ti;
    return 1;
  }
  return reMatchQuant(re, pi, ti, endPi, outTi);
}

static int32_t reSearch(const char* p, int64_t plen, const char* t, int64_t tlen, int64_t* start,
                        int64_t* end) {
  Re re;
  re.p = p == NULL ? "" : p;
  re.plen = plen < 0 ? 0 : plen;
  re.t = t == NULL ? "" : t;
  re.tlen = tlen < 0 ? 0 : tlen;
  const int anchored = re.plen > 0 && re.p[0] == '^';
  for (int64_t index = 0; index <= re.tlen; ++index) {
    int64_t matchedEnd = index;
    if (reMatchSeq(&re, 0, index, re.plen, &matchedEnd)) {
      if (start != NULL) {
        *start = index;
      }
      if (end != NULL) {
        *end = matchedEnd;
      }
      return 1;
    }
    if (anchored) {
      break;
    }
  }
  return 0;
}

static void outEmpty(const char** out_data, int64_t* out_len) {
  if (out_data != NULL) {
    *out_data = "";
  }
  if (out_len != NULL) {
    *out_len = 0;
  }
}

static void outOwned(char* data, int64_t len, const char** out_data, int64_t* out_len) {
  if (data == NULL) {
    outEmpty(out_data, out_len);
    return;
  }
  if (out_data != NULL) {
    *out_data = data;
  }
  if (out_len != NULL) {
    *out_len = len;
  }
}

int32_t sere_re_is_match(const char* pattern, int64_t pattern_len, const char* text,
                         int64_t text_len) {
  return reSearch(pattern, pattern_len, text, text_len, NULL, NULL);
}

void sere_re_find(const char* pattern, int64_t pattern_len, const char* text, int64_t text_len,
                  const char** out_data, int64_t* out_len) {
  int64_t start = 0;
  int64_t end = 0;
  if (!reSearch(pattern, pattern_len, text, text_len, &start, &end) || end < start) {
    outEmpty(out_data, out_len);
    return;
  }
  const int64_t len = end - start;
  char* copy = (char*)malloc((size_t)len + 1);
  if (copy == NULL) {
    outEmpty(out_data, out_len);
    return;
  }
  if (len > 0 && text != NULL) {
    memcpy(copy, text + start, (size_t)len);
  }
  copy[len] = '\0';
  outOwned(copy, len, out_data, out_len);
}

void sere_re_replace(const char* pattern, int64_t pattern_len, const char* text, int64_t text_len,
                     const char* repl, int64_t repl_len, const char** out_data, int64_t* out_len) {
  if (text == NULL || text_len <= 0) {
    outEmpty(out_data, out_len);
    return;
  }
  if (repl == NULL || repl_len < 0) {
    repl = "";
    repl_len = 0;
  }
  char* out = NULL;
  int64_t cap = 0;
  int64_t len = 0;
  int64_t cursor = 0;
  while (cursor <= text_len) {
    int64_t start = 0;
    int64_t end = 0;
    Re slice;
    slice.p = pattern == NULL ? "" : pattern;
    slice.plen = pattern_len < 0 ? 0 : pattern_len;
    slice.t = text + cursor;
    slice.tlen = text_len - cursor;
    if (!reSearch(slice.p, slice.plen, slice.t, slice.tlen, &start, &end)) {
      const int64_t rest = text_len - cursor;
      char* grown = (char*)realloc(out, (size_t)(len + rest + 1));
      if (grown == NULL) {
        free(out);
        outEmpty(out_data, out_len);
        return;
      }
      memcpy(grown + len, text + cursor, (size_t)rest);
      grown[len + rest] = '\0';
      outOwned(grown, len + rest, out_data, out_len);
      return;
    }
    const int64_t absStart = cursor + start;
    const int64_t absEnd = cursor + end;
    const int64_t prefix = absStart - cursor;
    const int64_t need = len + prefix + repl_len + 1;
    if (need > cap) {
      cap = need * 2;
      char* grown = (char*)realloc(out, (size_t)cap);
      if (grown == NULL) {
        free(out);
        outEmpty(out_data, out_len);
        return;
      }
      out = grown;
    }
    if (prefix > 0) {
      memcpy(out + len, text + cursor, (size_t)prefix);
      len += prefix;
    }
    if (repl_len > 0) {
      memcpy(out + len, repl, (size_t)repl_len);
      len += repl_len;
    }
    if (absEnd == cursor) {
      if (cursor >= text_len) {
        break;
      }
      if (cap < len + 2) {
        cap = (len + 2) * 2;
        char* grown = (char*)realloc(out, (size_t)cap);
        if (grown == NULL) {
          free(out);
          outEmpty(out_data, out_len);
          return;
        }
        out = grown;
      }
      out[len++] = text[cursor];
      cursor += 1;
    } else {
      cursor = absEnd;
    }
  }
  if (out == NULL) {
    outEmpty(out_data, out_len);
    return;
  }
  out[len] = '\0';
  outOwned(out, len, out_data, out_len);
}
