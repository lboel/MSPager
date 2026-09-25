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
#define PHONE_GLYPH_W  9   // 8px glyph + 1px spacing
static const uint8_t phone_glyph[8] = { 0x00, 0x3C, 0x7E, 0xE7, 0xC3, 0xC3, 0x00, 0x00 };

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
  return m;
}

void UITask::markVisibleRead() {
  int first = _inbox_count > CHAT_LINES ? _inbox_count - CHAT_LINES : 0;
  if (_sel >= 0 && _sel < first) first = _sel;
  for (int i = first; i < first + CHAT_LINES && i < _inbox_count; i++) {
    msgAt(i)->unread = false;
  }
}

// ---------------------------------------------------------------- AbstractUITask

void UITask::msgRead(int msgcount) {
  // offline queue drained by the app; pager inbox is independent
}

void UITask::newMsg(uint8_t path_len, const char* from_name, const char* text, int msgcount) {
  // Channel texts arrive as "<sender>: <body>". M1 shows every message;
  // filtering to the pager channel follows in M2 (FRD-008).
  const char* sep = strstr(text, ": ");
  if (sep && sep - text < 32) {
    char sender[32];
    snprintf(sender, sizeof(sender), "%.*s", (int)(sep - text), text);
    addMsg(sender, sep + 2, false);
  } else {
    addMsg(from_name, text, false);
  }

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
  // M1: local echo only. Sending on the pager channel follows in M2 (FRD-006/008).
  addMsg("me", body, true);
  showAlert("M1: local only", 1200);
}

// ---------------------------------------------------------------- rendering

int UITask::textWidth(const char* str) {
  int w = 0;
  for (const char* p = str; *p; ) {
    if (strncmp(p, PHONE_UTF8, 4) == 0) {
      w += PHONE_GLYPH_W;
      p += 4;
    } else {
      w += CHAR_W;
      p++;
      while ((*p & 0xC0) == 0x80) p++;  // one cell per UTF-8 codepoint
    }
  }
  return w;
}

// Draws text with the phone glyph inline; other non-ASCII codepoints become a block.
// Truncates with "..." if wider than max_w.
void UITask::renderText(int x, int y, int max_w, const char* str) {
  bool truncate = textWidth(str) > max_w;
  int limit = truncate ? max_w - 3 * CHAR_W : max_w;
  int cx = x;
  char ch[2] = { 0, 0 };

  for (const char* p = str; *p; ) {
    if (strncmp(p, PHONE_UTF8, 4) == 0) {
      if (cx + PHONE_GLYPH_W - x > limit) break;
      _display->drawXbm(cx, y, phone_glyph, 8, 8);
      cx += PHONE_GLYPH_W;
      p += 4;
      continue;
    }
    if (cx + CHAR_W - x > limit) break;
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
    cx += CHAR_W;
  }
  if (truncate) {
    _display->setCursor(cx, y);
    _display->print("...");
  }
}

void UITask::renderBattery() {
  uint16_t mv = getBattMilliVolts();
  int pct = ((int)mv - BATT_MIN_MILLIVOLTS) * 100 / (BATT_MAX_MILLIVOLTS - BATT_MIN_MILLIVOLTS);
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;

  const int w = 20, h = 9;
  int x = _display->width() - w - 3;
  _display->setColor(UIColor::title_txt);
  _display->drawRect(x, 0, w, h);
  _display->fillRect(x + w, 2, 2, h - 4);
  _display->fillRect(x + 2, 2, (w - 4) * pct / 100, h - 4);

  char tmp[8];
  snprintf(tmp, sizeof(tmp), "%d%%", pct);
  _display->drawTextRightAlign(x - 3, 1, tmp);
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
  renderBattery();
  _display->fillRect(0, HEADER_H - 1, _display->width(), 1);

  if (_inbox_count == 0) {
    _display->drawTextCentered(_display->width() / 2, 26, "No messages yet");
    _display->drawTextCentered(_display->width() / 2, 42, "Hold: send");
    return;
  }

  // window of CHAT_LINES messages ending at the newest, scrolled to keep the selection visible
  int first = _inbox_count > CHAT_LINES ? _inbox_count - CHAT_LINES : 0;
  if (_sel >= 0 && _sel < first) first = _sel;

  for (int i = 0; i < CHAT_LINES && first + i < _inbox_count; i++) {
    PagerMsg* m = msgAt(first + i);
    int y = CHAT_Y0 + i * LINE_H;
    bool selected = (first + i == _sel);

    char body[MAX_TEXT_LEN + 1];
    compactMention(body, sizeof(body), m->body);
    char line[MAX_TEXT_LEN + 40];
    snprintf(line, sizeof(line), "%s%s: %s", m->unread ? "\x07" : "", m->sender, body);

    if (selected) {
      _display->setColor(UIColor::title_txt);
      _display->fillRect(0, y - 1, _display->width(), LINE_H);
      _display->setColor(UIColor::window_bkg);
    } else {
      _display->setColor(UIColor::primary_txt);
    }
    renderText(0, y, _display->width(), line);
  }
  _display->setColor(UIColor::primary_txt);
}

void UITask::renderDetail() {
  PagerMsg* m = msgAt(_sel);
  if (m == NULL) { setScreen(Screen::CHAT); return; }

  // sender as inverted title bar
  _display->setColor(UIColor::title_txt);
  _display->fillRect(0, 0, _display->width(), 10);
  _display->setColor(UIColor::window_bkg);
  renderText(2, 1, _display->width() - 4, m->sender);

  _display->setColor(UIColor::primary_txt);
  renderText(0, 14, _display->width(), m->body);

  char tmp[24];
  formatAge(tmp, sizeof(tmp), m->rx_millis);
  renderText(0, 26, _display->width(), tmp);
  renderText(0, 36, _display->width(), "Position: unknown");   // M3: FRD-005/010

  _display->fillRect(0, 53, _display->width(), 1);
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
