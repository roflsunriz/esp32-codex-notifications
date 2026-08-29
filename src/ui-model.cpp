#include "ui-model.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {

constexpr std::int16_t kContentBottom = 208;
constexpr std::uint32_t kMinimumBootPressMs = 50;
constexpr std::uint32_t kCalibrationHoldMs = 1500;
constexpr const char* kAgentKeys[] = {"AG00", "AG01", "AG02", "AG03", "AG04", "AG05"};
constexpr const char* kCommandKeys[] = {"ACT06", "ACT07", "ACT08", "ACT09", "ACT10", "ACT12"};

bool contains(std::int16_t x, std::int16_t y, std::int16_t left, std::int16_t top,
              std::int16_t width, std::int16_t height) {
  return x >= left && x < left + width && y >= top && y < top + height;
}

InputAction gridAction(Page page, std::int16_t x, std::int16_t y, InputKind kind) {
  if (x < 4 || y < 32) {
    return {};
  }
  const auto column = static_cast<std::int16_t>((x - 4) / 106);
  const auto row = static_cast<std::int16_t>((y - 32) / 87);
  if (column < 0 || column > 2 || row < 0 || row > 1) {
    return {};
  }
  const auto left = static_cast<std::int16_t>(4 + column * 106);
  const auto top = static_cast<std::int16_t>(32 + row * 87);
  if (!contains(x, y, left, top, 100, 80)) {
    return {};
  }
  return {kind, page, static_cast<std::int8_t>(row * 3 + column), 0.0F};
}

}  // namespace

void DisplayPowerSync::reset(std::uint32_t nowMs) {
  awake_ = true;
  connected_ = false;
  lightingConfigOff_ = false;
  // 接続直後の通常状態が複数回全消灯で届いても画面を即座に消さない。
  ignoreAllOffUntil_ = nowMs + kWakeGraceMs;
  disconnectSleepAt_ = nowMs + kDisconnectSleepMs;
}

bool DisplayPowerSync::observeLightingConfig(bool allOff) {
  lightingConfigOff_ = allOff;
  if (allOff || awake_) return false;
  awake_ = true;
  return true;
}

bool DisplayPowerSync::observeThreadLighting(std::uint8_t updatedMask, bool allOff,
                                             std::uint32_t nowMs) {
  const bool completeAllOff = updatedMask == kAllThreadsMask && allOff;
  if (!completeAllOff) {
    if (awake_) return false;
    awake_ = true;
    return true;
  }
  if (!lightingConfigOff_) return false;
  if (static_cast<std::int32_t>(nowMs - ignoreAllOffUntil_) < 0) {
    return false;
  }
  if (!awake_) return false;
  awake_ = false;
  return true;
}

bool DisplayPowerSync::setConnected(bool connected, std::uint32_t nowMs) {
  connected_ = connected;
  if (!connected) {
    disconnectSleepAt_ = nowMs + kDisconnectSleepMs;
    return false;
  }

  lightingConfigOff_ = false;
  ignoreAllOffUntil_ = nowMs + kWakeGraceMs;
  if (awake_) return false;
  awake_ = true;
  return true;
}

bool DisplayPowerSync::tick(std::uint32_t nowMs) {
  if (connected_ || !awake_ ||
      static_cast<std::int32_t>(nowMs - disconnectSleepAt_) < 0) {
    return false;
  }
  awake_ = false;
  return true;
}

bool DisplayPowerSync::wake(std::uint32_t nowMs) {
  const bool changed = !awake_;
  awake_ = true;
  // Desktopは活動通知後に現在の照明状態を複数回再送する場合がある。全タスク
  // 未割り当てなら全組が消灯状態なので、短い復帰猶予中はAuto-dim扱いしない。
  ignoreAllOffUntil_ = nowMs + kWakeGraceMs;
  if (!connected_) disconnectSleepAt_ = nowMs + kDisconnectSleepMs;
  return changed;
}

InputAction actionAt(Page page, std::int16_t x, std::int16_t y) {
  if (x < 0 || x >= 320 || y < 0 || y >= 240) {
    return {};
  }

  if (y >= kContentBottom) {
    const auto tab = static_cast<std::uint8_t>(std::min<std::int16_t>(2, x / 107));
    return {InputKind::PageSwitch, static_cast<Page>(tab), -1, 0.0F};
  }

  if (page == Page::Agents) {
    return gridAction(page, x, y, InputKind::AgentKey);
  }
  if (page == Page::Commands) {
    return gridAction(page, x, y, InputKind::CommandKey);
  }

  if (contains(x, y, 18, 38, 70, 48)) return {InputKind::Joystick, page, -1, 0.75F};
  if (contains(x, y, 18, 150, 70, 48)) return {InputKind::Joystick, page, -1, 0.25F};
  if (contains(x, y, 2, 94, 70, 48)) return {InputKind::Joystick, page, -1, 0.50F};
  if (contains(x, y, 76, 94, 70, 48)) return {InputKind::Joystick, page, -1, 0.00F};
  if (contains(x, y, 166, 42, 68, 64)) return {InputKind::EncoderStep, page, 0, 0.0F};
  if (contains(x, y, 244, 42, 68, 64)) return {InputKind::EncoderStep, page, 1, 0.0F};
  if (contains(x, y, 166, 118, 146, 78)) return {InputKind::EncoderPress, page, -1, 0.0F};
  return {};
}

ScreenPoint orientPoint(ScreenPoint point, bool inverted) {
  if (!inverted) return point;
  return {static_cast<std::int16_t>(319 - point.x),
          static_cast<std::int16_t>(239 - point.y)};
}

bool TouchSampleFilter::push(ScreenPoint sample, ScreenPoint& stabilized) {
  if (delivered_) return false;
  if (count_ > 0) {
    const auto averageX = static_cast<std::int16_t>(sumX_ / count_);
    const auto averageY = static_cast<std::int16_t>(sumY_ / count_);
    if (std::abs(sample.x - averageX) > kMaximumDelta ||
        std::abs(sample.y - averageY) > kMaximumDelta) {
      reset();
    }
  }

  sumX_ += sample.x;
  sumY_ += sample.y;
  ++count_;
  if (count_ < kRequiredSamples) return false;

  stabilized = {static_cast<std::int16_t>(sumX_ / count_),
                static_cast<std::int16_t>(sumY_ / count_)};
  delivered_ = true;
  return true;
}

void TouchSampleFilter::reset() {
  sumX_ = 0;
  sumY_ = 0;
  count_ = 0;
  delivered_ = false;
}

BootGesture bootGestureForDuration(std::uint32_t durationMs) {
  if (durationMs < kMinimumBootPressMs) return BootGesture::None;
  if (durationMs >= kCalibrationHoldMs) return BootGesture::CalibrateTouch;
  return BootGesture::RotateScreen;
}

TouchTransition touchTransition(bool captured, bool hasPoint, bool contactActive) {
  if (!captured && hasPoint) return TouchTransition::Press;
  if (captured && !contactActive) return TouchTransition::Release;
  return TouchTransition::None;
}

const char* protocolKeyFor(const InputAction& action) {
  if (action.kind == InputKind::AgentKey && action.index >= 0 && action.index < 6) {
    return kAgentKeys[action.index];
  }
  if (action.kind == InputKind::CommandKey && action.index >= 0 && action.index < 6) {
    return kCommandKeys[action.index];
  }
  if (action.kind == InputKind::EncoderStep && action.index == 0) return "ENC_CW";
  if (action.kind == InputKind::EncoderStep && action.index == 1) return "ENC_CC";
  if (action.kind == InputKind::EncoderPress) return "ENC_CLK";
  return nullptr;
}

StatusKind statusKindForColor(std::uint32_t rgb, float brightness) {
  if (brightness <= 0.01F) {
    return StatusKind::Unassigned;
  }

  const auto red = static_cast<int>((rgb >> 16U) & 0xFFU);
  const auto green = static_cast<int>((rgb >> 8U) & 0xFFU);
  const auto blue = static_cast<int>(rgb & 0xFFU);
  const int maximum = std::max(red, std::max(green, blue));
  const int minimum = std::min(red, std::min(green, blue));

  if (maximum - minimum < 45) return StatusKind::Idle;
  if (blue > red + 35 && blue > green + 15) return StatusKind::Thinking;
  if (red > 150 && green > 70 && green < red && blue < green) return StatusKind::Attention;
  if (red > green + 35 && red > blue + 35) return StatusKind::Error;
  if (green > red + 20 && green >= blue - 10) return StatusKind::Complete;
  return StatusKind::Idle;
}

bool isNotificationStatus(StatusKind status) {
  return status == StatusKind::Complete || status == StatusKind::Attention ||
         status == StatusKind::Error;
}

float synchronizedLightingBrightness(float previous, float observed,
                                     bool observable) {
  const float safePrevious = std::isfinite(previous)
                                 ? std::max(0.0F, std::min(1.0F, previous))
                                 : 1.0F;
  if (!observable || !std::isfinite(observed)) return safePrevious;
  return std::max(0.0F, std::min(1.0F, observed));
}

std::uint8_t backlightDuty(float brightness, bool displayAwake) {
  if (!displayAwake || !std::isfinite(brightness)) return 0;
  const float clamped = std::max(0.0F, std::min(1.0F, brightness));
  return static_cast<std::uint8_t>(std::lround(clamped * 255.0F));
}
