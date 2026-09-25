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

#ifndef PAGER_CHANNEL_NAME
  #error "ui-pager requires PAGER_CHANNEL_NAME (see env heltec_v4_pager)"
#endif

#ifndef PAGER_DISPLAY_TIMEOUT_SECS
  #define PAGER_DISPLAY_TIMEOUT_SECS  15
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
};

class UITask : public AbstractUITask {
public:
  enum class Screen { BOOT, CHAT, DETAIL, COMPOSE, REPLY };

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

  PagerMsg* msgAt(int idx);
  PagerMsg* addMsg(const char* sender, const char* body, bool own);

  void handleShort();
  void handleLong();
  void handleDouble();
  void sendCanned(const char* text, const char* mention_to);
  void markVisibleRead();

  void setScreen(Screen s);
  void wake();
  void sleepDisplay();
  void showAlert(const char* text, int duration_millis);

  void renderBoot();
  void renderChat();
  void renderDetail();
  void renderPicker(bool reply);
  int  renderBattery();
  void renderHeader();
  void renderText(int x, int y, int max_w, const char* str);
  int  textWidth(const char* str);

public:
  UITask(mesh::MainBoard* board, MultiSerialInterface* serial)
    : AbstractUITask(board, serial), _display(NULL), _sensors(NULL), _node_prefs(NULL) {
    _screen = Screen::BOOT;
    _next_refresh = _auto_off = _boot_until = _next_batt_chck = 0;
    _alert_expiry = 0;
    _inbox_head = _inbox_count = 0;
    _sel = -1;
    _option = 0;
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
