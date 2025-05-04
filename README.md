# Locksy - Smart Lock System

A comprehensive smart lock system with web interface, RFID, keypad, and fingerprint support.

## Features

- 4x4 Keypad support
- RFID RC522 integration
- OLED/LCD Display
- Web panel with Light/Dark mode
- WiFi connectivity
- OTA updates
- Logging system
- Configurable pins
- Hall sensor/Microswitch support
- Active High/Low relay and buzzer
- Status LEDs
- User management
- Model selection
- Auto reboot scheduling
- Log download
- Pin configuration
- Advanced settings

## Installation

1. Install PlatformIO in VS Code
2. Clone this repository
3. Open the project in PlatformIO
4. Connect your ESP8266 board
5. Upload the filesystem (data folder)
6. Upload the code

## Configuration

1. After uploading, the device will create a WiFi network named "Locksy-Setup"
2. Connect to this network (password: 12345678)
3. Open 192.168.4.1 in your browser
4. Configure your WiFi settings
5. The device will restart and connect to your network

## Usage

- Access the web interface at the device's IP address
- Default admin credentials: admin/admin123
- Configure users, pins, and settings through the web interface
- Monitor system status and logs
- Control the lock remotely

## Hardware Requirements

- ESP8266 board
- RFID RC522 module
- 4x4 Keypad
- Relay module
- Door sensor
- Status LED
- Buzzer
- Optional: Fingerprint sensor, temperature sensor, battery monitor

## License

MIT License 
