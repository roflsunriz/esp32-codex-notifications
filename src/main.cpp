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
bool prevPressed = false;
bool lastConnected = false;
std::uint32_t lastIdleTimeoutSec = 0xFFFFFFFFU;
enum class DragKind : std::uint8_t { None, SleepMinutes, SleepHours, Scroll };
DragKind dragKind = DragKind::None;
std::int16_t dragStartY = 0;
std::int16_t dragStartScroll = 0;
bool bootRawHigh = true;
bool bootStableHigh = true;
bool bootGestureArmed = false;
std::uint32_t bootChangedAt = 0;
std::uint32_t bootPressedAt = 0;

constexpr std::uint32_t kBootDebounceMs = 30;

void press(const InputAction& action) {
  activeAction = action;
  touchActive = true;
  dragKind = DragKind::None;
  Serial.printf("UI input kind=%u index=%d\n", static_cast<unsigned>(action.kind),
                static_cast<int>(action.index));
  if (action.kind == InputKind::None) return;
  if (action.kind == InputKind::PageSwitch) {
    ui.setPage(action.page);
    return;
  }
  codex.noteDisplayActivity();
  if (action.kind == InputKind::SleepMinutes ||
      action.kind == InputKind::SleepHours) {
    const std::uint32_t current = ui.sleepTimeoutSec();
    const std::uint32_t minutes =
        action.kind == InputKind::SleepMinutes
            ? static_cast<std::uint32_t>(action.index)
            : sleep_menu::minutesPart(current);
    const std::uint32_t hours =
        action.kind == InputKind::SleepHours
            ? static_cast<std::uint32_t>(action.index)
            : sleep_menu::hoursPart(current);
    ui.setSleepTimeoutSec(sleep_menu::timeoutFromParts(minutes, hours));
    dragKind = action.kind == InputKind::SleepMinutes
                   ? DragKind::SleepMinutes
                   : DragKind::SleepHours;
    return;
  }
  if (action.kind == InputKind::NavigateScroll) {
    dragKind = DragKind::Scroll;
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
  dragKind = DragKind::None;
  ui.setDisplayAwake(true);
  codex.wakeDisplay();
  codex.noteDisplayActivity();
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
  dragKind = DragKind::None;
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
      std::int16_t scroll = 0;
      if (ui.page() == Page::Navigate) scroll = ui.navigateScroll();
      press(actionAt(ui.page(), x, y, scroll));
      if (ui.page() == Page::Navigate &&
          (dragKind == DragKind::SleepMinutes ||
           dragKind == DragKind::SleepHours ||
           dragKind == DragKind::Scroll)) {
        dragStartY = y;
        dragStartScroll = ui.navigateScroll();
      }
    } else {
      wakeDisplay();
    }
  }
  if (transition == TouchTransition::Release) release();
  // Contact-continuation drags on the Navigate tab. Taps are unaffected.
  if (touched && prevPressed && ui.displayAwake() &&
      ui.page() == Page::Navigate && dragKind != DragKind::None) {
    std::int16_t dragX = 0;
    std::int16_t dragY = 0;
    if (ui.readDragPoint(dragX, dragY)) {
      if (dragKind == DragKind::Scroll) {
        const std::int32_t delta =
            static_cast<std::int32_t>(dragStartY) - dragY;
        if (delta < -6 || delta > 6) {
          ui.setNavigateScroll(static_cast<std::int16_t>(dragStartScroll + delta));
        }
      } else if (dragX >= 16 && dragX <= 283) {
        const std::uint32_t current = ui.sleepTimeoutSec();
        if (dragKind == DragKind::SleepMinutes) {
          const std::uint32_t minutes = sleep_menu::sliderValueFromX(
              dragX, 0U, sleep_menu::kMinutesMax, 1U);
          if (minutes != sleep_menu::minutesPart(current)) {
            ui.setSleepTimeoutSec(sleep_menu::timeoutFromParts(
                minutes, sleep_menu::hoursPart(current)));
          }
        } else {
          const std::uint32_t hours = sleep_menu::sliderValueFromX(
              dragX, 0U, sleep_menu::kHoursMax, 1U);
          if (hours != sleep_menu::hoursPart(current)) {
            ui.setSleepTimeoutSec(sleep_menu::timeoutFromParts(
                sleep_menu::minutesPart(current), hours));
          }
        }
      }
    }
  }
  if (!touched) {
    dragKind = DragKind::None;
  }
  prevPressed = touched;
  if (ui.sleepTimeoutSec() != lastIdleTimeoutSec) {
    lastIdleTimeoutSec = ui.sleepTimeoutSec();
    codex.setDisplayIdleTimeout(lastIdleTimeoutSec);
  }
  codex.poll();

  const CodexMicroState state = codex.snapshot();
  if (state.dirty || state.connected != lastConnected) {
    lastConnected = state.connected;
    ui.setState(state, millis());
  }
  ui.tick(millis());
  delay(2);
}
