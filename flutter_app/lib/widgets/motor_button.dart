import 'package:flutter/material.dart';

class MotorButton extends StatelessWidget {
  final bool isRunning;
  final bool isEnabled;
  final VoidCallback? onStart;
  final VoidCallback? onStop;

  const MotorButton({
    super.key,
    required this.isRunning,
    required this.isEnabled,
    this.onStart,
    this.onStop,
  });

  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      onTapDown: isEnabled ? (_) => onStart?.call() : null,
      onTapUp: isEnabled ? (_) => onStop?.call() : null,
      onTapCancel: isEnabled ? () => onStop?.call() : null,
      child: AnimatedContainer(
        duration: const Duration(milliseconds: 150),
        width: 120,
        height: 120,
        decoration: BoxDecoration(
          shape: BoxShape.circle,
          gradient: LinearGradient(
            begin: Alignment.topLeft,
            end: Alignment.bottomRight,
            colors: isRunning
                ? [Colors.red.shade400, Colors.red.shade700]
                : isEnabled
                    ? [Colors.blue.shade400, Colors.blue.shade700]
                    : [Colors.grey.shade400, Colors.grey.shade600],
          ),
          boxShadow: [
            BoxShadow(
              color: isRunning
                  ? Colors.red.withOpacity(0.4)
                  : isEnabled
                      ? Colors.blue.withOpacity(0.4)
                      : Colors.grey.withOpacity(0.2),
              blurRadius: isRunning ? 20 : 10,
              spreadRadius: isRunning ? 5 : 2,
            ),
          ],
        ),
        child: Center(
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              Icon(
                isRunning ? Icons.stop : Icons.play_arrow,
                color: Colors.white,
                size: 40,
              ),
              Text(
                isRunning ? 'Running' : 'Press',
                style: const TextStyle(
                  color: Colors.white,
                  fontWeight: FontWeight.bold,
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}
