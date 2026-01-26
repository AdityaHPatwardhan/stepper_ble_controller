import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'models/device_state.dart';
import 'services/ble_service.dart';
import 'screens/scan_screen.dart';

void main() {
  runApp(const StepperBleApp());
}

class StepperBleApp extends StatelessWidget {
  const StepperBleApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MultiProvider(
      providers: [
        ChangeNotifierProvider(create: (_) => DeviceState()),
        ProxyProvider<DeviceState, BleService>(
          update: (_, deviceState, previous) =>
              previous ?? BleService(deviceState),
          dispose: (_, service) => service.dispose(),
        ),
      ],
      child: MaterialApp(
        title: 'Stepper BLE Controller',
        debugShowCheckedModeBanner: false,
        theme: ThemeData(
          colorScheme: ColorScheme.fromSeed(seedColor: Colors.blue),
          useMaterial3: true,
          appBarTheme: const AppBarTheme(
            centerTitle: false,
            elevation: 2,
          ),
        ),
        home: const ScanScreen(),
      ),
    );
  }
}
