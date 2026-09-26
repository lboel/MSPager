#include "PagerConfig.h"

#include <helpers/UTF8Helpers.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define YAML_LINE_MAX  256

void pagerCfgClear(PagerConfig& cfg) {
  memset(&cfg, 0, sizeof(cfg));
}

const PagerQuestion* pagerCfgFindQuestion(const PagerConfig& cfg, const char* text) {
  for (int i = 0; i < cfg.num_questions; i++) {
    if (strcmp(cfg.questions[i].text, text) == 0) return &cfg.questions[i];
  }
  return NULL;
}

// ---------------------------------------------------------------- YAML subset parser
//
// Supported: block mappings/sequences by space indentation, `#` comments, plain,
// 'single' and "double" quoted scalars (\" \\ \/ \xXX \uXXXX \UXXXXXXXX escapes),
// single-line flow lists `[a, "b"]`, a single-line flow mapping for `channel`.
// Not supported: tabs, anchors/aliases/tags, other flow mappings, block scalars (| >),
// multi-line scalars, multiple documents.

namespace {

bool isBlank(char c) { return c == ' ' || c == '\t'; }

void rtrim(char* s) {
  int n = strlen(s);
  while (n > 0 && isBlank(s[n - 1])) s[--n] = 0;
}

// a quote here opens a quoted scalar (and doesn't belong to plain text like "Geht's")
bool atTokenStart(const char* line, const char* p) {
  while (p > line && p[-1] == ' ') p--;
  return p == line || p[-1] == ':' || p[-1] == '-' || p[-1] == '[' || p[-1] == ',';
}

// skips a quoted scalar starting at p (on the quote), returns the char after the closing quote or NULL
char* skipQuoted(char* p) {
  char q = *p++;
  for (; *p; p++) {
    if (q == '"' && *p == '\\' && p[1]) {
      p++;
    } else if (*p == q) {
      if (q == '\'' && p[1] == '\'') { p++; continue; }   // '' = escaped quote
      return p + 1;
    }
  }
  return NULL;
}

void stripComment(char* line) {
  for (char* p = line; *p; p++) {
    if ((*p == '"' || *p == '\'') && atTokenStart(line, p)) {
      char* end = skipQuoted(p);
      if (end == NULL) return;   // unterminated: reported by the scalar parser
      p = end - 1;
    } else if (*p == '#' && (p == line || isBlank(p[-1]))) {
      *p = 0;
      rtrim(line);
      return;
    }
  }
}

// "key: value" / "key:" -> key, value (leading blanks skipped). Keys are [A-Za-z0-9_].
bool splitKey(char* s, char*& key, char*& value) {
  char* p = s;
  while ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_') p++;
  if (p == s || *p != ':' || (p[1] != 0 && !isBlank(p[1]))) return false;
  *p++ = 0;
  while (isBlank(*p)) p++;
  key = s;
  value = p;
  return true;
}

int hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

// n hex digits at p -> value, -1 if invalid
long hexNum(const char* p, int n) {
  long v = 0;
  for (int i = 0; i < n; i++) {
    int h = hexVal(p[i]);
    if (h < 0) return -1;
    v = v * 16 + h;
  }
  return v;
}

int putUtf8(char* dest, unsigned long cp) {
  if (cp < 0x80)    { dest[0] = cp; return 1; }
  if (cp < 0x800)   { dest[0] = 0xC0 | (cp >> 6); dest[1] = 0x80 | (cp & 0x3F); return 2; }
  if (cp < 0x10000) { dest[0] = 0xE0 | (cp >> 12); dest[1] = 0x80 | ((cp >> 6) & 0x3F); dest[2] = 0x80 | (cp & 0x3F); return 3; }
  dest[0] = 0xF0 | (cp >> 18); dest[1] = 0x80 | ((cp >> 12) & 0x3F);
  dest[2] = 0x80 | ((cp >> 6) & 0x3F); dest[3] = 0x80 | (cp & 0x3F);
  return 4;
}

class Parser {
public:
  Parser(PagerConfig& cfg, PagerCfgError& err) : _cfg(cfg), _err(err) { }
  bool run(const char* yaml, size_t len);

private:
  enum Section { NONE, CHANNEL, QUESTIONS };

  PagerConfig& _cfg;
  PagerCfgError& _err;
  int _line = 0;
  Section _section = NONE;
  unsigned _seen_top = 0, _seen_chan = 0, _seen_q = 0;
  int _chan_indent = -1;
  int _q_indent = -1;         // column of the "- " of question items
  int _q_key_indent = -1;     // column of the keys inside the current question
  int _replies_indent = -1;   // >= 0 while a block list of replies may follow
  PagerQuestion* _q = NULL;
  int _q_line[PAGER_CFG_MAX_QUESTIONS];

  bool fail(const char* fmt, ...);
  bool once(unsigned& seen, unsigned bit, const char* key);
  bool line(char* buf);
  bool dashItem(int indent, char* s);
  bool keyLine(int indent, char* s);
  bool topKey(const char* key, char* value);
  bool channelKey(const char* key, char* value);
  bool questionKey(const char* key, char* value, int indent);
  bool flowList(char* s);
  bool flowChannel(char* s);
  bool addReply(char* s);
  bool scalar(char* s, char* dest, int max_len, const char* what);
  bool finish();
};

bool Parser::fail(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vsnprintf(_err.msg, sizeof(_err.msg), fmt, args);
  va_end(args);
  _err.line = _line;
  return false;
}

bool Parser::once(unsigned& seen, unsigned bit, const char* key) {
  if (seen & bit) return fail("duplicate key '%s'", key);
  seen |= bit;
  return true;
}

bool Parser::run(const char* yaml, size_t len) {
  pagerCfgClear(_cfg);
  memset(&_err, 0, sizeof(_err));

  const char* p = yaml;
  const char* end = yaml + len;
  if (len >= 3 && memcmp(p, "\xEF\xBB\xBF", 3) == 0) p += 3;   // UTF-8 BOM

  char buf[YAML_LINE_MAX];
  while (p < end) {
    const char* nl = (const char*)memchr(p, '\n', end - p);
    size_t n = (nl ? nl : end) - p;
    _line++;
    if (n > 0 && p[n - 1] == '\r') n--;
    if (n >= sizeof(buf)) return fail("line longer than %d bytes", (int)sizeof(buf) - 1);
    memcpy(buf, p, n);
    buf[n] = 0;
    if (strlen(buf) != n) return fail("unexpected NUL byte");
    p = nl ? nl + 1 : end;
    if (!line(buf)) return false;
  }
  return finish();
}

bool Parser::line(char* buf) {
  int indent = 0;
  while (buf[indent] == ' ') indent++;
  if (buf[indent] == '\t') return fail("tabs are not allowed for indentation");
  stripComment(buf);
  rtrim(buf);
  char* s = buf + indent;
  if (*s == 0) return true;
  if (indent == 0 && (strcmp(s, "---") == 0 || strcmp(s, "...") == 0)) return true;   // document markers

  if (*s == '-' && (s[1] == 0 || s[1] == ' ')) return dashItem(indent, s);
  return keyLine(indent, s);
}

bool Parser::dashItem(int indent, char* s) {
  char* rest = s + 1;
  while (*rest == ' ') rest++;

  if (_section == QUESTIONS && _q && _replies_indent >= 0 && indent >= _replies_indent) {
    if (*rest == 0) return fail("empty reply");
    return addReply(rest);
  }
  _replies_indent = -1;

  if (_section != QUESTIONS || (_q_indent >= 0 && indent != _q_indent)) return fail("unexpected list item");
  if (_cfg.num_questions >= PAGER_CFG_MAX_QUESTIONS) return fail("more than %d questions", PAGER_CFG_MAX_QUESTIONS);
  _q_indent = indent;
  _q_line[_cfg.num_questions] = _line;
  _q = &_cfg.questions[_cfg.num_questions++];
  _seen_q = 0;
  _q_key_indent = -1;
  if (*rest == 0) return true;   // keys follow on the next lines

  _q_key_indent = indent + (rest - s);
  char *key, *value;
  if (!splitKey(rest, key, value)) return fail("expected 'text:' or 'replies:' in question");
  return questionKey(key, value, _q_key_indent);
}

bool Parser::keyLine(int indent, char* s) {
  _replies_indent = -1;
  char *key, *value;
  if (!splitKey(s, key, value)) return fail("expected 'key: value'");

  if (indent == 0) {
    _section = NONE;
    _q = NULL;
    return topKey(key, value);
  }
  if (_section == CHANNEL) {
    if (_chan_indent < 0) _chan_indent = indent;
    if (indent != _chan_indent) return fail("inconsistent indentation of '%s'", key);
    return channelKey(key, value);
  }
  if (_section == QUESTIONS && _q && indent > _q_indent) {
    if (_q_key_indent < 0) _q_key_indent = indent;
    if (indent != _q_key_indent) return fail("inconsistent indentation of '%s'", key);
    return questionKey(key, value, indent);
  }
  return fail("unexpected indentation of '%s'", key);
}

bool Parser::topKey(const char* key, char* value) {
  if (strcmp(key, "version") == 0) {
    if (!once(_seen_top, 1, key)) return false;
    char v[8];
    if (!scalar(value, v, sizeof(v) - 1, "version")) return false;
    if (strcmp(v, "1") != 0) return fail("unsupported version '%s' (expected 1)", v);
    return true;
  }
  if (strcmp(key, "nickname") == 0) {
    if (!once(_seen_top, 2, key)) return false;
    if (!scalar(value, _cfg.nickname, PAGER_CFG_NAME_MAX, "nickname")) return false;
    if (strpbrk(_cfg.nickname, ":[]")) return fail("nickname must not contain ':', '[' or ']'");   // FRD-009/007 parsing
    _cfg.flags |= PAGER_CFG_HAS_NICKNAME;
    return true;
  }
  if (strcmp(key, "channel") == 0) {
    if (!once(_seen_top, 4, key)) return false;
    if (*value == '{') return flowChannel(value);
    if (*value) return fail("'channel' needs nested 'name:' and 'key:' lines");
    _section = CHANNEL;
    _chan_indent = -1;
    return true;
  }
  if (strcmp(key, "questions") == 0) {
    if (!once(_seen_top, 8, key)) return false;
    if (strcmp(value, "[]") == 0) return true;
    if (*value) return fail("'questions' must be a list");
    _section = QUESTIONS;
    return true;
  }
  return fail("unknown key '%s'", key);
}

bool Parser::channelKey(const char* key, char* value) {
  if (strcmp(key, "name") == 0) {
    if (!once(_seen_chan, 1, key)) return false;
    if (!scalar(value, _cfg.channel_name, PAGER_CFG_NAME_MAX, "channel name")) return false;
    if (strcmp(_cfg.channel_name, "Public") == 0) return fail("channel name 'Public' is reserved");
    _cfg.flags |= PAGER_CFG_HAS_CHANNEL;
    return true;
  }
  if (strcmp(key, "key") == 0) {
    if (!once(_seen_chan, 2, key)) return false;
    char hex[65];
    if (!scalar(value, hex, sizeof(hex) - 1, "channel key")) return false;
    if (strlen(hex) != 32) return fail("channel key must be 32 hex characters");
    for (int i = 0; i < 16; i++) {
      long b = hexNum(&hex[i * 2], 2);
      if (b < 0) return fail("channel key must be 32 hex characters");
      _cfg.channel_key[i] = (uint8_t)b;
    }
    _cfg.flags |= PAGER_CFG_HAS_KEY;
    return true;
  }
  return fail("unknown key '%s' in channel", key);
}

bool Parser::questionKey(const char* key, char* value, int indent) {
  if (strcmp(key, "text") == 0) {
    if (!once(_seen_q, 1, key)) return false;
    return scalar(value, _q->text, PAGER_CFG_TEXT_MAX, "question text");
  }
  if (strcmp(key, "replies") == 0) {
    if (!once(_seen_q, 2, key)) return false;
    if (*value == 0) {
      _replies_indent = indent;   // "- ..." lines follow
      return true;
    }
    if (*value == '[') return flowList(value);
    return fail("'replies' must be a list");
  }
  return fail("unknown key '%s' in question", key);
}

// "[a, 'b', "c, d"]" on a single line
bool Parser::flowList(char* s) {
  char* p = s + 1;
  for (;;) {
    while (isBlank(*p)) p++;
    if (*p == ']') { p++; break; }   // empty list or trailing comma
    char* item = p;
    if (*p == '"' || *p == '\'') {
      p = skipQuoted(p);
      if (p == NULL) return fail("missing closing quote in replies");
    }
    while (*p && *p != ',' && *p != ']') p++;
    if (*p == 0) return fail("missing ']' in replies");
    char sep = *p;
    *p++ = 0;
    rtrim(item);
    if (*item == 0) return fail("empty reply");
    if (!addReply(item)) return false;
    if (sep == ']') break;
  }
  while (isBlank(*p)) p++;
  if (*p) return fail("unexpected text after ']'");
  return true;
}

// "{name: Familie, key: 00ff...}" on a single line
bool Parser::flowChannel(char* s) {
  char* p = s + 1;
  for (;;) {
    while (isBlank(*p)) p++;
    if (*p == '}') { p++; break; }
    char* item = p;
    while (*p && *p != ',' && *p != '}') {
      if ((*p == '"' || *p == '\'') && atTokenStart(item, p)) {
        p = skipQuoted(p);
        if (p == NULL) return fail("missing closing quote in channel");
      } else {
        p++;
      }
    }
    if (*p == 0) return fail("missing '}' in channel");
    char sep = *p;
    *p++ = 0;
    char *key, *value;
    if (!splitKey(item, key, value)) return fail("expected 'key: value' in channel");
    rtrim(value);
    if (!channelKey(key, value)) return false;
    if (sep == '}') break;
  }
  while (isBlank(*p)) p++;
  if (*p) return fail("unexpected text after '}'");
  return true;
}

bool Parser::addReply(char* s) {
  if (_q->num_replies >= PAGER_CFG_MAX_REPLIES) return fail("more than %d replies", PAGER_CFG_MAX_REPLIES);
  if (!scalar(s, _q->replies[_q->num_replies], PAGER_CFG_TEXT_MAX, "reply")) return false;
  _q->num_replies++;
  return true;
}

// single-line scalar -> dest (max_len bytes + NUL): non-empty, valid UTF-8, no control chars
bool Parser::scalar(char* s, char* dest, int max_len, const char* what) {
  while (isBlank(*s)) s++;
  if (*s == 0) return fail("%s has no value", what);

  char buf[YAML_LINE_MAX + 4];
  int n = 0;
  if (*s == '"') {
    char* p = s + 1;
    while (*p && *p != '"') {
      if (*p != '\\') { buf[n++] = *p++; continue; }
      p++;
      long cp = -1;
      switch (*p) {
        case '"':  case '\\': case '/':  buf[n++] = *p++; continue;
        case 'x':  cp = hexNum(p + 1, 2); p += 3; break;
        case 'u':  cp = hexNum(p + 1, 4); p += 5; break;
        case 'U':  cp = hexNum(p + 1, 8); p += 9; break;
        default:   return fail("unsupported escape '\\%c' in %s", *p ? *p : ' ', what);
      }
      if (cp >= 0xD800 && cp <= 0xDBFF && p[0] == '\\' && p[1] == 'u') {   // UTF-16 surrogate pair
        long lo = hexNum(p + 2, 4);
        if (lo >= 0xDC00 && lo <= 0xDFFF) {
          cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
          p += 6;
        }
      }
      if (cp < 0 || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return fail("invalid escape in %s", what);
      n += putUtf8(&buf[n], cp);
    }
    if (*p != '"') return fail("missing closing quote in %s", what);
    s = p + 1;
  } else if (*s == '\'') {
    char* p = s + 1;
    for (; *p; p++) {
      if (*p == '\'') {
        if (p[1] != '\'') break;
        p++;
      }
      buf[n++] = *p;
    }
    if (*p != '\'') return fail("missing closing quote in %s", what);
    s = p + 1;
  } else {
    if (strchr("[]{}&*!|>%@`", *s)) return fail("%s must be plain text (quote it)", what);
    while (*s) buf[n++] = *s++;
  }
  while (isBlank(*s)) s++;
  if (*s) return fail("unexpected text after quoted %s", what);
  buf[n] = 0;

  if (n == 0) return fail("%s is empty", what);
  if (n > max_len) return fail("%s longer than %d bytes", what, max_len);
  if (mesh::validUtf8PrefixLength(buf, n) != (size_t)n) return fail("%s is not valid UTF-8", what);
  for (int i = 0; i < n; i++) {
    if ((unsigned char)buf[i] < 0x20 || buf[i] == 0x7F) return fail("%s contains a control character", what);
  }
  memcpy(dest, buf, n + 1);
  return true;
}

bool Parser::finish() {
  for (int i = 0; i < _cfg.num_questions; i++) {
    const PagerQuestion& q = _cfg.questions[i];
    _line = _q_line[i];
    if (q.text[0] == 0) return fail("question without 'text'");
    if (q.num_replies == 0) return fail("question '%s' has no replies", q.text);
    for (int j = 0; j < i; j++) {
      if (strcmp(_cfg.questions[j].text, q.text) == 0) return fail("duplicate question '%s'", q.text);
    }
  }
  return true;
}

// ---------------------------------------------------------------- flash format
//
// "PCFG", version, flags, [nickname], [channel name], [16-byte key], question count,
// per question: text, reply count, replies. Strings are length-prefixed (1 byte), no NUL.

const uint8_t MAGIC[4] = { 'P', 'C', 'F', 'G' };

struct Writer {
  uint8_t* p;
  int left;
  bool ok;
  void bytes(const void* src, int n) {
    if (!ok || n > left) { ok = false; return; }
    memcpy(p, src, n);
    p += n;
    left -= n;
  }
  void byte(uint8_t b) { bytes(&b, 1); }
  void str(const char* s) { int n = strlen(s); byte(n); bytes(s, n); }
};

struct Reader {
  const uint8_t* p;
  int left;
  bool ok;
  void bytes(void* dest, int n) {
    if (!ok || n > left) { ok = false; return; }
    memcpy(dest, p, n);
    p += n;
    left -= n;
  }
  uint8_t byte() { uint8_t b = 0; bytes(&b, 1); return b; }
  void str(char* dest, int max_len) {
    int n = byte();
    if (n > max_len) ok = false;
    bytes(dest, n);
    dest[ok ? n : 0] = 0;
  }
};

}  // namespace

bool pagerCfgParseYaml(const char* yaml, size_t len, PagerConfig& cfg, PagerCfgError& err) {
  Parser parser(cfg, err);
  return parser.run(yaml, len);
}

int pagerCfgSerialize(const PagerConfig& cfg, uint8_t* dest, int dest_size) {
  Writer w = { dest, dest_size, true };
  w.bytes(MAGIC, sizeof(MAGIC));
  w.byte(PAGER_CFG_VERSION);
  w.byte(cfg.flags);
  if (cfg.flags & PAGER_CFG_HAS_NICKNAME) w.str(cfg.nickname);
  if (cfg.flags & PAGER_CFG_HAS_CHANNEL)  w.str(cfg.channel_name);
  if (cfg.flags & PAGER_CFG_HAS_KEY)      w.bytes(cfg.channel_key, sizeof(cfg.channel_key));
  w.byte(cfg.num_questions);
  for (int i = 0; i < cfg.num_questions; i++) {
    const PagerQuestion& q = cfg.questions[i];
    w.str(q.text);
    w.byte(q.num_replies);
    for (int j = 0; j < q.num_replies; j++) w.str(q.replies[j]);
  }
  return w.ok ? dest_size - w.left : 0;
}

bool pagerCfgDeserialize(const uint8_t* src, int len, PagerConfig& cfg) {
  pagerCfgClear(cfg);
  Reader r = { src, len, true };
  uint8_t magic[sizeof(MAGIC)];
  r.bytes(magic, sizeof(magic));
  if (!r.ok || memcmp(magic, MAGIC, sizeof(MAGIC)) != 0 || r.byte() != PAGER_CFG_VERSION) return false;
  cfg.flags = r.byte();
  if (cfg.flags & PAGER_CFG_HAS_NICKNAME) r.str(cfg.nickname, PAGER_CFG_NAME_MAX);
  if (cfg.flags & PAGER_CFG_HAS_CHANNEL)  r.str(cfg.channel_name, PAGER_CFG_NAME_MAX);
  if (cfg.flags & PAGER_CFG_HAS_KEY)      r.bytes(cfg.channel_key, sizeof(cfg.channel_key));
  cfg.num_questions = r.byte();
  if (cfg.num_questions > PAGER_CFG_MAX_QUESTIONS) r.ok = false;
  for (int i = 0; r.ok && i < cfg.num_questions; i++) {
    PagerQuestion& q = cfg.questions[i];
    r.str(q.text, PAGER_CFG_TEXT_MAX);
    q.num_replies = r.byte();
    if (q.num_replies > PAGER_CFG_MAX_REPLIES) r.ok = false;
    for (int j = 0; r.ok && j < q.num_replies; j++) r.str(q.replies[j], PAGER_CFG_TEXT_MAX);
  }
  if (!r.ok || r.left != 0) {
    pagerCfgClear(cfg);
    return false;
  }
  return true;
}
