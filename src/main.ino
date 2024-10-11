#include "Arduino.h"
#include "TFT_eSPI.h"
#include "CST816S.h"
#include "config.h"
#include "lvgl.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <time.h>
#include "wifi_config.h"

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

#define WAIT 1000

void lvgl_test(void);
void create_crypto_watch(lv_obj_t *parent, int index);
void create_system_info(lv_obj_t *parent);

static lv_color_t *buf = NULL;
static lv_disp_draw_buf_t draw_buf;
static lv_obj_t *dis = NULL;
CST816S touch;

const char *coins[] = {"BTC", "ETH", "GMT"};
const int numCoins = sizeof(coins) / sizeof(coins[0]);

struct CoinInfo
{
  String currentPrice;
  float priceChangePercentage;
  float dailyAverage;
  float dailyHigh;
  float dailyLow;
};

CoinInfo coinInfos[numCoins];

lv_obj_t *meters[numCoins];
lv_meter_indicator_t *price_indicators[numCoins];
lv_obj_t *price_labels[numCoins];
lv_obj_t *change_labels[numCoins];

int currentCoinIndex = 0;

unsigned long lastInteractionTime = 0;
const unsigned long sleepDelay = 30000; // 30 seconds of inactivity before sleep

struct CachedData
{
  float price;
  float change;
  unsigned long timestamp;
};

CachedData cachedPrices[numCoins];

lv_obj_t *date_label;
lv_obj_t *time_label;
lv_obj_t *wifi_label;
lv_obj_t *last_update_label;

void my_print(const char *buf)
{
  Serial.printf(buf);
  Serial.flush();
}

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  tft.pushImage(area->x1, area->y1, w, h, (uint16_t *)&color_p->full);
  lv_disp_flush_ready(disp);
}

void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{
  if (touch.ReadTouch())
  {
    uint16_t x = touch.getX();
    uint16_t y = touch.getY();

    data->state = LV_INDEV_STATE_PR;
    data->point.x = x;
    data->point.y = y;
    lastInteractionTime = millis();
  }
  else
  {
    data->state = LV_INDEV_STATE_REL;
  }
}

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
  Serial.println("Initializing LVGL...");
  lv_init();

  size_t buffer_size = sizeof(lv_color_t) * EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES;
  buf = (lv_color_t *)ps_malloc(buffer_size);
  if (buf == NULL)
  {
    Serial.println("Failed to allocate display buffer");
    while (1)
    {
      delay(500);
    }
  }
  lv_disp_draw_buf_init(&draw_buf, buf, NULL, buffer_size);

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = EXAMPLE_LCD_H_RES;
  disp_drv.ver_res = EXAMPLE_LCD_V_RES;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev_drv);

  Serial.println("Initializing EEPROM...");
  EEPROM.begin(sizeof(CachedData) * numCoins);
  EEPROM.get(0, cachedPrices);

  Serial.println("Creating LVGL UI...");
  lvgl_test();

  Serial.println("LVGL UI created");

  initWiFi();

  Serial.println("Setup complete");
}

void lvgl_test(void)
{
  dis = lv_tileview_create(lv_scr_act());
  lv_obj_align(dis, LV_ALIGN_TOP_RIGHT, 0, 0);
  lv_obj_set_size(dis, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(dis, lv_color_hex(0x000000), LV_PART_MAIN);

  for (int i = 0; i < numCoins; i++)
  {
    lv_obj_t *tv = lv_tileview_add_tile(dis, i, 0, LV_DIR_HOR);
    create_crypto_watch(tv, i);
  }

  lv_obj_t *system_info_tile = lv_tileview_add_tile(dis, numCoins, 0, LV_DIR_HOR);
  create_system_info(system_info_tile);

  lv_timer_create(updateCryptoPrice, 60000 * 5, NULL); // Update every 60 seconds
  lv_timer_create(updateSystemInfo, 1000, NULL);       // Update system info every second
}

void create_crypto_watch(lv_obj_t *parent, int index)
{
  meters[index] = lv_meter_create(parent);
  lv_obj_center(meters[index]);
  lv_obj_set_size(meters[index], 240, 240);

  lv_meter_scale_t *scale = lv_meter_add_scale(meters[index]);
  lv_meter_set_scale_ticks(meters[index], scale, 21, 2, 10, lv_palette_main(LV_PALETTE_GREY));
  lv_meter_set_scale_major_ticks(meters[index], scale, 5, 4, 15, lv_color_black(), 10);
  lv_meter_set_scale_range(meters[index], scale, -10, 10, 270, 135); // -10% to +10%, 270 degree arc

  price_indicators[index] = lv_meter_add_needle_line(meters[index], scale, 4, lv_palette_main(LV_PALETTE_RED), -10);

  price_labels[index] = lv_label_create(parent);
  lv_obj_align(price_labels[index], LV_ALIGN_CENTER, 0, 20);
  lv_label_set_text(price_labels[index], "Loading...");

  change_labels[index] = lv_label_create(parent);
  lv_obj_align(change_labels[index], LV_ALIGN_CENTER, 0, 40);
  lv_label_set_text(change_labels[index], "24h: --");

  lv_obj_t *coin_name = lv_label_create(parent);
  lv_obj_align(coin_name, LV_ALIGN_TOP_MID, 0, 10);
  lv_label_set_text(coin_name, coins[index]);
}

void create_system_info(lv_obj_t *parent)
{

  date_label = lv_label_create(parent);
  lv_obj_align(date_label, LV_ALIGN_TOP_MID, 0, 30);

  time_label = lv_label_create(parent);
  lv_obj_align(time_label, LV_ALIGN_TOP_MID, 0, 60);
  lv_label_set_text(time_label, "Time: --:--:--");

  // wifi_label = lv_label_create(parent);
  // lv_obj_align(wifi_label, LV_ALIGN_TOP_MID, 0, 60);
  // lv_label_set_text(wifi_label, "WiFi: --");

  last_update_label = lv_label_create(parent);
  lv_obj_align(last_update_label, LV_ALIGN_TOP_MID, 0, 100);
  lv_label_set_text(last_update_label, "Last update: --:--:--");
}

void getCryptoPrices()
{
  Serial.println("Fetching crypto prices...");
  HTTPClient http;
  String url = String(API_HOST) + "/api/crypto?symbols=";
  for (int i = 0; i < numCoins; i++)
  {
    url += coins[i];
    if (i < numCoins - 1)
      url += ",";
  }
  http.begin(url);
  int httpCode = http.GET();

  Serial.printf("HTTP response code: %d\n", httpCode);

  if (httpCode == HTTP_CODE_OK)
  {
    String payload = http.getString();
    DynamicJsonDocument doc(1024 * numCoins);
    DeserializationError error = deserializeJson(doc, payload);

    if (!error)
    {
      for (int i = 0; i < numCoins; i++)
      {
        float price = doc[i]["lastPrice"];
        coinInfos[i].currentPrice = String(price, 3);
        coinInfos[i].priceChangePercentage = doc[i]["priceChangePercent"];

        // Update cached data
        cachedPrices[i].price = price;
        cachedPrices[i].change = coinInfos[i].priceChangePercentage;
        cachedPrices[i].timestamp = millis();

        Serial.printf("Updated %s: Price: $%s, Change: %.2f%%\n", coins[i], coinInfos[i].currentPrice.c_str(), coinInfos[i].priceChangePercentage);
      }
      EEPROM.put(0, cachedPrices);
      EEPROM.commit();

      // Update last update time
      time_t now;
      time(&now);
      char buffer[20];
      strftime(buffer, sizeof(buffer), "%H:%M:%S", localtime(&now));
      if (last_update_label != NULL)
      {
        lv_label_set_text(last_update_label, (String("Last update: ") + buffer).c_str());
      }
    }
    else
    {
      Serial.println("Failed to parse JSON");
    }
  }
  else
  {
    Serial.println("Failed to fetch crypto prices");
  }
  http.end();
}

void updateCryptoPrice(lv_timer_t *timer)
{
  Serial.println("Updating crypto prices...");
  if (WiFi.status() == WL_CONNECTED)
  {
    getCryptoPrices();
    for (int i = 0; i < numCoins; i++)
    {
      lv_label_set_text(price_labels[i], coinInfos[i].currentPrice.c_str());

      int meterValue = map(coinInfos[i].priceChangePercentage * 10, -100, 100, -10, 10);
      lv_meter_set_indicator_value(meters[i], price_indicators[i], meterValue);

      String changeText = "24h: " + String(coinInfos[i].priceChangePercentage, 2) + "%";
      lv_label_set_text(change_labels[i], changeText.c_str());

      // Change color based on price change
      lv_color_t color;
      if (coinInfos[i].priceChangePercentage > 0)
      {
        color = lv_palette_main(LV_PALETTE_GREEN);
      }
      else if (coinInfos[i].priceChangePercentage < 0)
      {
        color = lv_palette_main(LV_PALETTE_RED);
      }
      else
      {
        color = lv_palette_main(LV_PALETTE_GREY);
      }
      lv_obj_set_style_text_color(price_labels[i], color, 0);
      lv_obj_set_style_text_color(change_labels[i], color, 0);

      if (abs(coinInfos[i].priceChangePercentage) > 5.0)
      {
        delay(200);
      }
    }
  }
  else
  {
    Serial.println("WiFi disconnected. Attempting to reconnect...");
    initWiFi(); // Try to reconnect
  }
  lv_timer_handler(); // Update the display
}

void updateSystemInfo(lv_timer_t *timer)
{
  time_t now;
  time(&now);
  char buffer[20];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d", localtime(&now));
  lv_label_set_text(date_label, (String("Date: ") + buffer).c_str());

  strftime(buffer, sizeof(buffer), "%H:%M:%S", localtime(&now));
  lv_label_set_text(time_label, (String("Time: ") + buffer).c_str());

  // lv_label_set_text(wifi_label, (String("WiFi: ") + (WiFi.status() == WL_CONNECTED ? WiFi.SSID() : "Disconnected")).c_str());

  lv_timer_handler(); // Update the display
}

void handleButtons()
{
  static bool lastButton1State = HIGH;
  static bool lastButton2State = HIGH;
  bool currentButton1State = digitalRead(BUTTON_1);
  bool currentButton2State = digitalRead(BUTTON_2);

  if (lastButton1State == HIGH && currentButton1State == LOW)
  {
    if (digitalRead(TFT_BL) == LOW)
    {
      digitalWrite(TFT_BL, HIGH);
    }

    currentCoinIndex = (currentCoinIndex + 1) % (numCoins + 1); // +1 for system info page
    lv_obj_set_tile_id(dis, currentCoinIndex, 0, LV_ANIM_ON);
    lastInteractionTime = millis();
    if (currentCoinIndex < numCoins && coinInfos[currentCoinIndex].currentPrice == "Loading...")
    {
      updateCryptoPrice(NULL); // Trigger an update if there's no current price
    }
  }

  if (lastButton2State == HIGH && currentButton2State == LOW)
  {
    if (digitalRead(TFT_BL) == LOW)
    {
      digitalWrite(TFT_BL, HIGH);
    }

    currentCoinIndex = (currentCoinIndex + numCoins) % (numCoins + 1); // +1 for system info page
    lv_obj_set_tile_id(dis, currentCoinIndex, 0, LV_ANIM_ON);
    lastInteractionTime = millis();
    if (currentCoinIndex < numCoins && coinInfos[currentCoinIndex].currentPrice == "Loading...")
    {
      updateCryptoPrice(NULL); // Trigger an update if there's no current price
    }
  }

  lastButton1State = currentButton1State;
  lastButton2State = currentButton2State;
}

void enterSleepMode()
{
  digitalWrite(TFT_BL, LOW);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_1, LOW);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_2, LOW);
  esp_deep_sleep_start();
}

void loop()
{
  lv_timer_handler();
  handleButtons();

  if (lastInteractionTime > 0 && (millis() - lastInteractionTime) > sleepDelay)
  {
    enterSleepMode();
  }

  delay(5);
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
      updateCryptoPrice(NULL);                    // Trigger an immediate update when connected
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
    updateCryptoPrice(NULL);                    // Trigger an immediate update when connected
    Serial.println("NTP configured");
  }
  else
  {
    Serial.println("Failed to connect to WiFi after configuration. Restarting...");
    ESP.restart();
  }
}