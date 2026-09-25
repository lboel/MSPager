#include "UITask.h"
#include "../MyMesh.h"
#include "target.h"

#define AUTO_OFF_MILLIS     ((unsigned long)PAGER_DISPLAY_TIMEOUT_SECS * 1000UL)
#define BOOT_SCREEN_MILLIS  5000

#ifndef BATT_MIN_MILLIVOLTS
  #define BATT_MIN_MILLIVOLTS 3000
#endif
#ifndef BATT_MAX_MILLIVOLTS
  #define BATT_MAX_MILLIVOLTS 4200
#endif

// 128x64 layout, 6x8 font (21 columns)
#define CHAR_W        6
#define LINE_H        8
#define HEADER_H      12
#define CHAT_Y0       13
#define CHAT_LINES    5
#define HINT_SEP_Y    53    // separator above the button hint
#define HINT_Y        56

// canned message catalogue (FRD-006). Location messages: FRD-019
#define LOC_REQUEST  "Standort?"       // other pagers answer automatically with LOC_SHARE
#define LOC_SHARE    "Mein Standort"   // shown with distance/bearing on the receiving pager
static const char* const COMPOSE_OPTIONS[] = { "Angekommen?", "Brauche Hilfe", LOC_REQUEST, LOC_SHARE };
static const char* const REPLY_OPTIONS[]   = { "Ja", "Nein", "OK", "\xF0\x9F\x93\x9E", LOC_REQUEST, LOC_SHARE };  // 4th: U+1F4DE
#define PICKER_ROWS  4
#define NUM_COMPOSE  (int)(sizeof(COMPOSE_OPTIONS) / sizeof(COMPOSE_OPTIONS[0]))
#define NUM_REPLY    (int)(sizeof(REPLY_OPTIONS) / sizeof(REPLY_OPTIONS[0]))

// U+1F4DE TELEPHONE RECEIVER, drawn as 8x8 glyph (FRD-013)
static const char PHONE_UTF8[] = "\xF0\x9F\x93\x9E";
// old-style desk phone (handset on top, body with dial) - more recognizable than a lone receiver
static const uint8_t phone_glyph[8] = { 0x7E, 0xC3, 0x00, 0x3C, 0x7E, 0x66, 0x7E, 0x00 };
static const uint8_t phone_glyph_2x[32] = {   // 16x16, drawn separately (not pixel-doubled)
  0x00,0x00, 0x3F,0xFC, 0x7F,0xFE, 0xF0,0x0F, 0xF0,0x0F, 0x00,0x00, 0x07,0xE0, 0x0C,0x30,
  0x1B,0xD8, 0x32,0x4C, 0x63,0xC6, 0x60,0x06, 0x7F,0xFE, 0x7F,0xFE, 0x00,0x00, 0x00,0x00 };

#define LED_ON_MILLIS    100   // new-message blink (FRD-014)
#define LED_OFF_MILLIS   900

// GPS status icons, 8x8, MSB first (header, left of the battery)
static const uint8_t gps_fix_icon[8]   = { 0x3C, 0x7E, 0xE7, 0xE7, 0x7E, 0x3C, 0x18, 0x18 };  // filled pin
static const uint8_t gps_nofix_icon[8] = { 0x3D, 0x42, 0x85, 0x89, 0x52, 0x24, 0x58, 0x80 };  // hollow pin, slashed
#define GPS_FIX_CURRENT_MILLIS  5000

// marquee: pause, scroll at MARQUEE_PX_PER_SEC, pause at the end, jump back
#define MARQUEE_PAUSE_MILLIS  1500
#define MARQUEE_PX_PER_SEC    20
#define MARQUEE_FRAME_MILLIS  50   // fix counts as current if the last valid reading is this recent

// CP437 arrows for button hints, printed as-is (other control chars become spaces)
#define ARROW_UP     "\x18"
#define ARROW_DOWN   "\x19"
#define ARROW_LEFT   "\x1B"
#define IS_ARROW(c)  ((c) >= 0x18 && (c) <= 0x1B)

// Per-cell advance. Size 2 uses 11px instead of GFX's 12px (10px glyph + 1px gap),
// so 11 chars fit the 128px width and "Angekommen?" stays on one line.
static int cellW(int sz)  { return sz == 2 ? 11 : CHAR_W; }
static int glyphW(int sz) { return sz == 2 ? 17 : 9; }      // phone glyph + 1px gap

// "@[Anna] Ja" -> "@Anna Ja"
static void compactMention(char* dest, size_t dest_size, const char* src) {
  if (src[0] == '@' && src[1] == '[') {
    const char* end = strstr(src + 2, "] ");
    if (end) {
      snprintf(dest, dest_size, "@%.*s %s", (int)(end - (src + 2)), src + 2, end + 2);
      return;
    }
  }
  snprintf(dest, dest_size, "%s", src);
}

static void formatAge(char* dest, size_t dest_size, unsigned long since_millis) {
  unsigned long secs = (millis() - since_millis) / 1000;
  if (secs < 60) {
    snprintf(dest, dest_size, "just now");
  } else if (secs < 3600) {
    snprintf(dest, dest_size, "%lu min ago", secs / 60);
  } else {
    snprintf(dest, dest_size, "%lu h ago", secs / 3600);
  }
}

void UITask::begin(DisplayDriver* display, SensorManager* sensors, NodePrefs* node_prefs) {
  _display = display;
  _sensors = sensors;
  _node_prefs = node_prefs;

  user_btn.begin();
#ifdef PAGER_LED_PIN
  pinMode(PAGER_LED_PIN, OUTPUT);
  digitalWrite(PAGER_LED_PIN, LOW);
#endif

  if (_display != NULL) {
    _display->turnOn();
    _display->setTextWrap(false);   // clip at the right edge; wrapping leaked scrolling text into the next row
  }
  _boot_until = millis() + BOOT_SCREEN_MILLIS;
  _auto_off = _boot_until + AUTO_OFF_MILLIS;
  setScreen(Screen::BOOT);
}

// ---------------------------------------------------------------- inbox

PagerMsg* UITask::msgAt(int idx) {
  if (idx < 0 || idx >= _inbox_count) return NULL;
  int start = (_inbox_head - _inbox_count + PAGER_INBOX_SIZE) % PAGER_INBOX_SIZE;
  return &_inbox[(start + idx) % PAGER_INBOX_SIZE];
}

PagerMsg* UITask::addMsg(const char* sender, const char* body, bool own, const PagerPos& pos) {
  PagerMsg* m = &_inbox[_inbox_head];
  _inbox_head = (_inbox_head + 1) % PAGER_INBOX_SIZE;
  if (_inbox_count < PAGER_INBOX_SIZE) {
    _inbox_count++;
  } else if (_sel >= 0) {
    _sel--;  // oldest dropped, keep the same message selected
  }
  snprintf(m->sender, sizeof(m->sender), "%s", sender);
  snprintf(m->body, sizeof(m->body), "%s", body);
  m->rx_millis = millis();
  m->own = own;
  m->unread = !own;
  m->pos = pos;

  char mention[40];
  snprintf(mention, sizeof(mention), "@[%s] ", _node_prefs->node_name);
  m->for_me = !own && strncmp(body, mention, strlen(mention)) == 0;
  return m;
}

void UITask::markVisibleRead() {
  int first, last;
  if (_sel >= 0) {                 // selected layout: at most one older line, the block, one newer line
    first = _sel > 0 ? _sel - 1 : 0;
    last = _sel + 1;
  } else {                         // plain layout: latest CHAT_LINES
    first = _inbox_count > CHAT_LINES ? _inbox_count - CHAT_LINES : 0;
    last = _inbox_count - 1;
  }
  for (int i = first; i <= last && i < _inbox_count; i++) {
    msgAt(i)->unread = false;
  }
}

// ---------------------------------------------------------------- AbstractUITask

void UITask::msgRead(int msgcount) {
  // offline queue drained by the app; pager inbox is independent
}

void UITask::newMsg(uint8_t path_len, const char* from_name, const char* text, int msgcount) {
  // MyMesh only forwards pager-channel texts here (FRD-008). They arrive as "<sender>: <body>" (FRD-009).
  const char* sep = strstr(text, ": ");
  char sender[32];
  char body[MAX_TEXT_LEN + 1];
  if (sep && sep - text < (int)sizeof(sender)) {
    snprintf(sender, sizeof(sender), "%.*s", (int)(sep - text), text);
    snprintf(body, sizeof(body), "%s", sep + 2);
  } else {
    snprintf(sender, sizeof(sender), "?");
    snprintf(body, sizeof(body), "%s", text);
  }
  PagerPos pos;
  parsePosSuffix(body, pos);   // FRD-010/012: strip "[lat,lon]" into pos

  // own messages (sent from the app over BLE) are shown as "me" and don't alert
  if (strcmp(sender, _node_prefs->node_name) == 0) {
    addMsg("me", body, true, pos);
    _next_refresh = 0;
    return;
  }

  addMsg(sender, body, false, pos);
  checkLocationRequest(sender, body);
  _led_alert = true;
  if (_screen == Screen::CHAT || _screen == Screen::BOOT) {
    setScreen(Screen::CHAT);
  }
  wake();
}

void UITask::notify(UIEventType t) {
  // no buzzer/vibration on Heltec V4; LED alert follows in M4 (FRD-014)
}

// ---------------------------------------------------------------- display power

void UITask::setScreen(Screen s) {
  _screen = s;
  _next_refresh = 0;
}

void UITask::wake() {
  if (_display == NULL) return;
  if (!_display->isOn()) _display->turnOn();
  _auto_off = millis() + AUTO_OFF_MILLIS;
  _next_refresh = 0;
}

void UITask::sleepDisplay() {
  if (_display != NULL) _display->turnOff();
  // FRD-003: back to chat, no selection, pickers cancelled
  _sel = -1;
  _option = 0;
  _screen = Screen::CHAT;
}

void UITask::showAlert(const char* text, int duration_millis) {
  snprintf(_alert, sizeof(_alert), "%s", text);
  _alert_expiry = millis() + duration_millis;
  _next_refresh = 0;
}

// ---------------------------------------------------------------- input (FRD-002)

void UITask::handleShort() {
  switch (_screen) {
    case Screen::PAIRING:
      endPairing(NULL);
      break;
    case Screen::BOOT:
      setScreen(Screen::CHAT);
      break;
    case Screen::CHAT:
      markVisibleRead();
      if (_inbox_count == 0) break;
      if (_sel < 0) {
        _sel = _inbox_count - 1;   // newest
      } else {
        _sel--;                    // older; past the oldest -> deselect
      }
      break;
    case Screen::DETAIL:
      break;
    case Screen::COMPOSE:
      _option = (_option + 1) % NUM_COMPOSE;
      break;
    case Screen::REPLY:
      _option = (_option + 1) % NUM_REPLY;
      break;
  }
}

void UITask::handleLong() {
  switch (_screen) {
    case Screen::PAIRING:
      break;
    case Screen::BOOT:
      if (millis() < 8000) {
        the_mesh.enterCLIRescue();   // keep companion's rescue CLI as recovery path
      } else {
        setScreen(Screen::CHAT);
      }
      break;
    case Screen::CHAT:
      markVisibleRead();
      _option = 0;
      if (_sel < 0) {
        if (the_mesh.findPagerChannel() < 0) {
          showAlert("No '" PAGER_CHANNEL_NAME "' channel", 1500);
          break;
        }
        setScreen(Screen::COMPOSE);
      } else {
        msgAt(_sel)->unread = false;
        setScreen(Screen::DETAIL);
      }
      break;
    case Screen::DETAIL:
      _option = 0;
      setScreen(Screen::REPLY);
      break;
    case Screen::COMPOSE:
      sendCanned(COMPOSE_OPTIONS[_option], NULL);
      setScreen(Screen::CHAT);
      break;
    case Screen::REPLY: {
      PagerMsg* m = msgAt(_sel);
      const char* to = m == NULL ? NULL : (m->own ? _node_prefs->node_name : m->sender);
      sendCanned(REPLY_OPTIONS[_option], to);
      _sel = -1;
      setScreen(Screen::CHAT);
      break;
    }
  }
}

void UITask::handleDouble() {
  switch (_screen) {
    case Screen::PAIRING:
      endPairing(NULL);
      break;
    case Screen::BOOT:
      setScreen(Screen::CHAT);
      break;
    case Screen::CHAT:
      if (_sel < 0) {
        sleepDisplay();
      } else {
        _sel = -1;
      }
      break;
    case Screen::DETAIL:
    case Screen::COMPOSE:
      setScreen(Screen::CHAT);
      break;
    case Screen::REPLY:
      setScreen(Screen::DETAIL);
      break;
  }
}

void UITask::sendCanned(const char* text, const char* mention_to, const char* alert) {
  char body[MAX_TEXT_LEN + 1];
  if (mention_to) {
    snprintf(body, sizeof(body), "@[%s] %s", mention_to, text);
  } else {
    snprintf(body, sizeof(body), "%s", text);
  }
  // every message carries the own position (FRD-010)
  PagerPos pos = currentPos();
  char suffix[48];
  formatPosSuffix(suffix, sizeof(suffix), pos);
  char wire[MAX_TEXT_LEN + 1];
  snprintf(wire, sizeof(wire), "%s%s", body, suffix);

  if (the_mesh.sendPagerMessage(wire)) {
    addMsg("me", body, true, pos);
    showAlert(alert, 1000);
  } else {
    showAlert("Send failed", 1500);
  }
}

// ---------------------------------------------------------------- location (FRD-010/011/019)

void UITask::updateFix() {
  if (millis() < _next_fix_check) return;
  _next_fix_check = millis() + 1000;
  LocationProvider* loc = _sensors ? _sensors->getLocationProvider() : NULL;
  if (loc && loc->isValid()) {
    _fix_lat_e6 = loc->getLatitude();
    _fix_lon_e6 = loc->getLongitude();
    _fix_millis = millis();
    _has_fix = true;
  }
}

// own position for a message: last fix + its age, or unknown
PagerPos UITask::currentPos() {
  PagerPos pos;
  memset(&pos, 0, sizeof(pos));
  if (_has_fix) {
    pos.known = true;
    pos.lat_e6 = _fix_lat_e6;
    pos.lon_e6 = _fix_lon_e6;
    pos.age_secs = (millis() - _fix_millis) / 1000;
  }
  return pos;
}

// "Standort?" to the group, or "@[<me>] Standort?": answer with LOC_SHARE after a random
// delay, so several pagers answering the same request don't collide on air.
void UITask::checkLocationRequest(const char* sender, const char* body) {
  char mention[40];
  snprintf(mention, sizeof(mention), "@[%s] ", _node_prefs->node_name);
  const char* rest = body;
  if (strncmp(body, mention, strlen(mention)) == 0) {
    rest = body + strlen(mention);
  } else if (body[0] == '@' && body[1] == '[') {
    return;                 // addressed to someone else
  }
  if (strcmp(rest, LOC_REQUEST) != 0) return;

  snprintf(_auto_reply_to, sizeof(_auto_reply_to), "%s", sender);
  _auto_reply_at = millis() + the_mesh.getRNG()->nextInt(500, 4000);
}

// relative position of a message's sender: "1.2km NE", "~340m S" (stale fix), or why it's unknown
void UITask::relText(char* dest, size_t dest_size, const PagerMsg* m) {
  if (m->own)            { snprintf(dest, dest_size, "you"); return; }
  if (!m->pos.known)     { snprintf(dest, dest_size, m->pos.no_gps ? "no GPS" : "no position"); return; }
  if (!_has_fix)         { snprintf(dest, dest_size, "no own GPS"); return; }
  double dist, bearing;
  distanceBearing(_fix_lat_e6, _fix_lon_e6, m->pos.lat_e6, m->pos.lon_e6, dist, bearing);
  char tmp[20];
  formatDistance(tmp, sizeof(tmp), dist, bearing);
  snprintf(dest, dest_size, "%s%s", m->pos.age_secs > PAGER_FIX_FRESH_SECS ? "~" : "", tmp);
}

// body as shown in the chat list; location shares get distance/bearing appended
void UITask::displayBody(char* dest, size_t dest_size, const PagerMsg* m) {
  compactMention(dest, dest_size, m->body);
  size_t len = strlen(dest), n = strlen(LOC_SHARE);
  if (!m->own && m->pos.known && len >= n && strcmp(dest + len - n, LOC_SHARE) == 0) {
    char rel[24];
    relText(rel, sizeof(rel), m);
    snprintf(dest + len, dest_size - len, " %s", rel);
  }
}

// ---------------------------------------------------------------- LED alert (FRD-014)

void UITask::ledLoop() {
#ifdef PAGER_LED_PIN
  if (!_led_alert) {
    if (_led_on) {
      _led_on = false;
      digitalWrite(PAGER_LED_PIN, LOW);
    }
    return;
  }
  if (millis() >= _led_next) {
    _led_on = !_led_on;
    digitalWrite(PAGER_LED_PIN, _led_on ? HIGH : LOW);
    _led_next = millis() + (_led_on ? LED_ON_MILLIS : LED_OFF_MILLIS);
  }
#endif
}

// ---------------------------------------------------------------- BLE pairing mode (FRD-017)

// MomentaryButton only reports the 1s long press, so the 10s hold is tracked here.
void UITask::checkPairingHold() {
  if (!user_btn.isPressed()) {
    _hold_start = 0;
    _hold_fired = false;
    return;
  }
  if (_hold_start == 0) {
    _hold_start = millis();
  } else if (!_hold_fired && millis() - _hold_start >= PAGER_PAIRING_HOLD_MILLIS) {
    _hold_fired = true;
    startPairing();
  }
}

// ESP32 BLE stops advertising while any peer holds a connection, so a phone that
// grabbed the link in the background makes the pager invisible to the app.
// Drop that link, then advertise again.
void UITask::startPairing() {
  disableBluetooth();   // stops advertising, disconnects the current peer
  _pair_reenable_at = millis() + 500;
  _pair_saw_idle = false;
  _pair_until = millis() + PAGER_PAIRING_SECS * 1000UL;
  setScreen(Screen::PAIRING);
  wake();
}

void UITask::endPairing(const char* alert) {
  if (_pair_reenable_at) {   // ended before BLE was restarted
    enableBluetooth();
    _pair_reenable_at = 0;
  }
  _pair_until = 0;
  _sel = -1;
  setScreen(Screen::CHAT);
  if (alert) showAlert(alert, 1500);
}

void UITask::renderPairing() {
  _display->setColor(UIColor::title_txt);
  _display->drawTextCentered(_display->width() / 2, 0, "Bluetooth pairing");
  _display->fillRect(0, 10, _display->width(), 1);

  char tmp[24];
  snprintf(tmp, sizeof(tmp), "%lu", (unsigned long)the_mesh.getBLEPin());
  _display->setTextSize(3);   // 18x24 px per char, 6 digits = 108 px
  _display->drawTextCentered(_display->width() / 2, 18, tmp);
  _display->setTextSize(1);

  long left = (long)(_pair_until - millis()) / 1000;
  if (left < 0) left = 0;
  snprintf(tmp, sizeof(tmp), "%s  %lds", _node_prefs->node_name, left);
  _display->drawTextCentered(_display->width() / 2, 44, tmp);
  renderHint("1x:cancel");
}

// ---------------------------------------------------------------- rendering

int UITask::textWidth(const char* str, int sz) {
  int w = 0;
  for (const char* p = str; *p; ) {
    if (strncmp(p, PHONE_UTF8, 4) == 0) {
      w += glyphW(sz);
      p += 4;
    } else {
      w += cellW(sz);
      p++;
      while ((*p & 0xC0) == 0x80) p++;  // one cell per UTF-8 codepoint
    }
  }
  return w;
}

// Draws text at size 1 or 2 with the phone glyph inline; other non-ASCII codepoints become a block.
// Truncates with "..." if wider than max_w.
void UITask::renderText(int x, int y, int max_w, const char* str, int sz) {
  bool truncate = textWidth(str, sz) > max_w;
  int limit = truncate ? max_w - 3 * cellW(sz) : max_w;
  int cx = x;
  char ch[2] = { 0, 0 };

  _display->setTextSize(sz);
  for (const char* p = str; *p; ) {
    if (strncmp(p, PHONE_UTF8, 4) == 0) {
      if (cx + glyphW(sz) - x > limit) break;
      if (sz == 2) _display->drawXbm(cx, y, phone_glyph_2x, 16, 16);
      else         _display->drawXbm(cx, y, phone_glyph, 8, 8);
      cx += glyphW(sz);
      p += 4;
      continue;
    }
    if (cx + cellW(sz) - x > limit) break;
    unsigned char c = (unsigned char)*p++;
    if (c >= 0x80) {
      c = 0xDB;  // CP437 full block
      while ((*p & 0xC0) == 0x80) p++;
    } else if (c < 32 && !IS_ARROW(c)) {
      c = ' ';
    }
    ch[0] = (char)c;
    _display->setCursor(cx, y);
    _display->print(ch);
    cx += cellW(sz);
  }
  for (int i = 0; truncate && i < 3; i++) {
    _display->setCursor(cx, y);
    _display->print(".");
    cx += cellW(sz);
  }
  _display->setTextSize(1);
}

// Draws the whole string from x (may be negative), no truncation; off-screen cells are skipped.
void UITask::renderTextRaw(int x, int y, const char* str, int sz) {
  int cx = x;
  char ch[2] = { 0, 0 };
  _display->setTextSize(sz);
  for (const char* p = str; *p && cx < _display->width(); ) {
    if (strncmp(p, PHONE_UTF8, 4) == 0) {
      if (cx + glyphW(sz) > 0) {
        if (sz == 2) _display->drawXbm(cx, y, phone_glyph_2x, 16, 16);
        else         _display->drawXbm(cx, y, phone_glyph, 8, 8);
      }
      cx += glyphW(sz);
      p += 4;
      continue;
    }
    unsigned char c = (unsigned char)*p++;
    if (c >= 0x80) {
      c = 0xDB;
      while ((*p & 0xC0) == 0x80) p++;
    } else if (c < 32 && !IS_ARROW(c)) {
      c = ' ';
    }
    if (cx + cellW(sz) > 0) {
      ch[0] = (char)c;
      _display->setCursor(cx, y);
      _display->print(ch);
    }
    cx += cellW(sz);
  }
  _display->setTextSize(1);
}

// Single line; if wider than w it scrolls slowly left, pauses at both ends, then restarts.
// Meant for lines spanning (almost) the full display width, which does the clipping.
void UITask::renderMarquee(int x, int y, int w, const char* str, int sz) {
  int max_off = textWidth(str, sz) - w;
  if (max_off <= 0) {
    renderTextRaw(x, y, str, sz);
    return;
  }
  unsigned long scroll_ms = (unsigned long)max_off * 1000 / MARQUEE_PX_PER_SEC;
  unsigned long cycle = MARQUEE_PAUSE_MILLIS + scroll_ms + MARQUEE_PAUSE_MILLIS;
  unsigned long t = (millis() - _scroll_start) % cycle;
  int off;
  if (t < MARQUEE_PAUSE_MILLIS)                    off = 0;
  else if (t < MARQUEE_PAUSE_MILLIS + scroll_ms)   off = (t - MARQUEE_PAUSE_MILLIS) * MARQUEE_PX_PER_SEC / 1000;
  else                                             off = max_off;
  renderTextRaw(x - off, y, str, sz);
  _scrolling = true;
}

// draws the battery icon top right, returns its left x
int UITask::renderBattery() {
  uint16_t mv = getBattMilliVolts();
  int pct = ((int)mv - BATT_MIN_MILLIVOLTS) * 100 / (BATT_MAX_MILLIVOLTS - BATT_MIN_MILLIVOLTS);
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;

  const int w = 20, h = 9;
  int x = _display->width() - w - 3;
  _display->setColor(UIColor::title_txt);
  _display->drawRect(x, 0, w, h);
  _display->fillRect(x + w, 2, 2, h - 4);            // cap
  _display->fillRect(x + 2, 2, (w - 4) * pct / 100, h - 4);
  return x;
}

// header: nickname left, GPS status + battery icon right (BLE PIN only on boot and in pairing mode)
void UITask::renderHeader() {
  int gps_x = renderBattery() - 4 - 8;
  bool fix = _has_fix && millis() - _fix_millis < GPS_FIX_CURRENT_MILLIS;
  _display->setColor(UIColor::title_txt);
  _display->drawXbm(gps_x, 0, fix ? gps_fix_icon : gps_nofix_icon, 8, 8);
  int right = gps_x - CHAR_W;
  renderText(0, 1, right, _node_prefs->node_name);
  _display->fillRect(0, HEADER_H - 1, _display->width(), 1);
}

// button hint at the bottom, same place on every screen
void UITask::renderHint(const char* hint) {
  _display->setColor(UIColor::primary_txt);
  _display->fillRect(0, HINT_SEP_Y, _display->width(), 1);
  renderText(0, HINT_Y, _display->width(), hint);
}

void UITask::renderBoot() {
  _display->setColor(UIColor::title_txt);
  _display->setTextSize(2);
  _display->drawTextCentered(_display->width() / 2, 2, "MSPager");
  _display->setTextSize(1);
  _display->drawTextCentered(_display->width() / 2, 22, FIRMWARE_VERSION);
  _display->drawTextCentered(_display->width() / 2, 34, _node_prefs->node_name);
  if (the_mesh.getBLEPin() != 0) {
    char tmp[20];
    snprintf(tmp, sizeof(tmp), "BLE PIN: %lu", (unsigned long)the_mesh.getBLEPin());
    _display->drawTextCentered(_display->width() / 2, 48, tmp);
  }
}

void UITask::renderChat() {
  renderHeader();

  if (_inbox_count == 0) {
    if (the_mesh.findPagerChannel() < 0) {
      _display->drawTextCentered(_display->width() / 2, 22, "No '" PAGER_CHANNEL_NAME "' channel");
      _display->drawTextCentered(_display->width() / 2, 36, "Configure via app");
      renderHint("2x:off");
      return;
    }
    _display->drawTextCentered(_display->width() / 2, 28, "No messages yet");
    renderHint("2x:off hold:send");
    return;
  }

  if (_sel >= 0) {
    renderChatSelected();
    renderHint("1x:" ARROW_UP " 2x:" ARROW_LEFT " hold:open");
    return;
  }
  renderHint("1x:" ARROW_UP " 2x:off hold:send");

  // latest CHAT_LINES messages, newest at the bottom
  int first = _inbox_count > CHAT_LINES ? _inbox_count - CHAT_LINES : 0;
  for (int i = 0; i < CHAT_LINES && first + i < _inbox_count; i++) {
    renderChatLine(first + i, CHAT_Y0 + i * LINE_H);
  }
}

// one small chat line: "[markers]<sender>: <body>"
void UITask::renderChatLine(int idx, int y) {
  PagerMsg* m = msgAt(idx);
  char body[MAX_TEXT_LEN + 40];
  displayBody(body, sizeof(body), m);
  char line[MAX_TEXT_LEN + 40];
  snprintf(line, sizeof(line), "%s: %s", m->sender, body);

  // CP437 markers, printed raw (renderText treats bytes >= 0x80 as UTF-8)
  char prefix[3];
  int np = 0;
  if (m->unread) prefix[np++] = '\x07';  // bullet
  if (m->for_me) prefix[np++] = '\xAF';  // >>
  prefix[np] = 0;

  _display->setColor(UIColor::primary_txt);
  if (np > 0) {
    _display->setCursor(0, y);
    _display->print(prefix);
  }
  renderText(np * CHAR_W, y, _display->width() - np * CHAR_W, line);
}

// Selected message enlarged (FRD-004): one older line for context, then an inverted block
// with the sender (small) and the body at double size, then newer lines as space allows.
void UITask::renderChatSelected() {
  PagerMsg* m = msgAt(_sel);
  char body[MAX_TEXT_LEN + 40];
  displayBody(body, sizeof(body), m);
  int block_h = LINE_H + 16;           // sender row + one size-2 row (glyphs leave 2 blank rows at the bottom)

  int y = CHAT_Y0;
  if (_sel > 0) {                      // older message as context
    renderChatLine(_sel - 1, y);
    y += LINE_H;
  }

  _display->setColor(UIColor::title_txt);
  _display->fillRect(0, y, _display->width(), block_h);
  _display->setColor(UIColor::window_bkg);

  if (m->for_me) {
    _display->setCursor(1, y + 1);
    _display->print("\xAF");
    renderText(1 + CHAR_W, y + 1, _display->width() - 2 - CHAR_W, m->sender);
  } else {
    renderText(1, y + 1, _display->width() - 2, m->sender);
  }
  renderMarquee(1, y + 1 + LINE_H, _display->width() - 2, body, 2);   // long text scrolls
  _display->setColor(UIColor::primary_txt);

  y += block_h;
  for (int i = _sel + 1; i < _inbox_count && y + LINE_H <= HINT_SEP_Y; i++) {
    renderChatLine(i, y);
    y += LINE_H;
  }
}

// Detail (FRD-005): title = sender + relative position, body at double size,
// then absolute coordinates + message age.
void UITask::renderDetail() {
  PagerMsg* m = msgAt(_sel);
  if (m == NULL) { setScreen(Screen::CHAT); return; }

  char rel[24];
  relText(rel, sizeof(rel), m);
  int rel_w = textWidth(rel);
  _display->setColor(UIColor::title_txt);
  _display->fillRect(0, 0, _display->width(), 10);
  _display->setColor(UIColor::window_bkg);
  renderText(2, 1, _display->width() - rel_w - 8, m->sender);
  renderText(_display->width() - rel_w - 2, 1, rel_w, rel);

  _display->setColor(UIColor::primary_txt);
  renderMarquee(0, 20, _display->width(), m->body, 2);   // double size, long text scrolls

  char coords[32], age[16];
  const PagerPos& pos = m->pos;
  if (pos.known) {
    char lat[16], lon[16];
    formatCoord(lat, sizeof(lat), pos.lat_e6);
    formatCoord(lon, sizeof(lon), pos.lon_e6);
    snprintf(coords, sizeof(coords), "%s,%s", lat, lon);
  } else {
    snprintf(coords, sizeof(coords), "%s", pos.no_gps ? "no GPS fix" : "no position");
  }
  unsigned long secs = (millis() - m->rx_millis) / 1000;
  if (secs < 60)        snprintf(age, sizeof(age), "now");
  else if (secs < 3600) snprintf(age, sizeof(age), "%lumin", secs / 60);
  else                  snprintf(age, sizeof(age), "%luh", secs / 3600);
  int age_w = textWidth(age);
  renderText(0, 45, _display->width() - age_w - CHAR_W, coords);
  renderText(_display->width() - age_w, 45, age_w, age);

  renderHint("2x:" ARROW_LEFT " hold:reply");
}

void UITask::renderPicker(bool reply) {
  const char* const* opts = reply ? REPLY_OPTIONS : COMPOSE_OPTIONS;
  int n = reply ? NUM_REPLY : NUM_COMPOSE;

  char title[48], count[8];
  if (reply) {
    PagerMsg* m = msgAt(_sel);
    snprintf(title, sizeof(title), "Reply to %s", m ? (m->own ? "me" : m->sender) : "?");
  } else {
    snprintf(title, sizeof(title), "Send to group");
  }
  snprintf(count, sizeof(count), "%d/%d", _option + 1, n);
  int count_w = textWidth(count);
  _display->setColor(UIColor::title_txt);
  _display->fillRect(0, 0, _display->width(), 10);
  _display->setColor(UIColor::window_bkg);
  renderText(2, 1, _display->width() - count_w - 8, title);
  renderText(_display->width() - count_w - 2, 1, count_w, count);

  // PICKER_ROWS visible, scrolled to keep the current option on screen
  int first = _option >= PICKER_ROWS ? _option - PICKER_ROWS + 1 : 0;
  for (int i = first; i < n && i < first + PICKER_ROWS; i++) {
    int y = 13 + (i - first) * 10;
    if (i == _option) {
      _display->setColor(UIColor::title_txt);
      _display->fillRect(0, y - 1, _display->width(), 10);
      _display->setColor(UIColor::window_bkg);
    } else {
      _display->setColor(UIColor::primary_txt);
    }
    renderText(4, y, _display->width() - 8, opts[i]);
  }

  renderHint("1x:" ARROW_DOWN " 2x:" ARROW_LEFT " hold:send");
}

// ---------------------------------------------------------------- loop

void UITask::loop() {
  checkPairingHold();
  updateFix();
  ledLoop();

  if (_auto_reply_at && millis() >= _auto_reply_at) {
    _auto_reply_at = 0;
    sendCanned(LOC_SHARE, _auto_reply_to, "Location shared");   // visible in own chat (FRD-019)
  }

  int ev = user_btn.check();
  if (ev == BUTTON_EVENT_CLICK || ev == BUTTON_EVENT_LONG_PRESS || ev == BUTTON_EVENT_DOUBLE_CLICK) {
    if (_display != NULL && !_display->isOn()) {
      wake();                // FRD-003: wake press is consumed (shows the overview)
      if (_screen == Screen::CHAT) _led_alert = false;   // FRD-014: overview opened
    } else if (_led_alert && _screen == Screen::CHAT) {
      _led_alert = false;    // FRD-014: first press on the overview only acknowledges
      markVisibleRead();
      wake();
    } else {
      if (ev == BUTTON_EVENT_CLICK) handleShort();
      else if (ev == BUTTON_EVENT_LONG_PRESS) handleLong();
      else handleDouble();
      if (_display != NULL && _display->isOn()) wake();   // restart timeout, refresh
      if (_screen == Screen::CHAT) _led_alert = false;   // back on the overview
    }
  }

  if (_screen == Screen::PAIRING) {
    if (_pair_reenable_at && millis() >= _pair_reenable_at) {
      enableBluetooth();   // advertise again
      _pair_reenable_at = 0;
    } else if (!_pair_reenable_at) {
      if (!hasConnection()) {
        _pair_saw_idle = true;
      } else if (_pair_saw_idle) {
        endPairing("Connected");
      }
    }
    if (_screen == Screen::PAIRING) {
      if (millis() >= _pair_until) {
        endPairing("Pairing timeout");
      } else {
        _auto_off = millis() + AUTO_OFF_MILLIS;   // keep display on while pairing
      }
    }
  }

  if (_screen == Screen::BOOT && millis() >= _boot_until) {
    setScreen(Screen::CHAT);
  }

  int scroll_key = (int)_screen * 100 + _sel;
  if (scroll_key != _scroll_key) {   // new message or screen: marquee starts over
    _scroll_key = scroll_key;
    _scroll_start = millis();
  }

  if (_display != NULL && _display->isOn()) {
    if (millis() >= _next_refresh) {
      _scrolling = false;
      _display->startFrame();
      _display->setTextSize(1);
      switch (_screen) {
        case Screen::BOOT:    renderBoot(); break;
        case Screen::CHAT:    renderChat(); break;
        case Screen::DETAIL:  renderDetail(); break;
        case Screen::COMPOSE: renderPicker(false); break;
        case Screen::REPLY:   renderPicker(true); break;
        case Screen::PAIRING: renderPairing(); break;
      }
      if (millis() < _alert_expiry) {
        int y = _display->height() / 3;
        _display->setColor(UIColor::popup_bkg);
        _display->fillRect(4, y, _display->width() - 8, y);
        _display->setColor(UIColor::popup_txt);
        _display->drawRect(4, y, _display->width() - 8, y);
        _display->drawTextCentered(_display->width() / 2, y + 7, _alert);
        _next_refresh = _alert_expiry;
      } else {
        // marquee needs frames; otherwise ages, battery and GPS icon update once per second
        _next_refresh = millis() + (_scrolling ? MARQUEE_FRAME_MILLIS : 1000);
      }
      _display->endFrame();
    }
    if (millis() > _auto_off) {
      sleepDisplay();
    }
  }

#ifdef AUTO_SHUTDOWN_MILLIVOLTS
  if (millis() > _next_batt_chck) {
    uint16_t mv = getBattMilliVolts();
    if (mv > 0 && mv < AUTO_SHUTDOWN_MILLIVOLTS && !board.isExternalPowered()) {
      if (_display != NULL) {
        _display->turnOn();
        _display->startFrame();
        _display->setTextSize(2);
        _display->drawTextCentered(_display->width() / 2, 20, "Low Battery");
        _display->endFrame();
        delay(3000);
      }
      shutdown();
    }
    _next_batt_chck = millis() + 8000;
  }
#endif
}

void UITask::shutdown(bool restart) {
  if (restart) {
    _board->reboot();
  } else {
    _board->powerOff();
  }
}
