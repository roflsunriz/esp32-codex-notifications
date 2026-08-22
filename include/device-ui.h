#pragma once

#include <SPI.h>
#include <TFT_eSPI.h>
#include <sensitive-xpt2046.h>

#include <array>
#include <cstdint>

#include "codex-micro-ble.h"
#include "ui-model.h"

class DeviceUi {
 public:
  DeviceUi();

  void begin();
  void calibrateTouch();
  bool readTouch(std::int16_t& x, std::int16_t& y);
  bool touchContactActive() const { return touch_.tirqTouched(); }
  void setState(const CodexMicroState& state, std::uint32_t now);
  void setPage(Page page);
  void toggleRotation();
  Page page() const { return page_; }
  void showPressed(const InputAction& action, bool pressed);
  void tick(std::uint32_t now);

 private:
  struct TouchCalibration {
    std::int16_t left;
    std::int16_t right;
    std::int16_t top;
    std::int16_t bottom;
  };

  void loadCalibration();
  void saveCalibration();
  void loadOrientation();
  void saveOrientation();
  void applyOrientation();
  bool captureCalibrationPoint(std::int16_t& rawX, std::int16_t& rawY);
  void drawAll();
  void drawHeader();
  void drawTabs();
  void drawAgents();
  void drawAgent(std::uint8_t index);
  void drawCommands();
  void drawNavigate();
  void drawButton(std::int16_t x, std::int16_t y, std::int16_t width,
                  std::int16_t height, bool pressed, std::uint16_t border);
  void drawStatusIcon(StatusKind status, std::int16_t x, std::int16_t y,
                      std::uint16_t color);
  void drawCommandIcon(std::uint8_t index, std::int16_t x, std::int16_t y,
                       std::uint16_t color);
  void drawArrow(std::int16_t x, std::int16_t y, std::int8_t dx, std::int8_t dy,
                 std::uint16_t color);
  std::uint16_t lightColor(const ThreadLight& light, float pulse = 1.0F);
  bool actionIsPressed(InputKind kind, std::int8_t index = -1) const;

  TFT_eSPI display_;
  SPIClass touchBus_;
  SensitiveXpt2046 touch_;
  TouchCalibration calibration_;
  CodexMicroState state_;
  std::array<StatusKind, 6> statuses_{};
  Page page_ = Page::Agents;
  InputAction pressedAction_;
  TouchSampleFilter touchFilter_;
  bool pressed_ = false;
  bool inverted_ = false;
  std::int8_t notificationAgent_ = -1;
  StatusKind notificationStatus_ = StatusKind::Unassigned;
  std::uint32_t notificationUntil_ = 0;
  std::uint32_t lastAnimation_ = 0;
};
