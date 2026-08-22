#include <Arduino.h>

#include "board-config.h"
#include "codex-micro-ble.h"
#include "device-ui.h"
#include "ui-model.h"

namespace {

CodexMicroBle codex;
DeviceUi ui;
InputAction activeAction;
bool touchActive = false;
bool lastConnected = false;

void press(const InputAction& action) {
  if (action.kind == InputKind::None) return;
  if (action.kind == InputKind::PageSwitch) {
    activeAction = action;
    touchActive = true;
    ui.setPage(action.page);
    return;
  }

  activeAction = action;
  touchActive = true;
  ui.showPressed(action, true);
  if (action.kind == InputKind::AgentKey) {
    codex.sendKey(protocolKeyFor(action), 1, action.index);
  } else if (action.kind == InputKind::CommandKey) {
    codex.sendKey(protocolKeyFor(action), 1);
  } else if (action.kind == InputKind::EncoderStep) {
    codex.sendKey(protocolKeyFor(action), 2);
  } else if (action.kind == InputKind::EncoderPress) {
    codex.sendKey(protocolKeyFor(action), 1);
  } else if (action.kind == InputKind::Joystick) {
    codex.sendJoystick(action.angle, 1.0F);
  }
}

void release() {
  if (!touchActive) return;
  if (activeAction.kind == InputKind::AgentKey) {
    codex.sendKey(protocolKeyFor(activeAction), 0, activeAction.index);
  } else if (activeAction.kind == InputKind::CommandKey) {
    codex.sendKey(protocolKeyFor(activeAction), 0);
  } else if (activeAction.kind == InputKind::EncoderPress) {
    codex.sendKey(protocolKeyFor(activeAction), 0);
  } else if (activeAction.kind == InputKind::Joystick) {
    codex.sendJoystick(activeAction.angle, 0.0F);
  }
  ui.showPressed(activeAction, false);
  activeAction = {};
  touchActive = false;
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(100);
  pinMode(board::kBootButtonPin, INPUT_PULLUP);
  const bool calibrate = digitalRead(board::kBootButtonPin) == LOW;

  ui.begin();
  if (calibrate) ui.calibrateTouch();
  codex.begin();
  ui.setState(codex.snapshot(), millis());
  Serial.println("CODEX_CYD_READY");
}

void loop() {
  codex.poll();
  std::int16_t x = 0;
  std::int16_t y = 0;
  const bool touched = ui.readTouch(x, y);
  if (touched && !touchActive) press(actionAt(ui.page(), x, y));
  if (!touched && touchActive) release();

  const CodexMicroState state = codex.snapshot();
  if (state.dirty || state.connected != lastConnected) {
    lastConnected = state.connected;
    ui.setState(state, millis());
  }
  ui.tick(millis());
  delay(8);
}
