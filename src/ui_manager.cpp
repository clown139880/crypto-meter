#include "ui_manager.h"
#include "config.h"
// #include "bluetooth_hid.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>

UIManager uiManager;

extern const char *coins[];
extern CachedData cachedPrices[];

struct CoinInfo
{
    String currentPrice;
    float priceChangePercentage;
};

CoinInfo coinInfos[3]; // Assuming 3 coins as in the original code

lv_obj_t *meters[3];
lv_meter_indicator_t *price_indicators[3];
lv_obj_t *price_labels[3];
lv_obj_t *change_labels[3];

lv_obj_t *date_label;
lv_obj_t *time_label;
lv_obj_t *last_update_label;

// Media UI elements
lv_obj_t *track_label;
lv_obj_t *artist_label;
lv_obj_t *status_label;
lv_obj_t *play_pause_btn;
lv_obj_t *next_btn;
lv_obj_t *prev_btn;

String currentTrack = "No track";
String currentArtist = "No artist";
String playbackStatus = "Stopped";

UIManager::UIManager() : tft(nullptr), touch(nullptr), dis(nullptr), buf(nullptr) {}

void UIManager::init(TFT_eSPI &tft, CST816S &touch)
{
    this->tft = &tft;
    this->touch = &touch;

    lv_init();

    size_t buffer_size = sizeof(lv_color_t) * EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES;
    buf = (lv_color_t *)ps_malloc(buffer_size);
    if (buf == nullptr)
    {
        Serial.println("Failed to allocate display buffer");
        return;
    }
    lv_disp_draw_buf_init(&draw_buf, buf, nullptr, buffer_size);

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
}

void UIManager::createUI()
{
    dis = lv_tileview_create(lv_scr_act());
    lv_obj_align(dis, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_size(dis, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(dis, lv_color_hex(0x000000), LV_PART_MAIN);

    for (int i = 0; i < numCoins; i++)
    {
        lv_obj_t *tv = lv_tileview_add_tile(dis, i, 0, LV_DIR_HOR);
        createCryptoWatch(tv, i);
    }

    lv_obj_t *media_tile = lv_tileview_add_tile(dis, numCoins, 0, LV_DIR_HOR);
    createMediaUI(media_tile);

    lv_obj_t *system_info_tile = lv_tileview_add_tile(dis, numCoins + 1, 0, LV_DIR_HOR);
    createSystemInfo(system_info_tile);

    lv_obj_t *pairing_tile = lv_tileview_add_tile(dis, numCoins + 2, 0, LV_DIR_HOR);
    createPairingUI(pairing_tile);

    lv_timer_create([](lv_timer_t *timer)
                    { uiManager.updateCryptoPrice(timer); }, 60000 * 5, nullptr);
    lv_timer_create([](lv_timer_t *timer)
                    { uiManager.updateSystemInfo(timer); }, 1000, nullptr);
    lv_timer_create([](lv_timer_t *timer)
                    { uiManager.updateMediaInfo(timer); }, 5000, nullptr);
}

void UIManager::createCryptoWatch(lv_obj_t *parent, int index)
{
    meters[index] = lv_meter_create(parent);
    lv_obj_center(meters[index]);
    lv_obj_set_size(meters[index], 240, 240);

    lv_meter_scale_t *scale = lv_meter_add_scale(meters[index]);
    lv_meter_set_scale_ticks(meters[index], scale, 21, 2, 10, lv_palette_main(LV_PALETTE_GREY));
    lv_meter_set_scale_major_ticks(meters[index], scale, 5, 4, 15, lv_color_black(), 10);
    lv_meter_set_scale_range(meters[index], scale, -10, 10, 270, 135);

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

void UIManager::createSystemInfo(lv_obj_t *parent)
{
    date_label = lv_label_create(parent);
    lv_obj_align(date_label, LV_ALIGN_TOP_MID, 0, 30);

    time_label = lv_label_create(parent);
    lv_obj_align(time_label, LV_ALIGN_TOP_MID, 0, 60);
    lv_label_set_text(time_label, "Time: --:--:--");

    last_update_label = lv_label_create(parent);
    lv_obj_align(last_update_label, LV_ALIGN_TOP_MID, 0, 100);
    lv_label_set_text(last_update_label, "Last update: --:--:--");
}

void UIManager::createMediaUI(lv_obj_t *parent)
{
    track_label = lv_label_create(parent);
    lv_obj_align(track_label, LV_ALIGN_TOP_MID, 0, 20);
    lv_label_set_text(track_label, currentTrack.c_str());

    artist_label = lv_label_create(parent);
    lv_obj_align(artist_label, LV_ALIGN_TOP_MID, 0, 50);
    lv_label_set_text(artist_label, currentArtist.c_str());

    status_label = lv_label_create(parent);
    lv_obj_align(status_label, LV_ALIGN_TOP_MID, 0, 80);
    lv_label_set_text(status_label, playbackStatus.c_str());

    prev_btn = lv_btn_create(parent);
    lv_obj_align(prev_btn, LV_ALIGN_BOTTOM_MID, -70, -20);
    lv_obj_t *prev_label = lv_label_create(prev_btn);
    lv_label_set_text(prev_label, LV_SYMBOL_PREV);
    lv_obj_add_event_cb(prev_btn, onPrevClicked, LV_EVENT_CLICKED, nullptr);

    play_pause_btn = lv_btn_create(parent);
    lv_obj_align(play_pause_btn, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_t *play_pause_label = lv_label_create(play_pause_btn);
    lv_label_set_text(play_pause_label, LV_SYMBOL_PLAY);
    lv_obj_add_event_cb(play_pause_btn, onPlayPauseClicked, LV_EVENT_CLICKED, nullptr);

    next_btn = lv_btn_create(parent);
    lv_obj_align(next_btn, LV_ALIGN_BOTTOM_MID, 70, -20);
    lv_obj_t *next_label = lv_label_create(next_btn);
    lv_label_set_text(next_label, LV_SYMBOL_NEXT);
    lv_obj_add_event_cb(next_btn, onNextClicked, LV_EVENT_CLICKED, nullptr);
}

void UIManager::createPairingUI(lv_obj_t *parent)
{
    lv_obj_t *pairing_label = lv_label_create(parent);
    lv_obj_align(pairing_label, LV_ALIGN_CENTER, 0, -20);
    lv_label_set_text(pairing_label, "Waiting for Bluetooth connection...");

    lv_obj_t *instructions_label = lv_label_create(parent);
    lv_obj_align(instructions_label, LV_ALIGN_CENTER, 0, 20);
    lv_label_set_text(instructions_label, "Connect to 'ESP32 Media Controller'");
}

void UIManager::updateCryptoPrice(lv_timer_t *timer)
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
        }
    }
    else
    {
        Serial.println("WiFi disconnected. Unable to update prices.");
    }
}

void UIManager::updateSystemInfo(lv_timer_t *timer)
{
    time_t now;
    time(&now);
    char buffer[20];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d", localtime(&now));
    lv_label_set_text(date_label, (String("Date: ") + buffer).c_str());

    strftime(buffer, sizeof(buffer), "%H:%M:%S", localtime(&now));
    lv_label_set_text(time_label, (String("Time: ") + buffer).c_str());
}

void UIManager::updateMediaInfo(lv_timer_t *timer)
{
    // In a real implementation, you would update these values based on the actual media information
    // received from the connected device via Bluetooth
    lv_label_set_text(track_label, currentTrack.c_str());
    lv_label_set_text(artist_label, currentArtist.c_str());
    lv_label_set_text(status_label, playbackStatus.c_str());

    // Update play/pause button icon based on playback status
    lv_obj_t *play_pause_label = lv_obj_get_child(play_pause_btn, 0);
    if (playbackStatus == "Playing")
    {
        lv_label_set_text(play_pause_label, LV_SYMBOL_PAUSE);
    }
    else
    {
        lv_label_set_text(play_pause_label, LV_SYMBOL_PLAY);
    }
}

void UIManager::handleButtons()
{
    // This method would be called in the main loop to handle physical button presses
    // Implement the logic for handling button presses here
    // For example, you might want to change tiles or trigger media controls
}

void UIManager::setTile(int index)
{
    lv_obj_set_tile_id(dis, index, 0, LV_ANIM_ON);
}

void UIManager::loop()
{
    lv_timer_handler();
}

void UIManager::getCryptoPrices()
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

void UIManager::my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    uiManager.tft->pushImage(area->x1, area->y1, w, h, (uint16_t *)&color_p->full);
    lv_disp_flush_ready(disp);
}

void UIManager::my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{
    if (uiManager.touch->ReadTouch())
    {
        uint16_t x = uiManager.touch->getX();
        uint16_t y = uiManager.touch->getY();
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

void UIManager::onPlayPauseClicked(lv_event_t *e)
{
    // bluetoothHID.sendMediaKeyPress(1 << 3); // Play/Pause
    if (playbackStatus == "Playing")
    {
        playbackStatus = "Paused";
    }
    else
    {
        playbackStatus = "Playing";
    }
    uiManager.updateMediaInfo(nullptr);
}

void UIManager::onNextClicked(lv_event_t *e)
{
    // bluetoothHID.sendMediaKeyPress(1 << 0); // Next Track
    uiManager.updateMediaInfo(nullptr);
}

void UIManager::onPrevClicked(lv_event_t *e)
{
    // bluetoothHID.sendMediaKeyPress(1 << 1); // Previous Track
    uiManager.updateMediaInfo(nullptr);
}
