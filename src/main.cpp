#include <Arduino.h>
#include <ESP_Panel_Library.h>
#include <ESP_IOExpander_Library.h>
#include <lvgl.h>
#include "lvgl_port_v8.h"
#include "esp_system.h" 
#include "esp_psram.h"
#include "esp_chip_info.h"

// --- ZAŁĄCZANIE PLIKÓW ---
#include "huge_70.c" 
// Używamy pliku, który zadziałał u Ciebie

// --- DEFINICJE PINÓW ---
#define TP_RST 1
#define LCD_BL 2
#define LCD_RST 3
#define SD_CS 4

#define SCREEN_TIMEOUT_MS 120000 

// --- KOLORYSTKA ---
#define COL_BG_SCREEN       lv_color_hex(0x000000) 
#define COL_CARD_INACTIVE   lv_color_hex(0x1F1F1F) 
#define COL_CARD_ACTIVE     lv_color_hex(0x1B301B) 
#define COL_ACCENT_GREEN    lv_color_hex(0x2ECC71) 
#define COL_ACCENT_ORANGE   lv_color_hex(0xE67E22) 
#define COL_ACCENT_RED      lv_color_hex(0xE74C3C) // Czerwony dla ZATRZYMANA
#define COL_TEXT_WHITE      lv_color_hex(0xFFFFFF) 
#define COL_TEXT_GREY       lv_color_hex(0x999999) 

// --- ZMIENNE GLOBALNE ---
ESP_IOExpander_CH422G *expander = NULL;
lv_obj_t *screensaver_btn = NULL;
bool is_screen_off = false;

// Deklaracje
LV_FONT_DECLARE(font_72);
LV_FONT_DECLARE(font_48);
// Deklaracja zmiennej obrazka (z pliku clear_day.c)
LV_IMG_DECLARE(clear_day); 

// Obiekty UI
static lv_obj_t * label_time;
static lv_obj_t * label_date;
static lv_obj_t * label_temp;
static lv_obj_t * label_weather_desc;

// Obiekty UI - Brama
lv_obj_t * btn_gate;        
lv_obj_t * label_gate_status;
// Obiekty UI - Garaż
lv_obj_t * btn_garage;
lv_obj_t * label_garage_status;


// --- STANY BRAMY ---
enum GateState {
    GATE_CLOSED = 0,
    GATE_OPEN = 1,
    GATE_MOVING = 2,
    GATE_STOPPED = 3 // Nowy stan
};

// --- FUNKCJA ANIMACJI RAMKI (MIGANIE) ---
static void anim_border_opa_cb(void * var, int32_t v) {
    lv_obj_set_style_border_opa((lv_obj_t*)var, v, 0);
}

// --- OBSŁUGA EKRANU ---
static void screensaver_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED) {
        lv_disp_trig_activity(NULL); 
        if (is_screen_off) {
            if(expander) expander->digitalWrite(LCD_BL, HIGH);
            is_screen_off = false;
        }
        lv_obj_add_flag(screensaver_btn, LV_OBJ_FLAG_HIDDEN);
    }
}

static void inactivity_check_cb(lv_timer_t * timer) {
    if (is_screen_off) return;
    if (lv_disp_get_inactive_time(NULL) > SCREEN_TIMEOUT_MS) {
        lv_obj_clear_flag(screensaver_btn, LV_OBJ_FLAG_HIDDEN);
        if(expander) expander->digitalWrite(LCD_BL, LOW);
        is_screen_off = true;
    }
}

static void clock_timer_cb(lv_timer_t * timer) {
    static int hour = 21;
    static int minute = 34;
    static unsigned long last_ms = 0;
    if (millis() - last_ms > 60000) { 
        minute++;
        if (minute > 59) { minute = 0; hour++; }
        if (hour > 23) hour = 0;
        last_ms = millis();
        lv_label_set_text_fmt(label_time, "%02d:%02d", hour, minute);
    }
}

// --- AKTUALIZACJA STANU BRAMY ---
void update_gate_state(int state) {
    if (!btn_gate || !label_gate_status) return;
    lv_obj_t* top_part = lv_obj_get_child(btn_gate, 0);
    lv_obj_t* icon = NULL;
    if(top_part) icon = lv_obj_get_child(top_part, 0);

    // 1. Zawsze czyścimy animację przy zmianie stanu
    lv_anim_del(btn_gate, anim_border_opa_cb);
    lv_obj_set_style_border_opa(btn_gate, LV_OPA_COVER, 0); // Reset widoczności ramki

    switch (state) {
        case GATE_CLOSED:
            lv_obj_set_style_bg_color(btn_gate, COL_CARD_INACTIVE, 0);
            lv_obj_set_style_border_width(btn_gate, 0, 0); 
            lv_label_set_text(label_gate_status, "ZAMKNIETA");
            lv_obj_set_style_text_color(label_gate_status, COL_TEXT_GREY, 0);
            if(icon) lv_obj_set_style_text_color(icon, COL_TEXT_GREY, 0);
            break;

        case GATE_OPEN:
            lv_obj_set_style_bg_color(btn_gate, COL_CARD_ACTIVE, 0);
            lv_obj_set_style_border_color(btn_gate, COL_ACCENT_GREEN, 0);
            lv_obj_set_style_border_width(btn_gate, 2, 0);
            lv_label_set_text(label_gate_status, "OTWARTA");
            lv_obj_set_style_text_color(label_gate_status, COL_TEXT_WHITE, 0);
            if(icon) lv_obj_set_style_text_color(icon, COL_ACCENT_GREEN, 0);
            break;

        case GATE_MOVING:
            // Stan RUCH: Pomarańczowy + Miganie ramki
            lv_obj_set_style_bg_color(btn_gate, lv_color_hex(0x331A00), 0); 
            lv_obj_set_style_border_color(btn_gate, COL_ACCENT_ORANGE, 0);
            lv_obj_set_style_border_width(btn_gate, 2, 0);
            
            lv_label_set_text(label_gate_status, "RUCH...");
            lv_obj_set_style_text_color(label_gate_status, COL_TEXT_WHITE, 0);
            if(icon) lv_obj_set_style_text_color(icon, COL_ACCENT_ORANGE, 0);

            // Start animacji pulsowania
            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, btn_gate);
            lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
            lv_anim_set_exec_cb(&a, anim_border_opa_cb);
            lv_anim_set_time(&a, 600);
            lv_anim_set_playback_time(&a, 600);
            lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
            lv_anim_start(&a);
            break;

        case GATE_STOPPED:
            // Stan ZATRZYMANA: Czerwony (Bez migania)
            lv_obj_set_style_bg_color(btn_gate, lv_color_hex(0x4A0E0E), 0); // Ciemna czerwień tła
            lv_obj_set_style_border_color(btn_gate, COL_ACCENT_RED, 0);     // Jasna czerwień ramki
            lv_obj_set_style_border_width(btn_gate, 2, 0);
            
            lv_label_set_text(label_gate_status, "ZATRZYMANA");
            lv_obj_set_style_text_color(label_gate_status, COL_TEXT_WHITE, 0);
            if(icon) lv_obj_set_style_text_color(icon, COL_ACCENT_RED, 0);
            break;
    }
}

// --- SYMULACJA KLIKNIĘCIA (4 STANY) ---
static void gate_click_cb(lv_event_t * e) {
    static int current_state = 0;
    current_state++;
    if (current_state > 3) current_state = 0; 
    update_gate_state(current_state);
}

lv_obj_t* create_card_btn(lv_obj_t* parent, const char* symbol, const char* title, const char* status_text, bool is_active, int w, int h) {
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_radius(btn, 12, 0);
    lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_NONE, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0); 
    lv_obj_set_style_pad_all(btn, 14, 0); 

    if(is_active) {
        lv_obj_set_style_bg_color(btn, COL_CARD_ACTIVE, 0); 
        lv_obj_set_style_border_width(btn, 2, 0);
        lv_obj_set_style_border_color(btn, COL_ACCENT_GREEN, 0);
    } else {
        lv_obj_set_style_bg_color(btn, COL_CARD_INACTIVE, 0);
        lv_obj_set_style_border_width(btn, 0, 0); 
    }

    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lv_obj_t * top_part = lv_obj_create(btn);
    lv_obj_set_size(top_part, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(top_part, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(top_part, 0, 0);
    lv_obj_set_style_pad_all(top_part, 0, 0);
    lv_obj_set_flex_flow(top_part, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top_part, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t* icon = lv_label_create(top_part);
        lv_label_set_text(icon, symbol);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_28, 0); 
        lv_obj_set_style_text_color(icon, is_active ? COL_ACCENT_GREEN : COL_TEXT_GREY, 0);

        lv_obj_t* label = lv_label_create(top_part);
        lv_label_set_text(label, title);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0); 
        lv_obj_set_style_text_color(label, COL_TEXT_WHITE, 0); 

    lv_obj_t* status_lbl = lv_label_create(btn);
    lv_label_set_text(status_lbl, status_text);
    lv_obj_set_style_text_font(status_lbl, &lv_font_montserrat_20, 0); 
    lv_obj_set_style_text_color(status_lbl, is_active ? COL_TEXT_WHITE : COL_TEXT_GREY, 0);
    
    return btn;
}

void create_dashboard() {
    lv_obj_t * screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, COL_BG_SCREEN, 0);
    lv_obj_set_style_pad_hor(screen, 25, 0); 
    lv_obj_set_style_pad_ver(screen, 15, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    
    // --- NAGŁÓWEK ---
    lv_obj_t * header = lv_obj_create(screen);
    lv_obj_set_width(header, lv_pct(100));
    lv_obj_set_height(header, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        // --- LEWA: ZEGAR ---
        lv_obj_t * time_box = lv_obj_create(header);
        lv_obj_set_size(time_box, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(time_box, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(time_box, 0, 0);
        lv_obj_set_style_pad_all(time_box, 0, 0);
        lv_obj_set_flex_flow(time_box, LV_FLEX_FLOW_COLUMN); 
        
            label_time = lv_label_create(time_box);
            lv_label_set_text(label_time, "21:35");
            lv_obj_set_style_text_font(label_time, &font_72, 0); 
            lv_obj_set_style_text_color(label_time, COL_TEXT_WHITE, 0);
            lv_obj_set_style_pad_bottom(label_time, 0, 0); 

            label_date = lv_label_create(time_box);
            lv_label_set_text(label_date, "Sroda, 26 lis");
            lv_obj_set_style_text_font(label_date, &lv_font_montserrat_20, 0); 
            lv_obj_set_style_text_color(label_date, COL_TEXT_GREY, 0);
            lv_obj_set_style_pad_left(label_date, 5, 0);
            lv_obj_set_style_pad_top(label_date, 8, 0); 

        // --- PRAWA: POGODA (SZTYWNY ROZMIAR) ---
        lv_obj_t * weather_box = lv_obj_create(header);
        // FIX: SZEROKOŚĆ 260PX (Zostawione zgodnie z życzeniem)
        lv_obj_set_width(weather_box, 260); 
        lv_obj_set_height(weather_box, LV_SIZE_CONTENT);
        
        lv_obj_set_style_bg_opa(weather_box, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(weather_box, 0, 0);
        lv_obj_set_style_pad_all(weather_box, 0, 0);
        
        lv_obj_set_flex_flow(weather_box, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_gap(weather_box, 15, 0); 
        lv_obj_set_flex_align(weather_box, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        
            // 1. IKONA Z PLIKU (CLEAR DAY)
            lv_obj_t * icon_img = lv_img_create(weather_box);
            lv_img_set_src(icon_img, &clear_day); 
            lv_obj_set_size(icon_img, 64, 64);

            // 2. TEKSTY
            lv_obj_t * w_txt = lv_obj_create(weather_box);
            lv_obj_set_size(w_txt, LV_SIZE_CONTENT, LV_SIZE_CONTENT);;
            lv_obj_set_style_bg_opa(w_txt, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(w_txt, 0, 0);
            lv_obj_set_style_pad_all(w_txt, 0, 0);
            lv_obj_set_flex_flow(w_txt, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_flex_align(w_txt, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END); 
            
                label_temp = lv_label_create(w_txt);
                lv_label_set_text(label_temp, "24°C"); 
                lv_obj_set_style_text_font(label_temp, &font_48, 0); 
                lv_obj_set_style_text_color(label_temp, COL_TEXT_WHITE, 0);
                
                label_weather_desc = lv_label_create(w_txt);
                lv_label_set_text(label_weather_desc, "Slonecznie");
                lv_obj_set_style_text_color(label_weather_desc, COL_ACCENT_ORANGE, 0); 
                lv_obj_set_style_text_font(label_weather_desc, &lv_font_montserrat_16, 0);

    // --- SIATKA KAFELKÓW ---
    lv_obj_t * grid = lv_obj_create(screen);
    lv_obj_set_size(grid, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(grid, 1); 
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, 0, 0);
    
    // OBNIŻENIE SEKCJI PRZYCISKÓW (25px)
    lv_obj_set_style_pad_top(grid, 25, 0);
    
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_gap(grid, 12, 12); 

    int btn_w = 240; 
    int btn_h = 145; 

    btn_gate = create_card_btn(grid, LV_SYMBOL_HOME, "Brama", "ZAMKNIETA", false, btn_w, btn_h);
    label_gate_status = lv_obj_get_child(btn_gate, 1); 
    lv_obj_add_event_cb(btn_gate, gate_click_cb, LV_EVENT_CLICKED, NULL);

    create_card_btn(grid, LV_SYMBOL_BELL, "Furtka", "ZAMKNIETA", false, btn_w, btn_h);
    btn_garage = create_card_btn(grid, LV_SYMBOL_DRIVE, "Garaz", "ZAMKNIETY", false, btn_w, btn_h);
    label_garage_status = lv_obj_get_child(btn_garage, 1);
    lv_obj_add_event_cb(btn_garage, gate_click_cb, LV_EVENT_CLICKED, NULL);

    create_card_btn(grid, LV_SYMBOL_VOLUME_MAX, "Salon", "WLACZONE", true, btn_w, btn_h);
    create_card_btn(grid, LV_SYMBOL_UP, "Rolety", "OTWARTE", false, btn_w, btn_h);
    create_card_btn(grid, LV_SYMBOL_BATTERY_2, "Ogrod", "WYLACZONE", false, btn_w, btn_h);

    lv_timer_create(clock_timer_cb, 1000, NULL);
}

void setup()
{
    Serial.begin(115200);
    delay(100);
    pinMode(GPIO_INPUT_IO_4, OUTPUT);
    expander = new ESP_IOExpander_CH422G((i2c_port_t)I2C_MASTER_NUM, ESP_IO_EXPANDER_I2C_CH422G_ADDRESS, I2C_MASTER_SCL_IO, I2C_MASTER_SDA_IO);
    expander->init();
    expander->begin();
    expander->enableAllIO_Output();
    expander->digitalWrite(TP_RST , HIGH);
    expander->digitalWrite(LCD_RST , HIGH);
    expander->digitalWrite(LCD_BL , HIGH);
    delay(100);
    expander->digitalWrite(TP_RST , LOW);
    delay(100);
    digitalWrite(GPIO_INPUT_IO_4, LOW);
    delay(100);
    expander->digitalWrite(TP_RST , HIGH);
    delay(200);

    ESP_Panel *panel = new ESP_Panel();
    panel->init();
#if LVGL_PORT_AVOID_TEAR
    ESP_PanelBus_RGB *rgb_bus = static_cast<ESP_PanelBus_RGB *>(panel->getLcd()->getBus());
    rgb_bus->configRgbFrameBufferNumber(LVGL_PORT_DISP_BUFFER_NUM);
    rgb_bus->configRgbBounceBufferSize(LVGL_PORT_RGB_BOUNCE_BUFFER_SIZE);
#endif
    panel->begin();
    lvgl_port_init(panel->getLcd(), panel->getTouch());

    lvgl_port_lock(-1);
    screensaver_btn = lv_btn_create(lv_layer_top());
    lv_obj_set_size(screensaver_btn, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(screensaver_btn, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(screensaver_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(screensaver_btn, screensaver_event_cb, LV_EVENT_CLICKED, NULL);
    lv_timer_create(inactivity_check_cb, 1000, NULL);
    create_dashboard();
    lvgl_port_unlock();
}

void loop()
{
    vTaskDelay(pdMS_TO_TICKS(1000));
}