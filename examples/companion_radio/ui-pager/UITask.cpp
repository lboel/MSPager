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
#define CHAT_Y0       14
#define CHAT_LINES    6

// canned message catalogue (FRD-006)
static const char* const COMPOSE_OPTIONS[] = { "Angekommen?", "Brauche Hilfe" };
static const char* const REPLY_OPTIONS[]   = { "Ja", "Nein", "OK", "\xF0\x9F\x93\x9E" };  // last: U+1F4DE
#define NUM_COMPOSE  (int)(sizeof(COMPOSE_OPTIONS) / sizeof(COMPOSE_OPTIONS[0]))
#define NUM_REPLY    (int)(sizeof(REPLY_OPTIONS) / sizeof(REPLY_OPTIONS[0]))

// U+1F4DE TELEPHONE RECEIVER, drawn as 8x8 glyph (FRD-013)
static const char PHONE_UTF8[] = "\xF0\x9F\x93\x9E";
static const uint8_t phone_glyph[8] = { 0x00, 0x3C, 0x7E, 0xE7, 0xC3, 0xC3, 0x00, 0x00 };
static const uint8_t phone_glyph_2x[32] = {   // pixel-doubled, 16x16
  0x00,0x00, 0x00,0x00, 0x0F,0xF0, 0x0F,0xF0, 0x3F,0xFC, 0x3F,0xFC, 0xFC,0x3F, 0xFC,0x3F,
  0xF0,0x0F, 0xF0,0x0F, 0xF0,0x0F, 0xF0,0x0F, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00 };

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

  if (_display != NULL) {
    _display->turnOn();
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

PagerMsg* UITask::addMsg(const char* sender, const char* body, bool own) {
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

  char mention[40];
  snprintf(mention, sizeof(mention), "@[%s] ", _node_prefs->node_name);
  m->for_me = !own && strncmp(body, mention, strlen(mention)) == 0;
  return m;
}

void UITask::markVisibleRead() {
  int first, last;
  if (_sel >= 0) {                 // selected layout: one older line, the block, a few newer lines
    first = _sel > 0 ? _sel - 1 : 0;
    last = _sel + 3;
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
  const char* body = text;
  if (sep && sep - text < (int)sizeof(sender)) {
    snprintf(sender, sizeof(sender), "%.*s", (int)(sep - text), text);
    body = sep + 2;
  } else {
    snprintf(sender, sizeof(sender), "?");
  }

  // own messages (sent from the app over BLE) are shown as "me" and don't alert
  if (strcmp(sender, _node_prefs->node_name) == 0) {
    addMsg("me", body, true);
    _next_refresh = 0;
    return;
  }

  addMsg(sender, body, false);
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

void UITask::sendCanned(const char* text, const char* mention_to) {
  char body[MAX_TEXT_LEN + 1];
  if (mention_to) {
    snprintf(body, sizeof(body), "@[%s] %s", mention_to, text);
  } else {
    snprintf(body, sizeof(body), "%s", text);
  }
  if (the_mesh.sendPagerMessage(body)) {
    addMsg("me", body, true);
    showAlert("Sent", 1000);
  } else {
    showAlert("Send failed", 1500);
  }
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
  _display->drawTextCentered(_display->width() / 2, 48, tmp);
  _display->drawTextCentered(_display->width() / 2, 56, "press: cancel");
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
    } else if (c < 32) {
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

// Word-wraps str into at most max_lines lines (the last one ellipsized). Returns lines used.
// With draw=false it only measures.
int UITask::renderWrapped(int x, int y, int max_w, const char* str, int sz, int max_lines, bool draw) {
  char line[MAX_TEXT_LEN + 1];
  const char* p = str;
  int n = 0;
  while (*p && n < max_lines) {
    while (*p == ' ') p++;
    if (!*p) break;
    int len;
    if (n == max_lines - 1) {
      len = strlen(p);                  // last line: the rest, truncated by renderText
    } else {
      len = 0;
      for (const char* q = p; ; ) {     // take whole words while they fit
        const char* wend = q;
        while (*wend && *wend != ' ') wend++;
        int cand = wend - p;
        memcpy(line, p, cand);
        line[cand] = 0;
        if (len > 0 && textWidth(line, sz) > max_w) break;
        len = cand;
        if (!*wend) break;
        q = wend + 1;
      }
    }
    memcpy(line, p, len);
    line[len] = 0;
    if (draw) renderText(x, y + n * 8 * sz, max_w, line, sz);
    n++;
    p += len;
  }
  return n;
}

// draws the battery percentage top right, returns its left x
int UITask::renderBattery() {
  uint16_t mv = getBattMilliVolts();
  int pct = ((int)mv - BATT_MIN_MILLIVOLTS) * 100 / (BATT_MAX_MILLIVOLTS - BATT_MIN_MILLIVOLTS);
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;

  char tmp[8];
  snprintf(tmp, sizeof(tmp), "%d%%", pct);
  int x = _display->width() - textWidth(tmp);
  _display->setColor(UIColor::title_txt);
  _display->setCursor(x, 1);
  _display->print(tmp);
  return _display->width() - textWidth("100%");   // fixed column, so the PIN doesn't shift
}

// header: "<name>      <pin> <pct>" - name left, BLE PIN right-aligned next to the battery percentage.
// 4px gaps leave exactly 10 chars for the name next to a 6-digit PIN and "100%".
#define HEADER_GAP  4

void UITask::renderHeader() {
  int right = renderBattery() - HEADER_GAP;

  if (the_mesh.getBLEPin() != 0) {
    char pin[12];
    snprintf(pin, sizeof(pin), "%lu", (unsigned long)the_mesh.getBLEPin());
    right -= textWidth(pin);
    _display->setCursor(right, 1);
    _display->print(pin);
    right -= HEADER_GAP;
  }
  renderText(0, 1, right, _node_prefs->node_name);
  _display->fillRect(0, HEADER_H - 1, _display->width(), 1);
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
      _display->drawTextCentered(_display->width() / 2, 26, "No '" PAGER_CHANNEL_NAME "' channel");
      _display->drawTextCentered(_display->width() / 2, 42, "Configure via app");
      return;
    }
    _display->drawTextCentered(_display->width() / 2, 26, "No messages yet");
    _display->drawTextCentered(_display->width() / 2, 42, "Hold: send");
    return;
  }

  if (_sel >= 0) {
    renderChatSelected();
    return;
  }

  // latest CHAT_LINES messages, newest at the bottom
  int first = _inbox_count > CHAT_LINES ? _inbox_count - CHAT_LINES : 0;
  for (int i = 0; i < CHAT_LINES && first + i < _inbox_count; i++) {
    renderChatLine(first + i, CHAT_Y0 + i * LINE_H);
  }
}

// one small chat line: "[markers]<sender>: <body>"
void UITask::renderChatLine(int idx, int y) {
  PagerMsg* m = msgAt(idx);
  char body[MAX_TEXT_LEN + 1];
  compactMention(body, sizeof(body), m->body);
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
  int y = CHAT_Y0 - 1;

  if (_sel > 0) {
    renderChatLine(_sel - 1, y + 1);
    y += LINE_H + 1;
  }

  char body[MAX_TEXT_LEN + 1];
  compactMention(body, sizeof(body), m->body);
  int lines = renderWrapped(0, 0, _display->width() - 2, body, 2, 2, false);
  int block_h = 1 + LINE_H + lines * 16 + 1;

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
  renderWrapped(1, y + 1 + LINE_H, _display->width() - 2, body, 2, 2, true);
  _display->setColor(UIColor::primary_txt);

  y += block_h + 1;
  for (int i = _sel + 1; i < _inbox_count && y + LINE_H <= _display->height(); i++) {
    renderChatLine(i, y);
    y += LINE_H;
  }
}

void UITask::renderDetail() {
  PagerMsg* m = msgAt(_sel);
  if (m == NULL) { setScreen(Screen::CHAT); return; }

  // title bar: sender left, age right
  char age[24];
  formatAge(age, sizeof(age), m->rx_millis);
  int age_w = textWidth(age);
  _display->setColor(UIColor::title_txt);
  _display->fillRect(0, 0, _display->width(), 10);
  _display->setColor(UIColor::window_bkg);
  renderText(2, 1, _display->width() - age_w - 8, m->sender);
  renderText(_display->width() - age_w - 2, 1, age_w, age);

  // message body at double size (FRD-005), up to 2 lines
  _display->setColor(UIColor::primary_txt);
  renderWrapped(0, 13, _display->width(), m->body, 2, 2, true);

  renderText(0, 46, _display->width(), "Position: unknown");   // M3: FRD-005/010
  renderText(0, 56, _display->width(), "Hold:reply  2x:back");
}

void UITask::renderPicker(bool reply) {
  const char* const* opts = reply ? REPLY_OPTIONS : COMPOSE_OPTIONS;
  int n = reply ? NUM_REPLY : NUM_COMPOSE;

  char title[48];
  if (reply) {
    PagerMsg* m = msgAt(_sel);
    snprintf(title, sizeof(title), "Reply to %s", m ? (m->own ? "me" : m->sender) : "?");
  } else {
    snprintf(title, sizeof(title), "Send to group");
  }
  _display->setColor(UIColor::title_txt);
  _display->fillRect(0, 0, _display->width(), 10);
  _display->setColor(UIColor::window_bkg);
  renderText(2, 1, _display->width() - 4, title);

  for (int i = 0; i < n; i++) {
    int y = 13 + i * 10;
    if (i == _option) {
      _display->setColor(UIColor::title_txt);
      _display->fillRect(0, y - 1, _display->width(), 10);
      _display->setColor(UIColor::window_bkg);
    } else {
      _display->setColor(UIColor::primary_txt);
    }
    renderText(4, y, _display->width() - 8, opts[i]);
  }

  _display->setColor(UIColor::primary_txt);
  _display->fillRect(0, 53, _display->width(), 1);
  renderText(0, 56, _display->width(), "Hold:send  2x:cancel");
}

// ---------------------------------------------------------------- loop

void UITask::loop() {
  checkPairingHold();

  int ev = user_btn.check();
  if (ev == BUTTON_EVENT_CLICK || ev == BUTTON_EVENT_LONG_PRESS || ev == BUTTON_EVENT_DOUBLE_CLICK) {
    if (_display != NULL && !_display->isOn()) {
      wake();                // FRD-003: wake press is consumed
    } else {
      if (ev == BUTTON_EVENT_CLICK) handleShort();
      else if (ev == BUTTON_EVENT_LONG_PRESS) handleLong();
      else handleDouble();
      if (_display != NULL && _display->isOn()) wake();   // restart timeout, refresh
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

  if (_display != NULL && _display->isOn()) {
    if (millis() >= _next_refresh) {
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
        _next_refresh = millis() + 1000;   // ages and battery update once per second
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
