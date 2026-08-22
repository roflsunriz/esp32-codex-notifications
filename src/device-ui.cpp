#include "device-ui.h"

#include <Preferences.h>

#include <algorithm>
#include <cmath>

#include "board-config.h"

namespace {

constexpr std::uint16_t kBackground = 0x0841;
constexpr std::uint16_t kPanel = 0x18E3;
constexpr std::uint16_t kPanelPressed = 0x31A6;
constexpr std::uint16_t kText = 0xFFFF;
constexpr std::uint16_t kMuted = 0x8410;
constexpr std::uint16_t kAccent = 0x2E73;
constexpr std::uint32_t kCalibrationVersion = 1;
constexpr std::uint32_t kOrientationVersion = 1;
constexpr std::uint32_t kNotificationDurationMs = 4000;
constexpr std::uint32_t kAnimationIntervalMs = 90;

std::int16_t mapAxis(std::int16_t raw, std::int16_t rawStart, std::int16_t rawEnd,
                     std::int16_t screenStart, std::int16_t screenEnd,
                     std::int16_t screenMaximum) {
  const long denominator = static_cast<long>(rawEnd) - rawStart;
  if (std::abs(denominator) < 100) return screenStart;
  const long value = screenStart +
                     (static_cast<long>(raw) - rawStart) * (screenEnd - screenStart) /
                         denominator;
  return static_cast<std::int16_t>(std::max<long>(0, std::min<long>(screenMaximum, value)));
}

}  // namespace

DeviceUi::DeviceUi()
    : touchBus_(VSPI),
      touch_(board::kTouchChipSelectPin, board::kTouchIrqPin,
             board::kTouchPressureMinimum),
      calibration_{board::kDefaultTouchLeft, board::kDefaultTouchRight,
                   board::kDefaultTouchTop, board::kDefaultTouchBottom} {}

void DeviceUi::begin() {
  pinMode(board::kBacklightPin, OUTPUT);
  digitalWrite(board::kBacklightPin, HIGH);
  loadOrientation();
  display_.init();
  applyOrientation();
  display_.setTextWrap(false);
  display_.fillScreen(kBackground);

  touchBus_.begin(board::kTouchClockPin, board::kTouchMisoPin, board::kTouchMosiPin,
                  board::kTouchChipSelectPin);
  touch_.begin(touchBus_);
  touch_.setRotation(1);
  loadCalibration();
  for (auto& status : statuses_) status = StatusKind::Unassigned;
  drawAll();
}

void DeviceUi::loadCalibration() {
  Preferences preferences;
  // 初回起動ではnamespaceがまだ無い。read-writeで開いて空namespaceを正常に作る。
  if (!preferences.begin("codex-touch", false)) return;
  if (preferences.getUInt("version", 0) == kCalibrationVersion) {
    TouchCalibration candidate{
        preferences.getShort("left", calibration_.left),
        preferences.getShort("right", calibration_.right),
        preferences.getShort("top", calibration_.top),
        preferences.getShort("bottom", calibration_.bottom),
    };
    if (std::abs(candidate.right - candidate.left) > 1000 &&
        std::abs(candidate.bottom - candidate.top) > 1000) {
      calibration_ = candidate;
    }
  }
  preferences.end();
}

void DeviceUi::saveCalibration() {
  Preferences preferences;
  if (!preferences.begin("codex-touch", false)) return;
  preferences.clear();
  preferences.putUInt("version", kCalibrationVersion);
  preferences.putShort("left", calibration_.left);
  preferences.putShort("right", calibration_.right);
  preferences.putShort("top", calibration_.top);
  preferences.putShort("bottom", calibration_.bottom);
  preferences.end();
}

void DeviceUi::loadOrientation() {
  Preferences preferences;
  if (!preferences.begin("codex-ui", false)) return;
  if (preferences.getUInt("version", 0) != kOrientationVersion) {
    preferences.clear();
    preferences.putUInt("version", kOrientationVersion);
    preferences.putBool("inverted", false);
  }
  inverted_ = preferences.getBool("inverted", false);
  preferences.end();
}

void DeviceUi::saveOrientation() {
  Preferences preferences;
  if (!preferences.begin("codex-ui", false)) return;
  preferences.putUInt("version", kOrientationVersion);
  preferences.putBool("inverted", inverted_);
  preferences.end();
}

void DeviceUi::applyOrientation() {
  display_.setRotation(inverted_ ? 3 : 1);
}

bool DeviceUi::captureCalibrationPoint(std::int16_t& rawX, std::int16_t& rawY) {
  const std::uint32_t timeout = millis() + 15000;
  while (static_cast<std::int32_t>(timeout - millis()) > 0) {
    if (touch_.tirqTouched()) {
      std::int32_t sumX = 0;
      std::int32_t sumY = 0;
      std::int16_t samples = 0;
      while (touch_.tirqTouched() && samples < 12) {
        const SensitiveTouchPoint point = touch_.getPoint();
        if (point.z >= board::kTouchPressureMinimum) {
          sumX += point.x;
          sumY += point.y;
          ++samples;
        }
        delay(12);
      }
      while (touch_.tirqTouched()) {
        touch_.getPoint();
        delay(10);
      }
      if (samples >= 4) {
        rawX = static_cast<std::int16_t>(sumX / samples);
        rawY = static_cast<std::int16_t>(sumY / samples);
        return true;
      }
    }
    delay(10);
  }
  return false;
}

void DeviceUi::calibrateTouch() {
  std::int16_t left = 0;
  std::int16_t top = 0;
  std::int16_t right = 0;
  std::int16_t bottom = 0;

  // Calibration値は常にrotation 1の物理座標として保存する。
  display_.setRotation(1);
  const auto finish = [this]() {
    applyOrientation();
    drawAll();
  };

  display_.fillScreen(kBackground);
  display_.setTextColor(kText, kBackground);
  display_.setTextDatum(MC_DATUM);
  display_.drawString("TOUCH 1/2", 160, 120, 2);
  display_.drawLine(14, 24, 34, 24, kAccent);
  display_.drawLine(24, 14, 24, 34, kAccent);
  if (!captureCalibrationPoint(left, top)) {
    finish();
    return;
  }

  display_.fillScreen(kBackground);
  display_.drawString("TOUCH 2/2", 160, 120, 2);
  display_.drawLine(285, 215, 305, 215, kAccent);
  display_.drawLine(295, 205, 295, 225, kAccent);
  if (!captureCalibrationPoint(right, bottom)) {
    finish();
    return;
  }

  if (std::abs(right - left) > 1000 && std::abs(bottom - top) > 1000) {
    calibration_ = {left, right, top, bottom};
    saveCalibration();
  }
  finish();
}

bool DeviceUi::readTouch(std::int16_t& x, std::int16_t& y) {
  if (!touch_.tirqTouched()) return false;
  const SensitiveTouchPoint point = touch_.getPoint();
  if (point.z < board::kTouchPressureMinimum) return false;
  const ScreenPoint oriented = orientPoint(
      {mapAxis(point.x, calibration_.left, calibration_.right, 24, 295,
               board::kScreenWidth - 1),
       mapAxis(point.y, calibration_.top, calibration_.bottom, 24, 215,
               board::kScreenHeight - 1)},
      inverted_);
  x = oriented.x;
  y = oriented.y;
  return true;
}

void DeviceUi::setState(const CodexMicroState& state, std::uint32_t now) {
  for (std::size_t index = 0; index < state.threads.size(); ++index) {
    const StatusKind next =
        statusKindForColor(state.threads[index].color, state.threads[index].brightness);
    if (next != statuses_[index] && isNotificationStatus(next)) {
      notificationAgent_ = static_cast<std::int8_t>(index);
      notificationStatus_ = next;
      notificationUntil_ = now + kNotificationDurationMs;
    }
    statuses_[index] = next;
  }
  state_ = state;
  drawAll();
}

void DeviceUi::setPage(Page page) {
  page_ = page;
  pressed_ = false;
  drawAll();
}

void DeviceUi::toggleRotation() {
  inverted_ = !inverted_;
  saveOrientation();
  applyOrientation();
  pressed_ = false;
  drawAll();
  Serial.printf("UI rotation=%s\n", inverted_ ? "inverted" : "normal");
}

void DeviceUi::showPressed(const InputAction& action, bool pressed) {
  pressedAction_ = action;
  pressed_ = pressed;
  drawAll();
}

void DeviceUi::tick(std::uint32_t now) {
  if (notificationAgent_ >= 0 &&
      static_cast<std::int32_t>(now - notificationUntil_) >= 0) {
    notificationAgent_ = -1;
    drawHeader();
  }
  if (page_ == Page::Agents && now - lastAnimation_ >= kAnimationIntervalMs) {
    bool animated = false;
    for (const ThreadLight& light : state_.threads) {
      animated = animated || light.effect == "breath";
    }
    if (animated) drawAgents();
    lastAnimation_ = now;
  }
}

void DeviceUi::drawAll() {
  display_.fillScreen(kBackground);
  drawHeader();
  if (page_ == Page::Agents) drawAgents();
  if (page_ == Page::Commands) drawCommands();
  if (page_ == Page::Navigate) drawNavigate();
  drawTabs();
}

void DeviceUi::drawHeader() {
  display_.fillRect(0, 0, 320, 28, kBackground);
  display_.setTextDatum(ML_DATUM);
  display_.setTextColor(kText, kBackground);
  display_.drawString("CODEX", 8, 14, 2);

  if (notificationAgent_ >= 0) {
    const ThreadLight& light = state_.threads[notificationAgent_];
    const std::uint16_t color = lightColor(light);
    display_.fillRoundRect(128, 3, 106, 22, 5, kPanel);
    display_.setTextDatum(MC_DATUM);
    char label[8];
    snprintf(label, sizeof(label), "A%d", static_cast<int>(notificationAgent_) + 1);
    display_.drawString(label, 153, 14, 2);
    drawStatusIcon(notificationStatus_, 207, 14, color);
  }

  const std::uint16_t connectionColor = state_.connected ? 0x07E0 : 0xF800;
  display_.drawLine(286, 8, 286, 20, connectionColor);
  display_.drawLine(286, 8, 294, 14, connectionColor);
  display_.drawLine(294, 14, 286, 20, connectionColor);
  display_.drawLine(286, 8, 292, 4, connectionColor);
  display_.drawLine(286, 20, 292, 24, connectionColor);
  display_.fillCircle(309, 14, 4, connectionColor);
}

void DeviceUi::drawTabs() {
  for (std::uint8_t index = 0; index < 3; ++index) {
    const std::int16_t x = static_cast<std::int16_t>(index * 107);
    const std::int16_t width = index == 2 ? 106 : 107;
    const bool selected = static_cast<std::uint8_t>(page_) == index;
    display_.fillRect(x, 208, width, 32, selected ? kAccent : kPanel);
    const std::uint16_t color = selected ? kText : kMuted;
    const std::int16_t center = x + width / 2;
    if (index == 0) {
      for (int row = 0; row < 2; ++row) {
        for (int column = 0; column < 3; ++column) {
          display_.drawRect(center - 14 + column * 10, 216 + row * 9, 7, 6, color);
        }
      }
    } else if (index == 1) {
      const std::int16_t pointsX[] = {
          static_cast<std::int16_t>(center + 2), static_cast<std::int16_t>(center - 5),
          center, static_cast<std::int16_t>(center - 3),
          static_cast<std::int16_t>(center + 7), static_cast<std::int16_t>(center + 2)};
      const std::int16_t pointsY[] = {212, 224, 224, 236, 222, 222};
      for (int point = 0; point < 5; ++point) {
        display_.drawLine(pointsX[point], pointsY[point], pointsX[point + 1],
                          pointsY[point + 1], color);
      }
    } else {
      drawArrow(center, 224, 0, -1, color);
      drawArrow(center, 224, 1, 0, color);
      drawArrow(center, 224, 0, 1, color);
      drawArrow(center, 224, -1, 0, color);
    }
  }
}

std::uint16_t DeviceUi::lightColor(const ThreadLight& light, float pulse) {
  const float brightness = std::max(0.0F, std::min(1.0F, light.brightness * pulse));
  const auto red = static_cast<std::uint8_t>(((light.color >> 16U) & 0xFFU) * brightness);
  const auto green = static_cast<std::uint8_t>(((light.color >> 8U) & 0xFFU) * brightness);
  const auto blue = static_cast<std::uint8_t>((light.color & 0xFFU) * brightness);
  return display_.color565(red, green, blue);
}

bool DeviceUi::actionIsPressed(InputKind kind, std::int8_t index) const {
  return pressed_ && pressedAction_.kind == kind &&
         (index < 0 || pressedAction_.index == index);
}

void DeviceUi::drawButton(std::int16_t x, std::int16_t y, std::int16_t width,
                          std::int16_t height, bool pressed, std::uint16_t border) {
  display_.fillRoundRect(x, y, width, height, 6, pressed ? kPanelPressed : kPanel);
  display_.drawRoundRect(x, y, width, height, 6, border);
}

void DeviceUi::drawAgents() {
  for (std::uint8_t index = 0; index < 6; ++index) drawAgent(index);
}

void DeviceUi::drawAgent(std::uint8_t index) {
  const std::int16_t x = 4 + (index % 3) * 106;
  const std::int16_t y = 32 + (index / 3) * 87;
  const ThreadLight& light = state_.threads[index];
  float pulse = 1.0F;
  if (light.effect == "breath") {
    pulse = 0.55F + 0.45F * (std::sin(millis() * 0.006F) * 0.5F + 0.5F);
  }
  std::uint16_t color = light.brightness <= 0.01F ? 0x4208 : lightColor(light, pulse);
  drawButton(x, y, 100, 80, actionIsPressed(InputKind::AgentKey, index), color);
  display_.setTextDatum(MC_DATUM);
  display_.setTextColor(kText, actionIsPressed(InputKind::AgentKey, index) ? kPanelPressed
                                                                          : kPanel);
  char number[5];
  snprintf(number, sizeof(number), "%u", static_cast<unsigned int>(index) + 1U);
  display_.drawString(number, x + 30, y + 40, 4);
  drawStatusIcon(statuses_[index], x + 72, y + 40, color);
}

void DeviceUi::drawStatusIcon(StatusKind status, std::int16_t x, std::int16_t y,
                              std::uint16_t color) {
  if (status == StatusKind::Unassigned) {
    display_.drawCircle(x, y, 10, kMuted);
  } else if (status == StatusKind::Idle) {
    display_.drawFastHLine(x - 10, y, 20, color);
  } else if (status == StatusKind::Thinking) {
    display_.fillCircle(x - 9, y, 3, color);
    display_.fillCircle(x, y, 3, color);
    display_.fillCircle(x + 9, y, 3, color);
  } else if (status == StatusKind::Complete) {
    display_.drawLine(x - 10, y, x - 2, y + 8, color);
    display_.drawLine(x - 2, y + 8, x + 12, y - 9, color);
  } else if (status == StatusKind::Attention) {
    display_.drawFastVLine(x, y - 11, 16, color);
    display_.fillCircle(x, y + 10, 2, color);
  } else {
    display_.drawLine(x - 9, y - 9, x + 9, y + 9, color);
    display_.drawLine(x + 9, y - 9, x - 9, y + 9, color);
  }
}

void DeviceUi::drawCommands() {
  for (std::uint8_t index = 0; index < 6; ++index) {
    const std::int16_t x = 4 + (index % 3) * 106;
    const std::int16_t y = 32 + (index / 3) * 87;
    const bool pressed = actionIsPressed(InputKind::CommandKey, index);
    drawButton(x, y, 100, 80, pressed, index == 1 ? 0x07E0 : kAccent);
    drawCommandIcon(index, x + 50, y + 40, kText);
  }
}

void DeviceUi::drawCommandIcon(std::uint8_t index, std::int16_t x, std::int16_t y,
                               std::uint16_t color) {
  if (index == 0) {
    display_.fillTriangle(x + 3, y - 18, x - 10, y + 2, x, y + 2, color);
    display_.fillTriangle(x - 3, y + 18, x + 10, y - 2, x, y - 2, color);
  } else if (index == 1) {
    display_.drawLine(x - 14, y, x - 3, y + 11, color);
    display_.drawLine(x - 3, y + 11, x + 16, y - 13, color);
  } else if (index == 2) {
    display_.drawLine(x - 13, y - 13, x + 13, y + 13, color);
    display_.drawLine(x + 13, y - 13, x - 13, y + 13, color);
  } else if (index == 3) {
    display_.drawLine(x - 14, y, x - 3, y, color);
    display_.drawLine(x - 3, y, x + 9, y - 12, color);
    display_.drawLine(x - 3, y, x + 9, y + 12, color);
    drawArrow(x + 12, y - 12, 1, 0, color);
    drawArrow(x + 12, y + 12, 1, 0, color);
  } else if (index == 4) {
    display_.drawRoundRect(x - 7, y - 16, 14, 24, 7, color);
    display_.drawArc(x, y, 16, 12, 0, 180, color, kPanel);
    display_.drawFastVLine(x, y + 12, 8, color);
    display_.drawFastHLine(x - 8, y + 20, 16, color);
  } else {
    display_.fillTriangle(x - 15, y - 13, x + 16, y, x - 15, y + 13, color);
  }
}

void DeviceUi::drawArrow(std::int16_t x, std::int16_t y, std::int8_t dx,
                         std::int8_t dy, std::uint16_t color) {
  const std::int16_t endX = x + dx * 14;
  const std::int16_t endY = y + dy * 14;
  display_.drawLine(x - dx * 7, y - dy * 7, endX, endY, color);
  display_.drawLine(endX, endY, endX - dx * 7 + dy * 6,
                    endY - dy * 7 - dx * 6, color);
  display_.drawLine(endX, endY, endX - dx * 7 - dy * 6,
                    endY - dy * 7 + dx * 6, color);
}

void DeviceUi::drawNavigate() {
  drawButton(18, 38, 70, 48, actionIsPressed(InputKind::Joystick) &&
                                  pressedAction_.angle == 0.75F,
             kAccent);
  drawArrow(53, 62, 0, -1, kText);
  drawButton(18, 150, 70, 48, actionIsPressed(InputKind::Joystick) &&
                                   pressedAction_.angle == 0.25F,
             kAccent);
  drawArrow(53, 174, 0, 1, kText);
  drawButton(2, 94, 70, 48, actionIsPressed(InputKind::Joystick) &&
                                 pressedAction_.angle == 0.50F,
             kAccent);
  drawArrow(37, 118, -1, 0, kText);
  drawButton(76, 94, 70, 48, actionIsPressed(InputKind::Joystick) &&
                                  pressedAction_.angle == 0.00F,
             kAccent);
  drawArrow(111, 118, 1, 0, kText);

  drawButton(166, 42, 68, 64, actionIsPressed(InputKind::EncoderStep, 0), 0xFFE0);
  drawArrow(200, 74, -1, 0, kText);
  drawButton(244, 42, 68, 64, actionIsPressed(InputKind::EncoderStep, 1), 0xFFE0);
  drawArrow(278, 74, 1, 0, kText);
  drawButton(166, 118, 146, 78, actionIsPressed(InputKind::EncoderPress), 0xFFE0);
  display_.drawCircle(239, 157, 25, kText);
  display_.drawCircle(239, 157, 16, kMuted);
  display_.fillCircle(239, 157, 5, kText);
}
