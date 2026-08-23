from __future__ import annotations

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src" / "device-ui.cpp").read_text(encoding="utf-8")


def function_body(signature: str) -> str:
    start = SOURCE.index(signature)
    opening = SOURCE.index("{", start)
    depth = 0
    for index in range(opening, len(SOURCE)):
        if SOURCE[index] == "{":
            depth += 1
        elif SOURCE[index] == "}":
            depth -= 1
            if depth == 0:
                return SOURCE[opening + 1 : index]
    raise AssertionError(f"function is not closed: {signature}")


class RenderPolicyTests(unittest.TestCase):
    def test_frequent_updates_never_redraw_the_whole_screen(self) -> None:
        for signature in (
            "void DeviceUi::setState",
            "void DeviceUi::setPage",
            "void DeviceUi::showPressed",
            "void DeviceUi::tick",
        ):
            body = function_body(signature)
            self.assertNotIn("drawAll(", body, signature)
            self.assertNotIn("fillScreen(", body, signature)

    def test_press_redraws_only_the_affected_control(self) -> None:
        body = function_body("void DeviceUi::showPressed")
        self.assertIn("drawPressedAction(action)", body)
        self.assertIn("InputKind::PageSwitch", body)

    def test_status_and_animation_updates_are_local(self) -> None:
        state_body = function_body("void DeviceUi::setState")
        self.assertIn("drawNotification()", state_body)
        self.assertIn("drawConnection()", state_body)
        self.assertIn("drawAgent(index)", state_body)

        tick_body = function_body("void DeviceUi::tick")
        self.assertIn("drawAgent(index)", tick_body)
        self.assertNotIn("drawAgents()", tick_body)


if __name__ == "__main__":
    unittest.main()
