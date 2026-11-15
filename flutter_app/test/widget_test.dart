import 'package:flutter_test/flutter_test.dart';
import 'package:stepper_ble_app/models/device_state.dart';

void main() {
  test('status byte is unpacked into pedal, counter and motor flags', () {
    final state = DeviceState();

    state.updateStatus(0x05); // bit 0 pedal + bit 2 motor
    expect(state.pedalPressed, isTrue);
    expect(state.counterComplete, isFalse);
    expect(state.motorRunning, isTrue);

    state.updateStatus(0x02); // bit 1 counter
    expect(state.pedalPressed, isFalse);
    expect(state.counterComplete, isTrue);
    expect(state.motorRunning, isFalse);
  });

  test('disconnecting clears the live status', () {
    final state = DeviceState();
    state.setConnected(true, deviceName: 'StepperCtrl');
    state.updateStatus(0x07);
    state.setMode(MotorMode.manualOn);

    state.setConnected(false);
    expect(state.isConnected, isFalse);
    expect(state.motorRunning, isFalse);
    expect(state.mode, MotorMode.auto);
  });
}
