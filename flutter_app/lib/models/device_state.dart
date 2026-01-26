import 'package:flutter/foundation.dart';

enum MotorMode { auto, manualOn, manualOff }

class DeviceState extends ChangeNotifier {
  bool _isConnected = false;
  bool _pedalPressed = false;
  bool _counterComplete = false;
  bool _motorRunning = false;
  MotorMode _mode = MotorMode.auto;
  int _frequency = 500;
  String _deviceName = '';

  bool get isConnected => _isConnected;
  bool get pedalPressed => _pedalPressed;
  bool get counterComplete => _counterComplete;
  bool get motorRunning => _motorRunning;
  MotorMode get mode => _mode;
  int get frequency => _frequency;
  String get deviceName => _deviceName;

  void setConnected(bool value, {String deviceName = ''}) {
    _isConnected = value;
    _deviceName = deviceName;
    if (!value) {
      _pedalPressed = false;
      _counterComplete = false;
      _motorRunning = false;
      _mode = MotorMode.auto;
    }
    notifyListeners();
  }

  void updateStatus(int statusByte) {
    _pedalPressed = (statusByte & 0x01) != 0;
    _counterComplete = (statusByte & 0x02) != 0;
    _motorRunning = (statusByte & 0x04) != 0;
    notifyListeners();
  }

  void setMode(MotorMode mode) {
    _mode = mode;
    notifyListeners();
  }

  void setFrequency(int freq) {
    _frequency = freq;
    notifyListeners();
  }

  String get modeString {
    switch (_mode) {
      case MotorMode.auto:
        return 'Auto';
      case MotorMode.manualOn:
        return 'Manual ON';
      case MotorMode.manualOff:
        return 'Manual OFF';
    }
  }
}
