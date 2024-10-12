#include "Arduino.h"
#include "TFT_eSPI.h"
#include "CST816S.h"
#include "config.h"
#include <WiFi.h>
#include <EEPROM.h>
#include <time.h>
#include "wifi_config.h"
// #include "bluetooth_hid.h"
#include "ui_manager.h"

LV_IMG_DECLARE(logo);
extern const uint16_t logo_map[];

#define TWATCH_TOUCH_RES 33
#define TWATCH_TOUCH_INT 35
#define TWATCH_IICSCL 25
#define TWATCH_IICSDA 26
#define BUTTON_1 0
#define BUTTON_2 35

static const uint16_t screenWidth = 240;
static const uint16_t screenHeight = 240;
TFT_eSPI tft = TFT_eSPI(screenWidth, screenHeight);

CST816S touch;

const char *coins[] = {"BTC", "ETH", "GMT"};
const int numCoins = sizeof(coins) / sizeof(coins[0]);

unsigned long lastInteractionTime = 0;
const unsigned long sleepDelay = 30000; // 30 seconds of inactivity before sleep

CachedData cachedPrices[numCoins];

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting setup...");
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  pinMode(BUTTON_1, INPUT_PULLUP);
  pinMode(BUTTON_2, INPUT_PULLUP);

  // Vibrate on startup
  Serial.println("Vibrating...");
  delay(200);

  Wire.begin(TWATCH_IICSDA, TWATCH_IICSCL);
  touch.begin(Wire, TWATCH_TOUCH_RES, TWATCH_TOUCH_INT);

  Serial.println("Initializing display...");
  tft.begin();
  tft.setRotation(0);
  tft.setSwapBytes(true);
  tft.pushImage(0, 0, 240, 240, (uint16_t *)logo_map);

  delay(1000);
  Serial.println("Initializing UI...");
  uiManager.init(tft, touch);
  uiManager.createUI();

  // bluetoothHID.setup();

  Serial.println("Initializing EEPROM...");
  EEPROM.begin(sizeof(CachedData) * numCoins);
  EEPROM.get(0, cachedPrices);

  initWiFi();

  Serial.println("Setup complete");
}

void loop()
{
  uiManager.loop();
  uiManager.handleButtons();

  // if (bluetoothHID.isConnected())
  // {
  //   // Device is connected, show media UI
  //   uiManager.setTile(numCoins);
  // }
  // else
  // {
  //   // Device is not connected, show pairing UI
  //   uiManager.setTile(numCoins + 2);
  // }

  if (lastInteractionTime > 0 && (millis() - lastInteractionTime) > sleepDelay)
  {
    enterSleepMode();
  }

  delay(5);
}

void enterSleepMode()
{
  digitalWrite(TFT_BL, LOW);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_1, LOW);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_2, LOW);
  esp_deep_sleep_start();
}

void initWiFi()
{
  Serial.println("Initializing WiFi...");
  WiFi.mode(WIFI_STA);

  String WIFI_SSID = "";
  String WIFI_PASSWORD = "";

  if (WiFiConfig::loadWiFiCredentials(WIFI_SSID, WIFI_PASSWORD))
  {
    Serial.println("Connecting to WiFi...");
    if (WiFiConfig::connect(WIFI_SSID.c_str(), WIFI_PASSWORD.c_str()))
    {
      Serial.println("WiFi connected. IP address: " + WiFi.localIP().toString());
      configTime(60 * 60 * 8, 0, "pool.ntp.org"); // Configure NTP
      uiManager.updateCryptoPrice(NULL);          // Trigger an immediate update when connected
      Serial.println("NTP configured");
    }
    else
    {
      Serial.println("Failed to connect to WiFi. Entering configuration mode...");
      enterConfigMode();
    }
  }
  else
  {
    Serial.println("Entering configuration mode...");
    enterConfigMode();
  }
}

void enterConfigMode()
{
  Serial.println("Entering WiFi configuration mode");
  WiFiConfig::startConfigPortal();
  // After exiting the config portal, try to connect again
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("WiFi connected after configuration. IP address: " + WiFi.localIP().toString());
    configTime(60 * 60 * 8, 0, "pool.ntp.org"); // Configure NTP
    uiManager.updateCryptoPrice(NULL);          // Trigger an immediate update when connected
    Serial.println("NTP configured");
  }
  else
  {
    Serial.println("Failed to connect to WiFi after configuration. Restarting...");
    ESP.restart();
  }
}
