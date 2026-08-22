#include "ui-model.h"

#include <algorithm>

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
  if (action.kind == InputKind::EncoderStep && action.index == 0) return "ENC_CC";
  if (action.kind == InputKind::EncoderStep && action.index == 1) return "ENC_CW";
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
