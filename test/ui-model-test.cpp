#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "ui-model.h"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}

void testAgentGrid() {
  for (int index = 0; index < 6; ++index) {
    const int column = index % 3;
    const int row = index / 3;
    const InputAction action = actionAt(Page::Agents, static_cast<std::int16_t>(54 + column * 106),
                                        static_cast<std::int16_t>(72 + row * 87));
    require(action.kind == InputKind::AgentKey, "Agent領域がAgent Keyにならない");
    require(action.index == index, "Agent番号が一致しない");
  }
}

void testTabs() {
  require(actionAt(Page::Navigate, 53, 224).page == Page::Agents, "Agentタブ");
  require(actionAt(Page::Agents, 160, 224).page == Page::Commands, "Commandタブ");
  require(actionAt(Page::Agents, 270, 224).page == Page::Navigate, "Navigateタブ");
}

void testRotationControlAndCoordinates() {
  const ScreenPoint normal = orientPoint({23, 41}, false);
  require(normal.x == 23 && normal.y == 41, "通常向きの座標維持");
  const ScreenPoint inverted = orientPoint({23, 41}, true);
  require(inverted.x == 296 && inverted.y == 198, "180度回転座標");
  const ScreenPoint restored = orientPoint(inverted, true);
  require(restored.x == 23 && restored.y == 41, "180度回転の可逆性");

  require(bootGestureForDuration(49) == BootGesture::None, "BOOTノイズ除外");
  require(bootGestureForDuration(50) == BootGesture::RotateScreen, "BOOT短押し");
  require(bootGestureForDuration(1499) == BootGesture::RotateScreen,
          "BOOT短押し上限");
  require(bootGestureForDuration(1500) == BootGesture::CalibrateTouch,
          "BOOT長押し調整");
}

void testTouchContactIsLockedUntilPhysicalRelease() {
  require(touchTransition(false, true, true) == TouchTransition::Press,
          "接触開始で一度だけ押下");
  require(touchTransition(true, true, true) == TouchTransition::None,
          "同じ接触の座標更新を無視");
  require(touchTransition(true, false, true) == TouchTransition::None,
          "圧力揺れを解放扱いしない");
  require(touchTransition(true, false, false) == TouchTransition::Release,
          "IRQ解放でのみリリース");
  require(touchTransition(false, false, false) == TouchTransition::None,
          "非接触を維持");
}

void testCommandGridAndProtocolIds() {
  const char* expected[] = {"ACT06", "ACT07", "ACT08", "ACT09", "ACT10", "ACT12"};
  for (int index = 0; index < 6; ++index) {
    const int column = index % 3;
    const int row = index / 3;
    const InputAction action =
        actionAt(Page::Commands, static_cast<std::int16_t>(54 + column * 106),
                 static_cast<std::int16_t>(72 + row * 87));
    require(action.kind == InputKind::CommandKey, "Command領域がCommand Keyにならない");
    require(action.index == index, "Command番号が一致しない");
    require(std::strcmp(protocolKeyFor(action), expected[index]) == 0,
            "CommandのプロトコルIDが一致しない");
  }
}

void testNavigationAngles() {
  require(std::abs(actionAt(Page::Navigate, 53, 62).angle - 0.75F) < 0.001F, "上方向");
  require(std::abs(actionAt(Page::Navigate, 53, 174).angle - 0.25F) < 0.001F, "下方向");
  require(std::abs(actionAt(Page::Navigate, 37, 118).angle - 0.50F) < 0.001F, "左方向");
  require(std::abs(actionAt(Page::Navigate, 111, 118).angle) < 0.001F, "右方向");
  const InputAction counterclockwise = actionAt(Page::Navigate, 200, 74);
  const InputAction clockwise = actionAt(Page::Navigate, 278, 74);
  const InputAction press = actionAt(Page::Navigate, 239, 157);
  require(counterclockwise.kind == InputKind::EncoderStep &&
              std::strcmp(protocolKeyFor(counterclockwise), "ENC_CC") == 0,
          "ダイヤル左回転");
  require(clockwise.kind == InputKind::EncoderStep &&
              std::strcmp(protocolKeyFor(clockwise), "ENC_CW") == 0,
          "ダイヤル右回転");
  require(press.kind == InputKind::EncoderPress &&
              std::strcmp(protocolKeyFor(press), "ENC_CLK") == 0,
          "ダイヤル押下");
}

void testGapsAndBoundsAreInactive() {
  require(actionAt(Page::Agents, -1, 100).kind == InputKind::None, "画面左外");
  require(actionAt(Page::Agents, 320, 100).kind == InputKind::None, "画面右外");
  require(actionAt(Page::Agents, 104, 80).kind == InputKind::None, "タイル間の隙間");
  require(actionAt(Page::Navigate, 155, 100).kind == InputKind::None, "操作外領域");
}

void testStatusColors() {
  require(statusKindForColor(0xFFFFFF, 0.0F) == StatusKind::Unassigned, "未割り当て");
  require(statusKindForColor(0xFFFFFF, 1.0F) == StatusKind::Idle, "待機");
  require(statusKindForColor(0x3291FF, 1.0F) == StatusKind::Thinking, "処理中");
  require(statusKindForColor(0x34C759, 1.0F) == StatusKind::Complete, "完了");
  require(statusKindForColor(0xFFB020, 1.0F) == StatusKind::Attention, "入力待ち");
  require(statusKindForColor(0xFF453A, 1.0F) == StatusKind::Error, "エラー");
  require(!isNotificationStatus(StatusKind::Thinking), "処理中を通知しない");
  require(isNotificationStatus(StatusKind::Complete), "完了を通知する");
  require(isNotificationStatus(StatusKind::Attention), "入力待ちを通知する");
  require(isNotificationStatus(StatusKind::Error), "エラーを通知する");
}

}  // namespace

int main() {
  testAgentGrid();
  testTabs();
  testRotationControlAndCoordinates();
  testTouchContactIsLockedUntilPhysicalRelease();
  testCommandGridAndProtocolIds();
  testNavigationAngles();
  testGapsAndBoundsAreInactive();
  testStatusColors();
  std::cout << "ui-model: 8 tests passed\n";
  return 0;
}
