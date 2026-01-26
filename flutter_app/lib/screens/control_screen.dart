import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../services/ble_service.dart';
import '../models/device_state.dart';
import '../widgets/status_indicator.dart';
import '../widgets/motor_button.dart';
import 'scan_screen.dart';

class ControlScreen extends StatefulWidget {
  const ControlScreen({super.key});

  @override
  State<ControlScreen> createState() => _ControlScreenState();
}

class _ControlScreenState extends State<ControlScreen> {
  double _frequencySlider = 500;

  @override
  void initState() {
    super.initState();
    final deviceState = context.read<DeviceState>();
    _frequencySlider = deviceState.frequency.toDouble();
  }

  void _onFrequencyChanged(double value) {
    setState(() {
      _frequencySlider = value;
    });
  }

  Future<void> _onFrequencyChangeEnd(double value) async {
    final bleService = context.read<BleService>();
    await bleService.setFrequency(value.round());
  }

  Future<void> _onMotorStart() async {
    final bleService = context.read<BleService>();
    await bleService.startMotor();
  }

  Future<void> _onMotorStop() async {
    final bleService = context.read<BleService>();
    await bleService.setAutoMode();
  }

  Future<void> _disconnect() async {
    final bleService = context.read<BleService>();
    await bleService.disconnect();

    if (mounted) {
      Navigator.of(context).pushReplacement(
        MaterialPageRoute(builder: (context) => const ScanScreen()),
      );
    }
  }

  @override
  Widget build(BuildContext context) {
    return Consumer<DeviceState>(
      builder: (context, deviceState, child) {
        if (!deviceState.isConnected) {
          WidgetsBinding.instance.addPostFrameCallback((_) {
            Navigator.of(context).pushReplacement(
              MaterialPageRoute(builder: (context) => const ScanScreen()),
            );
          });
        }

        return Scaffold(
          appBar: AppBar(
            title: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                const Text('Stepper Control'),
                Text(
                  deviceState.deviceName,
                  style: Theme.of(context).textTheme.bodySmall?.copyWith(
                        color: Colors.white70,
                      ),
                ),
              ],
            ),
            actions: [
              IconButton(
                icon: const Icon(Icons.bluetooth_disabled),
                onPressed: _disconnect,
                tooltip: 'Disconnect',
              ),
            ],
          ),
          body: SingleChildScrollView(
            padding: const EdgeInsets.all(16),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                Card(
                  color: Colors.green.shade50,
                  child: Padding(
                    padding: const EdgeInsets.all(12),
                    child: Row(
                      children: [
                        Icon(Icons.bluetooth_connected,
                            color: Colors.green.shade700),
                        const SizedBox(width: 8),
                        Text(
                          'Connected',
                          style: TextStyle(
                            color: Colors.green.shade700,
                            fontWeight: FontWeight.bold,
                          ),
                        ),
                        const Spacer(),
                        Text(
                          'Mode: ${deviceState.modeString}',
                          style: TextStyle(color: Colors.green.shade700),
                        ),
                      ],
                    ),
                  ),
                ),
                const SizedBox(height: 24),
                Row(
                  children: [
                    Expanded(
                      child: StatusIndicator(
                        label: 'Pedal',
                        isActive: deviceState.pedalPressed,
                        activeColor: Colors.blue,
                        icon: Icons.gamepad,
                      ),
                    ),
                    const SizedBox(width: 8),
                    Expanded(
                      child: StatusIndicator(
                        label: 'Counter',
                        isActive: deviceState.counterComplete,
                        activeColor: Colors.orange,
                        icon: Icons.timer,
                      ),
                    ),
                    const SizedBox(width: 8),
                    Expanded(
                      child: StatusIndicator(
                        label: 'Motor',
                        isActive: deviceState.motorRunning,
                        activeColor: Colors.green,
                        icon: Icons.settings,
                      ),
                    ),
                  ],
                ),
                const SizedBox(height: 32),
                Center(
                  child: Column(
                    children: [
                      Text(
                        'Manual Motor Control',
                        style: Theme.of(context).textTheme.titleMedium,
                      ),
                      const SizedBox(height: 8),
                      Text(
                        'Press and hold to run motor',
                        style: Theme.of(context).textTheme.bodySmall,
                      ),
                      const SizedBox(height: 16),
                      MotorButton(
                        isRunning: deviceState.motorRunning,
                        isEnabled: !deviceState.counterComplete,
                        onStart: _onMotorStart,
                        onStop: _onMotorStop,
                      ),
                      if (deviceState.counterComplete)
                        Padding(
                          padding: const EdgeInsets.only(top: 8),
                          child: Text(
                            'Counter complete - Motor disabled',
                            style: TextStyle(
                              color: Colors.orange.shade700,
                              fontStyle: FontStyle.italic,
                            ),
                          ),
                        ),
                    ],
                  ),
                ),
                const SizedBox(height: 32),
                Card(
                  child: Padding(
                    padding: const EdgeInsets.all(16),
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Row(
                          mainAxisAlignment: MainAxisAlignment.spaceBetween,
                          children: [
                            Text(
                              'Pulse Frequency',
                              style: Theme.of(context).textTheme.titleMedium,
                            ),
                            Text(
                              '${_frequencySlider.round()} Hz',
                              style: Theme.of(context)
                                  .textTheme
                                  .titleLarge
                                  ?.copyWith(
                                    color: Theme.of(context).primaryColor,
                                    fontWeight: FontWeight.bold,
                                  ),
                            ),
                          ],
                        ),
                        const SizedBox(height: 8),
                        Slider(
                          value: _frequencySlider,
                          min: 100,
                          max: 1000,
                          divisions: 18,
                          label: '${_frequencySlider.round()} Hz',
                          onChanged: _onFrequencyChanged,
                          onChangeEnd: _onFrequencyChangeEnd,
                        ),
                        Row(
                          mainAxisAlignment: MainAxisAlignment.spaceBetween,
                          children: [
                            Text('100 Hz',
                                style: Theme.of(context).textTheme.bodySmall),
                            Text('1000 Hz',
                                style: Theme.of(context).textTheme.bodySmall),
                          ],
                        ),
                      ],
                    ),
                  ),
                ),
                const SizedBox(height: 24),
                Row(
                  children: [
                    Expanded(
                      child: OutlinedButton.icon(
                        onPressed: () async {
                          final bleService = context.read<BleService>();
                          await bleService.setAutoMode();
                        },
                        icon: const Icon(Icons.auto_mode),
                        label: const Text('Auto Mode'),
                      ),
                    ),
                    const SizedBox(width: 8),
                    Expanded(
                      child: OutlinedButton.icon(
                        onPressed: () async {
                          final bleService = context.read<BleService>();
                          await bleService.stopMotor();
                        },
                        icon: const Icon(Icons.stop),
                        label: const Text('Force Stop'),
                        style: OutlinedButton.styleFrom(
                          foregroundColor: Colors.red,
                        ),
                      ),
                    ),
                  ],
                ),
              ],
            ),
          ),
        );
      },
    );
  }
}
