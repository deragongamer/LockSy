# Locksy Bot - ESP Edition 🔒

A smart lock system based on ESP8266 with web interface, RFID support, and advanced security features.

## Features ✨

### Hardware Support
- 4x4 Matrix Keypad
- RFID RC522 Module
- OLED/LCD Display
- Door Sensor (Hall Effect or Microswitch)
- Relay Control (Active High/Low configurable)
- Buzzer (Active High/Low configurable)
- Status LEDs for various states

### Security
- Admin password (changeable through web panel)
- 42-digit Developer password (hashed)
- Brute-force protection
- Unique device serial number
- Remote locking capability
- Access logging with CSV export

### Web Interface
- Modern UI with Light/Dark mode
- Real-time status monitoring
- Pin configuration through web panel
- Log viewer and download
- OTA firmware updates
- WiFi configuration
- Device settings management

### Smart Features
- Online time sync (time.ir or NTP)
- Automatic reboot scheduling
- Remote lock/unlock via serial number
- Configurable auto-lock
- Status LED indicators for:
  - WiFi connection
  - Access granted/denied
  - System status
  - Door state

## Hardware Requirements 🛠

- ESP8266 development board
- RC522 RFID reader
- 4x4 Matrix keypad
- OLED/LCD display
- Relay module
- Buzzer
- Status LEDs
- Door sensor (Hall effect or microswitch)
- Power supply

## Pin Configuration 📌

Default pins (configurable through web panel):
- RFID_SS_PIN: D4
- RFID_RST_PIN: D3
- RELAY_PIN: D8
- BUZZER_PIN: D0
- DOOR_SENSOR_PIN: D1
- STATUS_LED_PIN: D2
- Keypad ROWS: D5, D6, D7, D8
- Keypad COLS: D0, D1, D2, D3

## Setup Instructions 🚀

1. Install required libraries:
   - ESP8266WiFi
   - ESP8266WebServer
   - MFRC522
   - Wire
   - SPI
   - Keypad
   - LittleFS
   - ArduinoJson
   - TimeLib

2. Upload the code to your ESP8266

3. First boot:
   - Device will create an AP named "Locksy-Setup"
   - Connect to it and configure WiFi credentials
   - Device will restart and connect to your network

4. Access web interface:
   - Go to the IP address shown on the display
   - Default admin password: 123456
   - Configure pins and other settings

## Web Interface Features 🌐

### Dashboard
- Real-time door status
- WiFi signal strength
- System uptime
- Last access time
- Quick lock/unlock controls

### Settings
- Pin configuration
- WiFi settings
- Admin password change
- Auto-lock timing
- Reboot scheduling
- Active High/Low configuration for relay and buzzer

### Logs
- Access history
- Error logs
- CSV export
- Log clearing

## Security Features 🛡

- Brute-force protection: System locks for 5 minutes after 3 wrong attempts
- Developer password: 42-digit hash for maintenance access
- Remote locking: Lock device using unique serial number
- Access logging: All attempts (successful or not) are logged
- Secure OTA: Updates only accepted from verified sources

## Contributing 🤝

Feel free to submit issues and enhancement requests!

## License 📄

This project is licensed under the MIT License - see the LICENSE file for details. 