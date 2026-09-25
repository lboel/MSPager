#pragma once

// MSPager UI: single-button pager interface for the Heltec V4 OLED.
// Spec: FRD-002 (buttons), FRD-003 (display power), FRD-004 (chat screen), FRD-016 (inbox)

#include <MeshCore.h>
#include <helpers/ui/DisplayDriver.h>
#include <helpers/ui/UIScreen.h>
#include <helpers/SensorManager.h>
#include <helpers/MultiSerialInterface.h>
#include <helpers/BaseChatMesh.h>
#include <Arduino.h>

#include "../AbstractUITask.h"
#include "../NodePrefs.h"
#include "PagerLocation.h"

#ifndef PAGER_CHANNEL_NAME
  #error "ui-pager requires PAGER_CHANNEL_NAME (see env heltec_v4_pager)"
#endif

#ifndef PAGER_DISPLAY_TIMEOUT_SECS
  #define PAGER_DISPLAY_TIMEOUT_SECS  15
#endif

#ifndef PAGER_PAIRING_HOLD_MILLIS
  #define PAGER_PAIRING_HOLD_MILLIS  10000   // hold PRG this long to enter BLE pairing mode (FRD-017)
#endif
#ifndef PAGER_PAIRING_SECS
  #define PAGER_PAIRING_SECS  30
#endif

#ifndef PAGER_INBOX_SIZE
  #define PAGER_INBOX_SIZE  16
#endif

struct PagerMsg {
  char sender[32];
  char body[MAX_TEXT_LEN + 1];
  unsigned long rx_millis;
  bool own;
  bool unread;
  bool for_me;    // mentions this pager's nickname (FRD-007)
  PagerPos pos;   // sender position from the message suffix (FRD-010)
};

class UITask : public AbstractUITask {
public:
  enum class Screen { BOOT, CHAT, DETAIL, COMPOSE, REPLY, PAIRING };

private:
  DisplayDriver* _display;
  SensorManager* _sensors;
  NodePrefs* _node_prefs;

  Screen _screen;
  unsigned long _next_refresh, _auto_off, _boot_until, _next_batt_chck;
  char _alert[40];
  unsigned long _alert_expiry;

  // inbox ring buffer, chronological access via msgAt(0 = oldest)
  PagerMsg _inbox[PAGER_INBOX_SIZE];
  int _inbox_head, _inbox_count;
  int _sel;       // selected message index (chronological), -1 = none
  int _option;    // current option in compose/reply picker

  // BLE pairing mode (FRD-017)
  unsigned long _hold_start;      // PRG press start, 0 = released
  bool _hold_fired;               // pairing already triggered by this hold
  unsigned long _pair_until;      // pairing mode timeout
  unsigned long _pair_reenable_at;  // BLE restart after dropping the old link, 0 = done
  bool _pair_saw_idle;            // saw "not connected" after restart, so the next connect is new

  PagerMsg* msgAt(int idx);
  PagerMsg* addMsg(const char* sender, const char* body, bool own, const PagerPos& pos);

  // own GPS fix (FRD-011)
  bool _has_fix;
  long _fix_lat_e6, _fix_lon_e6;
  unsigned long _fix_millis, _next_fix_check;
  void updateFix();
  PagerPos currentPos();

  // automatic answer to "Standort?" (FRD-019)
  unsigned long _auto_reply_at;
  char _auto_reply_to[32];
  void checkLocationRequest(const char* sender, const char* body);

  void relText(char* dest, size_t dest_size, const PagerMsg* m);
  void displayBody(char* dest, size_t dest_size, const PagerMsg* m);

  void handleShort();
  void handleLong();
  void handleDouble();
  void sendCanned(const char* text, const char* mention_to, const char* alert = "Sent");
  void markVisibleRead();
  void checkPairingHold();
  void startPairing();
  void endPairing(const char* alert);

  void setScreen(Screen s);
  void wake();
  void sleepDisplay();
  void showAlert(const char* text, int duration_millis);

  void renderBoot();
  void renderChat();
  void renderDetail();
  void renderPicker(bool reply);
  void renderPairing();
  int  renderBattery();
  void renderHeader();
  void renderHint(const char* hint);
  void renderChatLine(int idx, int y);
  void renderChatSelected();
  void renderText(int x, int y, int max_w, const char* str, int sz = 1);
  int  renderWrapped(int x, int y, int max_w, const char* str, int sz, int max_lines, bool draw);
  int  textWidth(const char* str, int sz = 1);

public:
  UITask(mesh::MainBoard* board, MultiSerialInterface* serial)
    : AbstractUITask(board, serial), _display(NULL), _sensors(NULL), _node_prefs(NULL) {
    _screen = Screen::BOOT;
    _next_refresh = _auto_off = _boot_until = _next_batt_chck = 0;
    _has_fix = false;
    _fix_lat_e6 = _fix_lon_e6 = 0;
    _fix_millis = _next_fix_check = _auto_reply_at = 0;
    _auto_reply_to[0] = 0;
    _alert_expiry = 0;
    _inbox_head = _inbox_count = 0;
    _sel = -1;
    _option = 0;
    _hold_start = _pair_until = _pair_reenable_at = 0;
    _hold_fired = _pair_saw_idle = false;
  }

  void begin(DisplayDriver* display, SensorManager* sensors, NodePrefs* node_prefs);
  bool hasDisplay() const { return _display != NULL; }

  // from AbstractUITask
  void msgRead(int msgcount) override;
  void newMsg(uint8_t path_len, const char* from_name, const char* text, int msgcount) override;
  void notify(UIEventType t = UIEventType::none) override;
  void loop() override;

  void shutdown(bool restart = false);
};
