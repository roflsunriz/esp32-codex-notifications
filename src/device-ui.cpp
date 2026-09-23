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
constexpr std::uint32_t kCalibrationVersion = 2;
struct StoredCalibration {
  std::uint32_t version;
  std::int16_t values[5];
  std::uint16_t check;
};
static_assert(sizeof(StoredCalibration) == 16, "touch calibration record size");
std::uint16_t calibrationCheck(const StoredCalibration& record) {
  std::uint16_t result = 0xA53C;
  for (const auto value : record.values) result ^= static_cast<std::uint16_t>(value);
  return result;
}
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
                   board::kDefaultTouchTop, board::kDefaultTouchBottom,
                   board::kTouchPressureMinimum} {}

void DeviceUi::begin() {
  loadOrientation();
  display_.init();
  backlightPwmReady_ =
      ledcSetup(board::kBacklightPwmChannel, board::kBacklightPwmFrequency,
                board::kBacklightPwmResolution) != 0;
  if (backlightPwmReady_) {
    ledcAttachPin(board::kBacklightPin, board::kBacklightPwmChannel);
  } else {
    pinMode(board::kBacklightPin, OUTPUT);
    Serial.println("UI backlight PWM unavailable; using on/off fallback");
  }
  setBacklight(1.0F, true);
  applyOrientation();
  display_.setTextWrap(false);

  touchBus_.begin(board::kTouchClockPin, board::kTouchMisoPin, board::kTouchMosiPin,
                   board::kTouchChipSelectPin);
  touch_.begin(touchBus_);
  touch_.setRotation(1);
  loadCalibration();
  loadSleep();
  for (auto& status : statuses_) status = StatusKind::Unassigned;
  drawAll();
}

void DeviceUi::loadCalibration() {
  Preferences preferences;
  // 初回起動ではnamespaceがまだ無い。read-writeで開いて空namespaceを正常に作る。
  if (!preferences.begin("codex-touch", false)) return;
  StoredCalibration stored = {};
  if (preferences.getBytesLength("calib") == sizeof(stored) &&
      preferences.getBytes("calib", &stored, sizeof(stored)) == sizeof(stored) &&
      stored.version == kCalibrationVersion &&
      stored.check == calibrationCheck(stored)) {
    TouchCalibration candidate{stored.values[0], stored.values[1],
                               stored.values[2], stored.values[3], stored.values[4]};
    if (std::abs(candidate.right - candidate.left) > 1000 &&
        std::abs(candidate.bottom - candidate.top) > 1000 &&
        candidate.pressure >= 12 && candidate.pressure <= board::kTouchPressureMinimum)
      calibration_ = candidate;
  } else if (preferences.getUInt("version", 0) == 1) {
    TouchCalibration candidate{
        preferences.getShort("left", calibration_.left),
        preferences.getShort("right", calibration_.right),
        preferences.getShort("top", calibration_.top),
        preferences.getShort("bottom", calibration_.bottom),
        board::kTouchPressureMinimum,
    };
    if (std::abs(candidate.right - candidate.left) > 1000 &&
        std::abs(candidate.bottom - candidate.top) > 1000 &&
        candidate.pressure >= 12 && candidate.pressure <= board::kTouchPressureMinimum) {
      calibration_ = candidate;
    }
  }
  preferences.end();
  touch_.setPressureThreshold(calibration_.pressure);
}

bool DeviceUi::saveCalibration() {
  Preferences preferences;
  if (!preferences.begin("codex-touch", false)) return false;
  StoredCalibration stored{kCalibrationVersion,
      {calibration_.left, calibration_.right, calibration_.top,
       calibration_.bottom, calibration_.pressure}, 0};
  stored.check = calibrationCheck(stored);
  const bool saved = preferences.putBytes("calib", &stored, sizeof(stored)) ==
                     sizeof(stored);
  preferences.end();
  return saved;
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

void DeviceUi::loadSleep() {
  sleepTimeoutSec_ = 0U;
  navigateScroll_ = 0;
  Preferences preferences;
  if (!preferences.begin("codex-ui", true)) return;
  const std::uint32_t saved =
      static_cast<std::uint32_t>(preferences.getUInt("sleep_sec", 0U));
  if (sleep_menu::isValidTimeout(saved)) sleepTimeoutSec_ = saved;
  preferences.end();
}

bool DeviceUi::saveSleep() {
  Preferences preferences;
  if (!preferences.begin("codex-ui", false)) return false;
  preferences.putUInt("sleep_sec", sleepTimeoutSec_);
  preferences.end();
  return true;
}

void DeviceUi::setSleepTimeoutSec(std::uint32_t timeoutSec) {
  if (!sleep_menu::isValidTimeout(timeoutSec)) return;
  if (sleepTimeoutSec_ == timeoutSec) return;
  const std::uint32_t previous = sleepTimeoutSec_;
  sleepTimeoutSec_ = timeoutSec;
  if (!saveSleep()) sleepTimeoutSec_ = previous;
  if (displayAwake_ && page_ == Page::Navigate) drawAll();
}

void DeviceUi::setNavigateScroll(std::int16_t scroll) {
  const std::int16_t clamped = sleep_menu::clampScroll(scroll);
  if (navigateScroll_ == clamped) return;
  navigateScroll_ = clamped;
  if (displayAwake_ && page_ == Page::Navigate) drawAll();
}

void DeviceUi::pageNavigateScroll(int dir) {
  if (dir == 0) return;
  setNavigateScroll(
      static_cast<std::int16_t>(navigateScroll_ + dir * 40));
}

void DeviceUi::applyOrientation() {
  display_.setRotation(inverted_ ? 3 : 1);
}

bool DeviceUi::captureCalibrationPoint(std::int16_t& rawX, std::int16_t& rawY,
                                       std::int16_t& pressure) {
  const std::uint32_t timeout = millis() + 15000;
  while (static_cast<std::int32_t>(timeout - millis()) > 0) {
    if (touch_.tirqTouched()) {
      std::int32_t sumX = 0;
      std::int32_t sumY = 0;
      std::int16_t samples = 0;
      std::int16_t weakest = 32767;
      while (touch_.tirqTouched() && samples < 12) {
        const SensitiveTouchPoint point = touch_.getPoint();
        if (point.z >= 12) {
          sumX += point.x;
          sumY += point.y;
          weakest = std::min(weakest, point.z);
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
        pressure = weakest;
        return true;
      }
    }
    delay(10);
  }
  return false;
}

void DeviceUi::calibrateTouch() {
  setDisplayAwake(true);
  touch_.setPressureThreshold(12);
  std::int16_t left = 0;
  std::int16_t top = 0;
  std::int16_t right = 0;
  std::int16_t bottom = 0;
  std::int16_t firstPressure = 0;
  std::int16_t secondPressure = 0;

  // Calibration値は常にrotation 1の物理座標として保存する。
  display_.setRotation(1);
  const auto finish = [this]() {
    touch_.setPressureThreshold(calibration_.pressure);
    applyOrientation();
    drawAll();
  };

  display_.fillScreen(kBackground);
  display_.setTextColor(kText, kBackground);
  display_.setTextDatum(MC_DATUM);
  display_.drawString("TOUCH 1/2", 160, 120, 2);
  display_.drawLine(14, 24, 34, 24, kAccent);
  display_.drawLine(24, 14, 24, 34, kAccent);
  if (!captureCalibrationPoint(left, top, firstPressure)) {
    display_.fillScreen(kBackground);
    display_.drawString("NO TOUCH", 160, 120, 2);
    delay(1800);
    finish();
    return;
  }

  display_.fillScreen(kBackground);
  display_.drawString("TOUCH 2/2", 160, 120, 2);
  display_.drawLine(285, 215, 305, 215, kAccent);
  display_.drawLine(295, 205, 295, 225, kAccent);
  if (!captureCalibrationPoint(right, bottom, secondPressure)) {
    display_.fillScreen(kBackground);
    display_.drawString("NO TOUCH", 160, 120, 2);
    delay(1800);
    finish();
    return;
  }

  if (std::abs(right - left) > 1000 && std::abs(bottom - top) > 1000) {
    const TouchCalibration previous = calibration_;
    calibration_ = {left, right, top, bottom,
                    touchThresholdForPressure(std::min(firstPressure, secondPressure))};
    if (!saveCalibration()) {
      calibration_ = previous;
      display_.fillScreen(kBackground);
      display_.drawString("SAVE FAILED", 160, 120, 2);
      delay(1800);
    }
  } else {
    display_.fillScreen(kBackground);
    display_.drawString("INVALID TOUCH", 160, 120, 2);
    delay(1800);
  }
  finish();
}

bool DeviceUi::readTouch(std::int16_t& x, std::int16_t& y) {
  if (!touch_.tirqTouched()) {
    touchFilter_.reset();
    return false;
  }
  const SensitiveTouchPoint point = touch_.getPoint();
  if (point.z < calibration_.pressure) return false;
  const ScreenPoint oriented = orientPoint(
      {mapAxis(point.x, calibration_.left, calibration_.right, 24, 295,
               board::kScreenWidth - 1),
       mapAxis(point.y, calibration_.top, calibration_.bottom, 24, 215,
               board::kScreenHeight - 1)},
      inverted_);
  ScreenPoint stabilized;
  if (!touchFilter_.push(oriented, stabilized)) return false;
  x = stabilized.x;
  y = stabilized.y;
  return true;
}

bool DeviceUi::readDragPoint(std::int16_t& x, std::int16_t& y) {
  // Tap path uses the smoothed filter output; drags must follow the same
  // source so lightening pressure does not freeze or flap the contact.
  if (!touch_.tirqTouched()) {
    touchFilter_.reset();
    return false;
  }
  const SensitiveTouchPoint point = touch_.getPoint();
  if (point.z >= calibration_.pressure) {
    const ScreenPoint oriented = orientPoint(
        {mapAxis(point.x, calibration_.left, calibration_.right, 24, 295,
                 board::kScreenWidth - 1),
         mapAxis(point.y, calibration_.top, calibration_.bottom, 24, 215,
                 board::kScreenHeight - 1)},
        inverted_);
    ScreenPoint stabilized;
    if (touchFilter_.push(oriented, stabilized)) {
      x = stabilized.x;
      y = stabilized.y;
      return true;
    }
  }
  ScreenPoint stable;
  if (!touchFilter_.current(stable)) return false;
  x = stable.x;
  y = stable.y;
  return true;
}

void DeviceUi::setDisplayAwake(bool awake) {
  if (displayAwake_ == awake) {
    setBacklight(state_.lightingBrightness, awake);
    return;
  }
  displayAwake_ = awake;
  pressed_ = false;
  touchFilter_.reset();
  if (!awake) {
    setBacklight(state_.lightingBrightness, false);
    display_.writecommand(TFT_DISPOFF);
    display_.writecommand(ILI9341_SLPIN);
    Serial.println("UI display=off");
    return;
  }

  display_.writecommand(ILI9341_SLPOUT);
  delay(120);
  display_.writecommand(TFT_DISPON);
  setBacklight(state_.lightingBrightness, true);
  drawAll();
  Serial.println("UI display=on");
}

void DeviceUi::setBacklight(float brightness, bool awake) {
  const std::uint8_t duty = backlightDuty(brightness, awake);
  if (backlightDuty_ == duty) return;
  backlightDuty_ = duty;
  if (backlightPwmReady_) {
    ledcWrite(board::kBacklightPwmChannel, duty);
  } else {
    digitalWrite(board::kBacklightPin, duty == 0 ? LOW : HIGH);
  }
  Serial.printf("UI backlight duty=%u\n", static_cast<unsigned>(duty));
}

void DeviceUi::setState(const CodexMicroState& state, std::uint32_t now) {
  const bool connectionChanged = state_.connected != state.connected;
  const std::int8_t previousNotificationAgent = notificationAgent_;
  const StatusKind previousNotificationStatus = notificationStatus_;
  std::array<bool, 6> agentChanged{};
  for (std::size_t index = 0; index < state.threads.size(); ++index) {
    const StatusKind next =
        statusKindForColor(state.threads[index].color, state.threads[index].brightness);
    const ThreadLight& previous = state_.threads[index];
    const ThreadLight& current = state.threads[index];
    agentChanged[index] = statuses_[index] != next || previous.color != current.color ||
                          previous.brightness != current.brightness ||
                          previous.effect != current.effect;
    if (next != statuses_[index] && isNotificationStatus(next)) {
      notificationAgent_ = static_cast<std::int8_t>(index);
      notificationStatus_ = next;
      notificationUntil_ = now + kNotificationDurationMs;
    }
    statuses_[index] = next;
  }
  state_ = state;
  const bool powerChanged = displayAwake_ != state.displayAwake;
  setDisplayAwake(state.displayAwake);
  if (powerChanged || !displayAwake_) return;

  display_.startWrite();
  if (previousNotificationAgent != notificationAgent_ ||
      previousNotificationStatus != notificationStatus_) {
    drawNotification();
  }
  if (connectionChanged) drawConnection();
  if (page_ == Page::Agents) {
    for (std::uint8_t index = 0; index < agentChanged.size(); ++index) {
      if (agentChanged[index]) drawAgent(index);
    }
  }
  display_.endWrite();
}

void DeviceUi::setPage(Page page) {
  if (page_ == page) return;
  const Page previousPage = page_;
  page_ = page;
  pressed_ = false;
  if (!displayAwake_) return;

  // Clear the whole content area (not just the visible band) so scrolled
  // content from the previous page leaves no remnants behind.
  display_.startWrite();
  display_.fillRect(0, 28, 320, 180, kBackground);
  drawContent();
  drawTab(static_cast<std::uint8_t>(previousPage));
  drawTab(static_cast<std::uint8_t>(page_));
  display_.endWrite();
}

void DeviceUi::toggleRotation() {
  inverted_ = !inverted_;
  saveOrientation();
  applyOrientation();
  pressed_ = false;
  if (displayAwake_) drawAll();
  Serial.printf("UI rotation=%s\n", inverted_ ? "inverted" : "normal");
}

void DeviceUi::showPressed(const InputAction& action, bool pressed) {
  if (action.kind == InputKind::None || action.kind == InputKind::PageSwitch) return;
  pressedAction_ = action;
  pressed_ = pressed;
  if (displayAwake_) {
    display_.startWrite();
    drawPressedAction(action);
    display_.endWrite();
  }
  if (!pressed) pressedAction_ = {};
}

void DeviceUi::tick(std::uint32_t now) {
  if (notificationAgent_ >= 0 &&
      static_cast<std::int32_t>(now - notificationUntil_) >= 0) {
    notificationAgent_ = -1;
    if (displayAwake_) {
      display_.startWrite();
      drawNotification();
      display_.endWrite();
    }
  }
  if (displayAwake_ && page_ == Page::Agents &&
      now - lastAnimation_ >= kAnimationIntervalMs) {
    bool transactionStarted = false;
    for (std::uint8_t index = 0; index < state_.threads.size(); ++index) {
      if (state_.threads[index].effect != "breath") continue;
      if (!transactionStarted) {
        display_.startWrite();
        transactionStarted = true;
      }
      drawAgent(index);
    }
    if (transactionStarted) display_.endWrite();
    lastAnimation_ = now;
  }
}

void DeviceUi::drawAll() {
  if (!displayAwake_) return;
  display_.startWrite();
  display_.fillScreen(kBackground);
  drawHeader();
  drawContent();
  drawTabs();
  display_.endWrite();
}

void DeviceUi::drawContent() {
  display_.fillRect(0, 28, 320, 180, kBackground);
  if (page_ == Page::Agents) drawAgents();
  if (page_ == Page::Commands) drawCommands();
  if (page_ == Page::Navigate) drawNavigate();
}

void DeviceUi::drawHeader() {
  display_.fillRect(0, 0, 320, 28, kBackground);
  display_.setTextDatum(ML_DATUM);
  display_.setTextColor(kText, kBackground);
  display_.drawString("CODEX", 8, 14, 2);

  drawNotification();
  drawConnection();
}

void DeviceUi::drawNotification() {
  display_.fillRect(124, 0, 116, 28, kBackground);

  if (notificationAgent_ >= 0) {
    const ThreadLight& light = state_.threads[notificationAgent_];
    const std::uint16_t color = lightColor(light);
    display_.fillRoundRect(128, 3, 106, 22, 5, kPanel);
    display_.setTextDatum(MC_DATUM);
    display_.setTextColor(kText, kPanel);
    char label[8];
    snprintf(label, sizeof(label), "A%d", static_cast<int>(notificationAgent_) + 1);
    display_.drawString(label, 153, 14, 2);
    drawStatusIcon(notificationStatus_, 207, 14, color);
  }
}

void DeviceUi::drawConnection() {
  display_.fillRect(280, 0, 40, 28, kBackground);
  const std::uint16_t connectionColor = state_.connected ? 0x07E0 : 0xF800;
  display_.drawLine(286, 8, 286, 20, connectionColor);
  display_.drawLine(286, 8, 294, 14, connectionColor);
  display_.drawLine(294, 14, 286, 20, connectionColor);
  display_.drawLine(286, 8, 292, 4, connectionColor);
  display_.drawLine(286, 20, 292, 24, connectionColor);
  display_.fillCircle(309, 14, 4, connectionColor);
}

void DeviceUi::drawTabs() {
  for (std::uint8_t index = 0; index < 3; ++index) drawTab(index);
}

void DeviceUi::drawTab(std::uint8_t index) {
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
  for (std::uint8_t index = 0; index < 6; ++index) drawCommand(index);
}

void DeviceUi::drawCommand(std::uint8_t index) {
  const std::int16_t x = 4 + (index % 3) * 106;
  const std::int16_t y = 32 + (index / 3) * 87;
  const bool pressed = actionIsPressed(InputKind::CommandKey, index);
  drawButton(x, y, 100, 80, pressed, index == 1 ? 0x07E0 : kAccent);
  drawCommandIcon(index, x + 50, y + 40, kText);
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
    display_.fillRoundRect(x - 6, y - 17, 13, 25, 6, color);
    display_.fillRect(x - 14, y - 2, 2, 5, color);
    display_.fillRect(x + 13, y - 2, 2, 5, color);
    display_.drawArc(x, y + 2, 14, 13, 270, 90, color, kPanel, false);
    display_.fillRect(x, y + 16, 2, 6, color);
    display_.fillRect(x - 8, y + 21, 17, 2, color);
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
  using namespace sleep_menu;
  const std::int16_t scroll = clampScroll(navigateScroll_);
  auto visible = [scroll](std::int16_t y, std::int16_t h) {
    return y - scroll + h > kVisibleTop && y - scroll < kVisibleBottom;
  };
  if (visible(38, 48)) drawJoystickButton(18, 38, 0.75F, 0, -1);
  if (visible(150, 48)) drawJoystickButton(18, 150, 0.25F, 0, 1);
  if (visible(94, 48)) drawJoystickButton(2, 94, 0.50F, -1, 0);
  if (visible(94, 48)) drawJoystickButton(76, 94, 0.00F, 1, 0);
  if (visible(42, 64)) drawEncoderStepButton(0, 150, -1);
  if (visible(42, 64)) drawEncoderStepButton(1, 214, 1);
  if (visible(118, 78)) drawEncoderPressButton();
  const std::uint32_t minutes = minutesPart(sleepTimeoutSec_);
  const std::uint32_t hours = hoursPart(sleepTimeoutSec_);
  display_.setTextDatum(TL_DATUM);
  auto drawLabel = [&](std::int16_t y, const char* text) {
    if (y - scroll < kVisibleTop || y - scroll > kVisibleBottom - 16) return;
    display_.setTextColor(kText, kBackground);
    display_.drawString(text, 8, y - scroll, 2);
  };
  auto drawTrack = [&](std::int16_t centerY, std::uint32_t value,
                       std::uint32_t minV, std::uint32_t maxV) {
    const std::int16_t y =
        static_cast<std::int16_t>(centerY - scroll);
    if (y < kVisibleTop + 8 || y > kVisibleBottom - 8) return;
    display_.drawRect(kTrackX0, y - 2, kTrackX1 - kTrackX0, 5, kText);
    const std::int16_t thumbX = sliderXFromValue(value, minV, maxV);
    if (thumbX > kTrackX0)
      display_.fillRect(kTrackX0, y - 2, thumbX - kTrackX0, 5, kAccent);
    display_.fillRect(thumbX - 6, y - 6, 12, 13, kText);
    display_.fillRect(thumbX - 4, y - 4, 8, 9, kPanel);
  };
  char line[32];
  if (sleepTimeoutSec_ == 0U) {
    snprintf(line, sizeof(line), "SLEEP ALWAYS ON");
  } else {
    snprintf(line, sizeof(line), "SLEEP %uH %uM", hours, minutes);
  }
  drawLabel(kCombinedY, line);
  snprintf(line, sizeof(line), "MIN 0-59: %u", minutes);
  drawLabel(kMinutesLabelY, line);
  drawTrack(kMinutesY, minutes, 0U, kMinutesMax);
  snprintf(line, sizeof(line), "HRS 0-24: %u", hours);
  drawLabel(kHoursLabelY, line);
  drawTrack(kHoursY, hours, 0U, kHoursMax);
  drawLabel(kNoteY, "0M 0H = ALWAYS ON");
  // Scrollbar stays fixed on the right edge.
  const std::int16_t trackH = kScrollBarY1 - kScrollBarY0;
  const std::int16_t thumbH = static_cast<std::int16_t>(
      static_cast<std::int32_t>(kVisibleBottom - kVisibleTop) * trackH /
      kContentH);
  const std::int16_t travel = trackH - thumbH;
  const std::int16_t thumbY =
      travel <= 0 || kScrollMax <= 0
          ? kScrollBarY0
          : static_cast<std::int16_t>(kScrollBarY0 +
                                      scroll * travel / kScrollMax);
  display_.drawRect(kScrollBarX0, kScrollBarY0, 12, trackH, kPanel);
  display_.fillRect(kScrollBarX0 + 2, thumbY, 8, thumbH, kText);
}

void DeviceUi::drawPressedAction(const InputAction& action) {
  if (action.page != page_) return;
  if (action.kind == InputKind::PageSwitch ||
      action.kind == InputKind::SleepMinutes ||
      action.kind == InputKind::SleepHours ||
      action.kind == InputKind::NavigateScroll) {
    return;
  }
  if (action.kind == InputKind::AgentKey && action.index >= 0 && action.index < 6) {
    drawAgent(static_cast<std::uint8_t>(action.index));
  } else if (action.kind == InputKind::CommandKey && action.index >= 0 &&
             action.index < 6) {
    drawCommand(static_cast<std::uint8_t>(action.index));
  } else if (action.kind == InputKind::Joystick) {
    if (action.angle == 0.75F) drawJoystickButton(18, 38, 0.75F, 0, -1);
    if (action.angle == 0.25F) drawJoystickButton(18, 150, 0.25F, 0, 1);
    if (action.angle == 0.50F) drawJoystickButton(2, 94, 0.50F, -1, 0);
    if (action.angle == 0.00F) drawJoystickButton(76, 94, 0.00F, 1, 0);
  } else if (action.kind == InputKind::EncoderStep && action.index >= 0 &&
             action.index < 2) {
    const std::uint8_t index = static_cast<std::uint8_t>(action.index);
    drawEncoderStepButton(index, index == 0 ? 150 : 214, index == 0 ? -1 : 1);
  } else if (action.kind == InputKind::EncoderPress) {
    drawEncoderPressButton();
  }
}

void DeviceUi::drawJoystickButton(std::int16_t x, std::int16_t y, float angle,
                                   std::int8_t dx, std::int8_t dy) {
  const std::int16_t scrolled =
      static_cast<std::int16_t>(y - sleep_menu::clampScroll(navigateScroll_));
  if (scrolled + 48 <= sleep_menu::kVisibleTop || scrolled >= sleep_menu::kVisibleBottom)
    return;
  drawButton(x, scrolled, 70, 48,
             actionIsPressed(InputKind::Joystick) && pressedAction_.angle == angle,
             kAccent);
  drawArrow(x + 35, scrolled + 24, dx, dy, kText);
}

void DeviceUi::drawEncoderStepButton(std::uint8_t index, std::int16_t x,
                                     std::int8_t dx) {
  const std::int16_t scrolled =
      static_cast<std::int16_t>(42 - sleep_menu::clampScroll(navigateScroll_));
  if (scrolled + 64 <= sleep_menu::kVisibleTop || scrolled >= sleep_menu::kVisibleBottom)
    return;
  drawButton(x, scrolled, 60, 64, actionIsPressed(InputKind::EncoderStep, index), 0xFFE0);
  drawArrow(x + 30, scrolled + 32, dx, 0, kText);
}

void DeviceUi::drawEncoderPressButton() {
  const std::int16_t scrolled =
      static_cast<std::int16_t>(118 - sleep_menu::clampScroll(navigateScroll_));
  if (scrolled + 78 <= sleep_menu::kVisibleTop || scrolled >= sleep_menu::kVisibleBottom)
    return;
  drawButton(166, scrolled, 106, 78, actionIsPressed(InputKind::EncoderPress), 0xFFE0);
  display_.drawCircle(219, scrolled + 39, 25, kText);
  display_.drawCircle(219, scrolled + 39, 16, kMuted);
  display_.fillCircle(219, scrolled + 39, 5, kText);
}
