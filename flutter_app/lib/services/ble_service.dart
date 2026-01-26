import 'dart:async';
import 'dart:typed_data';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import '../models/device_state.dart';

class BleService {
  static const String serviceUuid = '12345678-1234-5678-1234-56789abcdef0';
  static const String statusCharUuid = '12345678-1234-5678-1234-56789abcdef1';
  static const String commandCharUuid = '12345678-1234-5678-1234-56789abcdef2';
  static const String configCharUuid = '12345678-1234-5678-1234-56789abcdef3';

  static const int cmdStop = 0x00;
  static const int cmdStart = 0x01;
  static const int cmdAutoMode = 0x02;

  BluetoothDevice? _device;
  BluetoothCharacteristic? _statusChar;
  BluetoothCharacteristic? _commandChar;
  BluetoothCharacteristic? _configChar;
  StreamSubscription? _statusSubscription;
  StreamSubscription? _connectionSubscription;

  final DeviceState deviceState;

  BleService(this.deviceState);

  bool get isConnected => _device != null && deviceState.isConnected;

  Stream<List<ScanResult>> get scanResults => FlutterBluePlus.scanResults;

  Future<void> startScan({Duration timeout = const Duration(seconds: 10)}) async {
    await FlutterBluePlus.startScan(
      timeout: timeout,
      androidUsesFineLocation: true,
    );
  }

  Future<void> stopScan() async {
    await FlutterBluePlus.stopScan();
  }

  Future<bool> connect(BluetoothDevice device) async {
    try {
      _device = device;

      _connectionSubscription = device.connectionState.listen((state) {
        if (state == BluetoothConnectionState.disconnected) {
          deviceState.setConnected(false);
          _cleanup();
        }
      });

      await device.connect(timeout: const Duration(seconds: 10));

      List<BluetoothService> services = await device.discoverServices();

      BluetoothService? stepperService;
      for (var service in services) {
        if (service.uuid.toString().toLowerCase() == serviceUuid.toLowerCase()) {
          stepperService = service;
          break;
        }
      }

      if (stepperService == null) {
        await disconnect();
        return false;
      }

      for (var char in stepperService.characteristics) {
        String uuid = char.uuid.toString().toLowerCase();
        if (uuid == statusCharUuid.toLowerCase()) {
          _statusChar = char;
        } else if (uuid == commandCharUuid.toLowerCase()) {
          _commandChar = char;
        } else if (uuid == configCharUuid.toLowerCase()) {
          _configChar = char;
        }
      }

      if (_statusChar == null || _commandChar == null || _configChar == null) {
        await disconnect();
        return false;
      }

      await _statusChar!.setNotifyValue(true);
      _statusSubscription = _statusChar!.onValueReceived.listen((value) {
        if (value.isNotEmpty) {
          deviceState.updateStatus(value[0]);
        }
      });

      List<int> freqData = await _configChar!.read();
      if (freqData.length >= 2) {
        int freq = freqData[0] | (freqData[1] << 8);
        deviceState.setFrequency(freq);
      }

      deviceState.setConnected(true, deviceName: device.platformName);
      return true;
    } catch (e) {
      await disconnect();
      return false;
    }
  }

  Future<void> disconnect() async {
    await _cleanup();
    if (_device != null) {
      try {
        await _device!.disconnect();
      } catch (_) {}
      _device = null;
    }
    deviceState.setConnected(false);
  }

  Future<void> _cleanup() async {
    await _statusSubscription?.cancel();
    _statusSubscription = null;
    await _connectionSubscription?.cancel();
    _connectionSubscription = null;
    _statusChar = null;
    _commandChar = null;
    _configChar = null;
  }

  Future<bool> sendCommand(int command) async {
    if (_commandChar == null) return false;

    try {
      await _commandChar!.write([command], withoutResponse: false);

      switch (command) {
        case cmdStop:
          deviceState.setMode(MotorMode.manualOff);
          break;
        case cmdStart:
          deviceState.setMode(MotorMode.manualOn);
          break;
        case cmdAutoMode:
          deviceState.setMode(MotorMode.auto);
          break;
      }
      return true;
    } catch (e) {
      return false;
    }
  }

  Future<bool> startMotor() => sendCommand(cmdStart);
  Future<bool> stopMotor() => sendCommand(cmdStop);
  Future<bool> setAutoMode() => sendCommand(cmdAutoMode);

  Future<bool> setFrequency(int frequency) async {
    if (_configChar == null) return false;

    frequency = frequency.clamp(100, 1000);

    try {
      Uint8List data = Uint8List(2);
      data[0] = frequency & 0xFF;
      data[1] = (frequency >> 8) & 0xFF;

      await _configChar!.write(data.toList(), withoutResponse: false);
      deviceState.setFrequency(frequency);
      return true;
    } catch (e) {
      return false;
    }
  }

  void dispose() {
    disconnect();
  }
}
