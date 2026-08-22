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

struct InputAction {
  InputKind kind = InputKind::None;
  Page page = Page::Agents;
  std::int8_t index = -1;
  float angle = 0.0F;

  InputAction() = default;
  InputAction(InputKind kindValue, Page pageValue, std::int8_t indexValue, float angleValue)
      : kind(kindValue), page(pageValue), index(indexValue), angle(angleValue) {}
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
const char* protocolKeyFor(const InputAction& action);
StatusKind statusKindForColor(std::uint32_t rgb, float brightness);
bool isNotificationStatus(StatusKind status);
