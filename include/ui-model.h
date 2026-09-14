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
  bool awake() const { return awake_; }

 private:
  static constexpr std::uint8_t kAllThreadsMask = 0x3F;
  static constexpr std::uint32_t kWakeGraceMs = 5000;
  static constexpr std::uint32_t kDisconnectSleepMs = 30000;

  bool awake_ = true;
  bool connected_ = false;
  bool lightingConfigOff_ = false;
  std::uint32_t ignoreAllOffUntil_ = kWakeGraceMs;
  std::uint32_t disconnectSleepAt_ = kDisconnectSleepMs;
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

 private:
  static constexpr std::uint8_t kRequiredSamples = 3;
  static constexpr std::int16_t kMaximumDelta = 18;

  std::int32_t sumX_ = 0;
  std::int32_t sumY_ = 0;
  std::uint8_t count_ = 0;
  bool delivered_ = false;
};

enum class StatusKind : std::uint8_t {
  Unassigned,
  Idle,
  Thinking,
  Complete,
  Attention,
  Error,
};

InputAction actionAt(Page page, std::int16_t x, std::int16_t y);
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
