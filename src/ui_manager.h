#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include "lvgl.h"
#include "TFT_eSPI.h"
#include "CST816S.h"

extern const int numCoins;
extern unsigned long lastInteractionTime;

struct CachedData
{
    float price;
    float change;
    unsigned long timestamp;
};

class UIManager
{
public:
    UIManager();
    void init(TFT_eSPI &tft, CST816S &touch);
    void createUI();
    void updateCryptoPrice(lv_timer_t *timer);
    void updateSystemInfo(lv_timer_t *timer);
    void updateMediaInfo(lv_timer_t *timer);
    void handleButtons();
    void setTile(int index);
    void loop();

private:
    void createCryptoWatch(lv_obj_t *parent, int index);
    void createSystemInfo(lv_obj_t *parent);
    void createMediaUI(lv_obj_t *parent);
    void createPairingUI(lv_obj_t *parent);
    void getCryptoPrices();

    static void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);
    static void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data);

    static void onPlayPauseClicked(lv_event_t *e);
    static void onNextClicked(lv_event_t *e);
    static void onPrevClicked(lv_event_t *e);

    TFT_eSPI *tft;
    CST816S *touch;
    lv_obj_t *dis;
    lv_color_t *buf;
    lv_disp_draw_buf_t draw_buf;
};

extern UIManager uiManager;

#endif // UI_MANAGER_H
