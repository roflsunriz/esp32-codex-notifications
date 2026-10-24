#pragma once

#include <cstdint>

enum class Page : std::uint8_t { Agents = 0, Commands = 1, Navigate = 2 };

enum class InputKind : std::uint8_t {
  None,
  PageSwitch,
  AgentKey,
  CommandKey,
  EncoderStep,
  EncoderPress,
  Joystick,
  SleepMinutes,
  SleepHours,
  ScreenOff,
  NavigateScroll,
};

enum class BootGesture : std::uint8_t { None, RotateScreen, CalibrateTouch };
enum class TouchTransition : std::uint8_t { None, Press, Release };

class DisplayPowerSync {
 public:
  void reset(std::uint32_t nowMs);
  bool observeLightingConfig(bool allOff);
  bool observeThreadLighting(std::uint8_t updatedMask, bool allOff,
                             std::uint32_t nowMs);
  bool setConnected(bool connected, std::uint32_t nowMs);
  bool tick(std::uint32_t nowMs);
  bool wake(std::uint32_t nowMs);
  bool sleepNow();
  bool awake() const { return awake_; }
  // User idle auto-off in seconds. 0 disables it (lighting-driven only).
  void setIdleTimeoutSec(std::uint32_t seconds) {
    idleTimeoutSec_ = seconds <= kIdleTimeoutMaxSec ? seconds : 0U;
  }
  std::uint32_t idleTimeoutSec() const { return idleTimeoutSec_; }
  void noteActivity(std::uint32_t nowMs) { lastActivityMs_ = nowMs; }

 private:
  static constexpr std::uint8_t kAllThreadsMask = 0x3F;
  static constexpr std::uint32_t kWakeGraceMs = 5000;
  static constexpr std::uint32_t kDisconnectSleepMs = 30000;
  static constexpr std::uint32_t kIdleTimeoutMaxSec = 24U * 3600U + 59U * 60U;

  bool awake_ = true;
  bool connected_ = false;
  bool lightingConfigOff_ = false;
  bool manualOff_ = false;
  std::uint32_t ignoreAllOffUntil_ = kWakeGraceMs;
  std::uint32_t disconnectSleepAt_ = kDisconnectSleepMs;
  std::uint32_t idleTimeoutSec_ = 0U;
  std::uint32_t lastActivityMs_ = 0U;
};

struct InputAction {
  InputKind kind = InputKind::None;
  Page page = Page::Agents;
  std::int8_t index = -1;
  float angle = 0.0F;

  InputAction() = default;
  InputAction(InputKind kindValue, Page pageValue, std::int8_t indexValue, float angleValue)
      : kind(kindValue), page(pageValue), index(indexValue), angle(angleValue) {}
};

struct ScreenPoint {
  std::int16_t x = 0;
  std::int16_t y = 0;

  ScreenPoint() = default;
  ScreenPoint(std::int16_t xValue, std::int16_t yValue) : x(xValue), y(yValue) {}
};

class TouchSampleFilter {
 public:
  bool push(ScreenPoint sample, ScreenPoint& stabilized);
  void reset();
  bool current(ScreenPoint& stabilized) const {
    if (!delivered_) return false;
    stabilized = last_;
    return true;
  }

 private:
  static constexpr std::uint8_t kRequiredSamples = 3;
  static constexpr std::int16_t kMaximumDelta = 18;

  std::int32_t sumX_ = 0;
  std::int32_t sumY_ = 0;
  std::uint8_t count_ = 0;
  bool delivered_ = false;
  ScreenPoint last_{};
  std::int32_t emaX_ = 0;
  std::int32_t emaY_ = 0;
};

enum class StatusKind : std::uint8_t {
  Unassigned,
  Idle,
  Thinking,
  Complete,
  Attention,
  Error,
};

InputAction actionAt(Page page, std::int16_t x, std::int16_t y,
                     std::int16_t scroll = 0,
                     std::uint32_t timeoutSec = 0U);
InputAction navigateActionAt(std::int16_t x, std::int16_t y,
                             std::int16_t scroll,
                             std::uint32_t timeoutSec = 0U);
ScreenPoint orientPoint(ScreenPoint point, bool inverted);
BootGesture bootGestureForDuration(std::uint32_t durationMs);
std::int16_t touchThresholdForPressure(std::int16_t weakestPressure);
TouchTransition touchTransition(bool captured, bool hasPoint, bool contactActive);
const char* protocolKeyFor(const InputAction& action);
StatusKind statusKindForColor(std::uint32_t rgb, float brightness);
bool isNotificationStatus(StatusKind status);
float synchronizedLightingBrightness(float previous, float observed,
                                       bool observable);
std::uint8_t backlightDuty(float brightness, bool displayAwake);

// Navigate-tab sleep menu (auto-off without polling). Minutes/hours use
// vertical drums; 0 minutes + 0 hours disables the local timer.
// Touch positions are calibrated to screen 24..295, so controls stay inside.
namespace sleep_menu {
constexpr std::uint32_t kMinutesMax = 59U;
constexpr std::uint32_t kHoursMax = 24U;
constexpr std::uint32_t kTimeoutMaxSec = 24U * 3600U + 59U * 60U;
constexpr std::int16_t kTrackX0 = 24;
constexpr std::int16_t kTrackX1 = 275;
constexpr std::int16_t kWheelRowH = 22;
constexpr std::int16_t kWheelCenterY = 272;
constexpr std::int16_t kWheelTopY = kWheelCenterY - kWheelRowH * 2 - 11;
constexpr std::int16_t kWheelBottomY = kWheelCenterY + kWheelRowH * 2 + 11;
constexpr std::int16_t kHoursWheelX0 = 24;
constexpr std::int16_t kHoursWheelX1 = 146;
constexpr std::int16_t kMinutesWheelX0 = 154;
constexpr std::int16_t kMinutesWheelX1 = 275;
constexpr std::int16_t kScreenOffY = 338;
constexpr std::int16_t kScreenOffH = 30;
constexpr std::int16_t kContentH = 382;
constexpr std::int16_t kVisibleTop = 28;
constexpr std::int16_t kVisibleBottom = 208;
constexpr std::int16_t kScrollMax =
    kContentH - (kVisibleBottom - kVisibleTop);
constexpr std::int16_t kScrollBarX0 = 283;
constexpr std::int16_t kScrollBarY0 = 32;
constexpr std::int16_t kScrollBarY1 = 200;

inline std::uint32_t minutesPart(std::uint32_t timeoutSec) {
  return (timeoutSec % 3600U) / 60U;
}

inline std::uint32_t hoursPart(std::uint32_t timeoutSec) {
  return timeoutSec / 3600U;
}

inline std::uint32_t timeoutFromParts(std::uint32_t minutes,
                                      std::uint32_t hours) {
  if (minutes > kMinutesMax) minutes = kMinutesMax;
  if (hours > kHoursMax) hours = kHoursMax;
  return hours * 3600U + minutes * 60U;
}

inline bool isValidTimeout(std::uint32_t timeoutSec) {
  return timeoutSec <= kTimeoutMaxSec;
}

inline std::uint32_t sliderValueFromX(std::int16_t x, std::uint32_t minV,
                                      std::uint32_t maxV, std::uint32_t step) {
  if (maxV <= minV || step == 0U) return minV;
  std::int32_t clamped = x;
  if (clamped < kTrackX0) clamped = kTrackX0;
  if (clamped > kTrackX1) clamped = kTrackX1;
  const std::uint32_t trackW = static_cast<std::uint32_t>(kTrackX1 - kTrackX0);
  const std::uint32_t offset = static_cast<std::uint32_t>(clamped - kTrackX0);
  const std::uint32_t range = maxV - minV;
  const std::uint32_t steps = range / step;
  std::uint32_t index =
      (offset * steps + trackW / 2U) / trackW;
  if (index > steps) index = steps;
  return minV + index * step;
}

inline std::int16_t sliderXFromValue(std::uint32_t value, std::uint32_t minV,
                                     std::uint32_t maxV) {
  if (maxV <= minV) return kTrackX0;
  if (value < minV) value = minV;
  if (value > maxV) value = maxV;
  const std::uint32_t trackW = static_cast<std::uint32_t>(kTrackX1 - kTrackX0);
  const std::uint32_t range = maxV - minV;
  return static_cast<std::int16_t>(
      kTrackX0 + ((value - minV) * trackW + range / 2U) / range);
}

inline std::int16_t clampScroll(int value) {
  if (value < 0) return 0;
  if (value > kScrollMax) return kScrollMax;
  return static_cast<std::int16_t>(value);
}

inline int wheelSteps(int pixels) {
  return pixels >= 0 ? (pixels + kWheelRowH / 2) / kWheelRowH
                     : -((-pixels + kWheelRowH / 2) / kWheelRowH);
}

inline std::uint32_t clampWheelValue(int value, std::uint32_t maximum) {
  if (value < 0) return 0U;
  if (static_cast<std::uint32_t>(value) > maximum) return maximum;
  return static_cast<std::uint32_t>(value);
}

inline std::uint32_t wheelValueForTap(std::uint32_t current, int touchY,
                                      std::uint32_t maximum) {
  return clampWheelValue(static_cast<int>(current) +
                             wheelSteps(touchY - kWheelCenterY),
                         maximum);
}

inline std::uint32_t wheelValueForDrag(std::uint32_t start, int startY,
                                       int currentY,
                                       std::uint32_t maximum) {
  return clampWheelValue(static_cast<int>(start) +
                             wheelSteps(startY - currentY),
                         maximum);
}

inline std::int16_t scrollFromTrackY(std::int16_t y) {
  const std::int16_t trackH = kScrollBarY1 - kScrollBarY0;
  const std::int16_t thumbH = static_cast<std::int16_t>(
      static_cast<std::int32_t>(kVisibleBottom - kVisibleTop) * trackH /
      kContentH);
  const std::int16_t travel = trackH - thumbH;
  if (travel <= 0 || kScrollMax <= 0) return 0;
  const std::int32_t offset = (static_cast<std::int32_t>(y) - thumbH / 2 -
                               kScrollBarY0) *
                              kScrollMax / travel;
  return clampScroll(offset);
}
}  // namespace sleep_menu
