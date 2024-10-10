# Crypto Meter

This project is an ESP32-based cryptocurrency price tracker.

## Configuration

To set up the project:

1. Copy `src/config_template.h` to `src/config.h`
2. Edit `src/config.h` and replace the placeholder values with your actual WiFi credentials:

   ```cpp
   #define WIFI_SSID "YOUR_WIFI_SSID"
   #define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
   ```

3. Do not commit `src/config.h` to version control, as it contains sensitive information.

## Building and Running

(Add instructions for building and running the project here)
