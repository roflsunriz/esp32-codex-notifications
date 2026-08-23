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
bool bootRawHigh = true;
bool bootStableHigh = true;
bool bootGestureArmed = false;
std::uint32_t bootChangedAt = 0;
std::uint32_t bootPressedAt = 0;

constexpr std::uint32_t kBootDebounceMs = 30;

void press(const InputAction& action) {
  activeAction = action;
  touchActive = true;
  Serial.printf("UI input kind=%u index=%d\n", static_cast<unsigned>(action.kind),
                static_cast<int>(action.index));
  if (action.kind == InputKind::None) return;
  if (action.kind == InputKind::PageSwitch) {
    ui.setPage(action.page);
    return;
  }
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

void wakeDisplay() {
  activeAction = {};
  touchActive = true;
  ui.setDisplayAwake(true);
  codex.wakeDisplay();
  Serial.println("UI wake touch consumed");
}

void initializeBootGesture() {
  bootRawHigh = digitalRead(board::kBootButtonPin) == HIGH;
  bootStableHigh = bootRawHigh;
  bootGestureArmed = bootStableHigh;
  bootChangedAt = millis();
  bootPressedAt = 0;
}

void updateBootGesture(std::uint32_t now) {
  const bool rawHigh = digitalRead(board::kBootButtonPin) == HIGH;
  if (rawHigh != bootRawHigh) {
    bootRawHigh = rawHigh;
    bootChangedAt = now;
  }
  if (rawHigh == bootStableHigh || now - bootChangedAt < kBootDebounceMs) return;

  bootStableHigh = rawHigh;
  if (!bootStableHigh) {
    if (bootGestureArmed) bootPressedAt = now;
    return;
  }

  if (bootPressedAt != 0) {
    const BootGesture gesture = bootGestureForDuration(now - bootPressedAt);
    if (gesture == BootGesture::RotateScreen) ui.toggleRotation();
    if (gesture == BootGesture::CalibrateTouch) ui.calibrateTouch();
  }
  bootPressedAt = 0;
  bootGestureArmed = true;
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
  if (activeAction.kind != InputKind::None) ui.showPressed(activeAction, false);
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
  initializeBootGesture();
  codex.begin();
  ui.setState(codex.snapshot(), millis());
  Serial.println("CODEX_CYD_READY");
}

void loop() {
  updateBootGesture(millis());
  std::int16_t x = 0;
  std::int16_t y = 0;
  const bool touched = ui.readTouch(x, y);
  const bool contactActive = ui.touchContactActive();
  const TouchTransition transition = touchTransition(touchActive, touched, contactActive);
  if (transition == TouchTransition::Press) {
    if (ui.displayAwake()) {
      press(actionAt(ui.page(), x, y));
    } else {
      wakeDisplay();
    }
  }
  if (transition == TouchTransition::Release) release();
  codex.poll();

  const CodexMicroState state = codex.snapshot();
  if (state.dirty || state.connected != lastConnected) {
    lastConnected = state.connected;
    ui.setState(state, millis());
  }
  ui.tick(millis());
  delay(2);
}
