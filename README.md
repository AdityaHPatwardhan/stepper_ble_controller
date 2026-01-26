# Stepper BLE Controller

A complete BLE-based stepper motor control system with an ESP32 firmware and Flutter mobile app.

## Overview

This project allows you to control a stepper motor via Bluetooth Low Energy (BLE) from a mobile app. The system includes:

- **ESP32 Firmware**: Runs on ESP32-C3 or ESP32-S3, handles motor control logic and BLE communication
- **Flutter App**: Cross-platform mobile app (iOS/Android) for remote control and monitoring

## Features

- Real-time status monitoring (pedal, counter, motor state)
- Manual motor control from the app
- Configurable pulse frequency (100-1000 Hz)
- Automatic mode with pedal and counter logic
- 50ms status update interval for responsive UI

## Hardware Setup

### Pin Configuration

| Function | GPIO | Description |
|----------|------|-------------|
| Pulse Output | GPIO 4 | PWM signal to stepper driver (500 Hz default) |
| Pedal Input | GPIO 5 | HIGH when pedal pressed, LOW when released |
| Counter Input | GPIO 6 | HIGH when count complete, LOW otherwise |

### Wiring Diagram

```
                    ESP32-C3/S3
                   +------------+
                   |            |
    Stepper Driver |  GPIO 4  <-------- PWM Pulse Output (to STEP pin)
                   |            |
         5V/Pedal  |  GPIO 5  <-------- Pedal Switch (with pull-down)
                   |            |
       5V/Counter  |  GPIO 6  <-------- Counter Signal (with pull-down)
                   |            |
               GND |  GND    ---------> Common Ground
                   |            |
                   +------------+

    Pedal Switch Wiring:

        5V ----+
               |
             [ Switch ]
               |
        GPIO 5 +---- 10K ---- GND
                   (pull-down)

    Counter Wiring:

        Counter Output (5V when complete) ---- GPIO 6 ---- 10K ---- GND
                                                          (pull-down)
```

### Control Logic

```
if (counter_pin == HIGH):
    motor = OFF           # Count complete, stop regardless of pedal
elif (pedal_pin == HIGH):
    motor = PWM_ON        # Pedal pressed, run motor
else:
    motor = OFF           # Pedal released, stop motor
```

## ESP32 Firmware

### Prerequisites

- ESP-IDF v5.0 or later (tested with v6.0)
- ESP32-C3 or ESP32-S3 development board

### Build Instructions

```bash
# Navigate to firmware directory
cd esp32_firmware

# Set target (choose one)
idf.py set-target esp32c3
# OR
idf.py set-target esp32s3

# Build
idf.py build

# Flash and monitor
idf.py -p /dev/ttyUSB0 flash monitor
```

### Configuration

Use `idf.py menuconfig` to customize:

- `Stepper BLE Controller Configuration`
  - GPIO pin assignments
  - Default frequency
  - BLE device name
  - Status update interval

### BLE Service Specification

| Item | Value |
|------|-------|
| Service UUID | `12345678-1234-5678-1234-56789abcdef0` |
| Status Char UUID | `12345678-1234-5678-1234-56789abcdef1` |
| Command Char UUID | `12345678-1234-5678-1234-56789abcdef2` |
| Config Char UUID | `12345678-1234-5678-1234-56789abcdef3` |

#### Status Characteristic (Read, Notify)

1-byte status packed as bits:
- Bit 0: Pedal pressed (1 = pressed)
- Bit 1: Counter complete (1 = complete)
- Bit 2: Motor running (1 = running)

#### Command Characteristic (Write)

1-byte commands:
- `0x00`: Stop motor (manual off)
- `0x01`: Start motor (manual on)
- `0x02`: Auto mode (pedal control)

#### Config Characteristic (Read, Write)

2-byte little-endian frequency value (100-1000 Hz)

## Flutter App

### Prerequisites

- Flutter SDK 3.0 or later
- Android Studio or Xcode for device deployment

### Build Instructions

```bash
# Navigate to app directory
cd flutter_app

# Get dependencies
flutter pub get

# Run on connected device
flutter run

# Build release APK (Android)
flutter build apk --release

# Build release IPA (iOS)
flutter build ios --release
```

### App Features

1. **Scan Screen**
   - Scans for nearby BLE devices
   - Shows devices with "StepperCtrl" service
   - Tap to connect

2. **Control Screen**
   - Live status indicators (Pedal, Counter, Motor)
   - Press-and-hold motor control button
   - Frequency slider (100-1000 Hz)
   - Auto mode / Force stop buttons

### Required Permissions

**Android** (in AndroidManifest.xml):
- `BLUETOOTH`
- `BLUETOOTH_ADMIN`
- `BLUETOOTH_SCAN`
- `BLUETOOTH_CONNECT`
- `ACCESS_FINE_LOCATION`

**iOS** (in Info.plist):
- `NSBluetoothAlwaysUsageDescription`
- `NSBluetoothPeripheralUsageDescription`
- `NSLocationWhenInUseUsageDescription`

## Troubleshooting

### ESP32 Issues

| Problem | Solution |
|---------|----------|
| BLE not advertising | Check `idf.py monitor` for errors, ensure NimBLE is enabled |
| Motor not running | Verify GPIO connections, check PWM output with oscilloscope |
| Counter not detecting | Check pull-down resistor, verify 5V signal level |

### App Issues

| Problem | Solution |
|---------|----------|
| Device not found | Ensure ESP32 is powered, check Bluetooth permissions |
| Connection fails | Make sure only one device is connecting, try resetting ESP32 |
| Status not updating | Check BLE notifications are enabled, verify UUID match |

### Testing with nRF Connect

1. Install nRF Connect app on your phone
2. Scan and connect to "StepperCtrl"
3. Find service `12345678-1234-5678-1234-56789abcdef0`
4. Enable notifications on status characteristic
5. Write commands to test motor control

## Project Structure

```
stepper_ble_controller/
├── README.md
├── esp32_firmware/
│   ├── CMakeLists.txt
│   ├── sdkconfig.defaults
│   ├── sdkconfig.defaults.esp32c3
│   ├── sdkconfig.defaults.esp32s3
│   └── main/
│       ├── CMakeLists.txt
│       ├── Kconfig.projbuild
│       ├── main.c
│       ├── motor_control.c
│       ├── motor_control.h
│       ├── ble_service.c
│       └── ble_service.h
└── flutter_app/
    ├── pubspec.yaml
    ├── analysis_options.yaml
    ├── android/
    │   └── app/src/main/AndroidManifest.xml
    ├── ios/
    │   └── Runner/Info.plist
    └── lib/
        ├── main.dart
        ├── models/
        │   └── device_state.dart
        ├── services/
        │   └── ble_service.dart
        ├── screens/
        │   ├── scan_screen.dart
        │   └── control_screen.dart
        └── widgets/
            ├── status_indicator.dart
            └── motor_button.dart
```

## License

This project is provided as-is for educational purposes.
