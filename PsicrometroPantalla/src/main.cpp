#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#include "ui.h" 
#include <Wire.h>
#include "RTClib.h"
#include <FS.h>
#include <SD.h>
#include <WiFi.h>
#include <esp_now.h>

#define SD_CS 5 

RTC_DS3231 rtc;
TFT_eSPI tft = TFT_eSPI();

static const uint16_t screenWidth  = 320;
static const uint16_t screenHeight = 240;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[ screenWidth * 10 ];

void my_disp_flush( lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p ) {
    uint32_t w = ( area->x2 - area->x1 + 1 );
    uint32_t h = ( area->y2 - area->y1 + 1 );
    tft.startWrite();
    tft.setAddrWindow( area->x1, area->y1, w, h );
    tft.pushColors( ( uint16_t * )&color_p->full, w * h, true );
    tft.endWrite();
    lv_disp_flush_ready( disp_drv );
}

// =========================================================================
//                               ESP - NOW
// =========================================================================
typedef struct struct_message {
  uint8_t tipo_mensaje; 
  float tbs;            
  float tbh;            
  float t_dht;          
  float hr_dht;         
  float hr_calc;        
  float pv;             
  float pvs;            
  float dpv;            
  float w;              
  float ws;             
  float mu;             
  float v_eh;           
  float h;              
  float t_pr;           
} struct_message;

struct_message datos_pantalla; 
struct_message datos_sd;       
bool guardar_en_sd = false;    

void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
    struct_message paquete_entrante;
    memcpy(&paquete_entrante, incomingData, sizeof(paquete_entrante));

    if (paquete_entrante.tipo_mensaje == 0) {
        datos_pantalla = paquete_entrante; 
    } 
    else if (paquete_entrante.tipo_mensaje == 1) {
        datos_sd = paquete_entrante;       
        guardar_en_sd = true; 
        Serial.println("¡Llegó el paquete pesado de 10 mins! Guardando...");
    }
}

void SDCardInit(){
    delay(500);
    if(!SD.begin(SD_CS, SPI, 4000000)) {
        Serial.println("SD no detectada.");
        return;
    }
    Serial.println("SD conectada.");
}

void setup() {
    Serial.begin(115200);

    pinMode(21, OUTPUT); digitalWrite(21, HIGH);
    pinMode(27, OUTPUT); digitalWrite(27, HIGH);

    tft.begin();
    tft.setRotation(1);
    tft.invertDisplay(false); 

    lv_init();
    lv_disp_draw_buf_init( &draw_buf, buf, NULL, screenWidth * 10 );
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init( &disp_drv );
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register( &disp_drv );

    Wire.begin(22, 21);

    if (!rtc.begin()) {
        Serial.println("No se detecta el módulo RTC");
    } else {
        Serial.println("Reloj RTC funcionando");
        if (rtc.lostPower()) rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        
        DateTime ahora = rtc.now();
        struct timeval tv;
        tv.tv_sec = ahora.unixtime(); 
        tv.tv_usec = 0;
        settimeofday(&tv, NULL);      
    }

    SDCardInit();
    File archivo = SD.open("/datos_psicrometro.txt", FILE_APPEND);
    if (archivo) {
        archivo.println("\n--- INICIO DE NUEVO REGISTRO PSICROMÉTRICO ---");
        archivo.close();
    }

    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error iniciando ESP-NOW");
    } else {
        esp_now_register_recv_cb(OnDataRecv);
        Serial.println(" ESP-NOW listo. Escuchando toda la psicrometría...");
    }

    ui_init(); 
}

bool ya_cambio = false;

void loop() {
    lv_timer_handler(); 

    // --- 1. PANTALLA  ---
    static uint32_t ultimo_reloj = 0;
    if (millis() - ultimo_reloj > 1000) {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            char texto_fecha[16];
            char texto_hora[16];
            snprintf(texto_fecha, sizeof(texto_fecha), "%04d/%02d/%02d", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
            snprintf(texto_hora, sizeof(texto_hora), "%02d:%02d hrs", timeinfo.tm_hour, timeinfo.tm_min);
            
            if (ya_cambio) {
                lv_label_set_text(ui_uiLabelFecha, texto_fecha); 
                lv_label_set_text(ui_uiLabelHora, texto_hora); 
                
                char txt_tbs[16], txt_tbh[16], txt_ref[16], txt_hum[16];
                snprintf(txt_tbs, sizeof(txt_tbs), "%.1f", datos_pantalla.tbs);
                snprintf(txt_tbh, sizeof(txt_tbh), "%.1f", datos_pantalla.tbh);
                snprintf(txt_ref, sizeof(txt_ref), "%.1f", datos_pantalla.t_dht);
                snprintf(txt_hum, sizeof(txt_hum), "%.1f", datos_pantalla.hr_dht);

                lv_label_set_text(ui_uiLabelTbs, txt_tbs);
                lv_label_set_text(ui_uiLabelTbh, txt_tbh);
                lv_label_set_text(ui_uiLabelRef, txt_ref);
                lv_label_set_text(ui_uiLabelHum, txt_hum); 

                lv_arc_set_value(ui_GrafHum, (int)datos_pantalla.hr_dht);
            }
        }
        ultimo_reloj = millis();
    }

    // --- 2. GUARDADO EN SD  ---
    if (guardar_en_sd) {
        struct tm timeinfo;
        getLocalTime(&timeinfo);
        
        char registro_sd[256]; // Buffer grande para que quepa todo
        // Formato final con todas tus propiedades de la tarea
        snprintf(registro_sd, sizeof(registro_sd), 
            "[%04d:%02d:%02d %02d:%02d:%02d, Tbs:%.2f, Tbh:%.2f, Pvs:%.4f, Pv:%.4f, DPVa:%.4f, W:%.5f, Ws:%.5f, mu:%.2f, Veh:%.5f, h:%.2f, Tpr:%.2f]",
            timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
            timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
            datos_sd.tbs, datos_sd.tbh, datos_sd.pvs, datos_sd.pv, 
            datos_sd.dpv, datos_sd.w, datos_sd.ws, datos_sd.mu, 
            datos_sd.v_eh, datos_sd.h, datos_sd.t_pr);

        File archivo = SD.open("/datos_psicrometro.txt", FILE_APPEND);
        if (archivo) {
            archivo.println(registro_sd);
            archivo.close();
            Serial.println(registro_sd);
            Serial.println("¡Variables ASHRAE guardadas en SD!");
        } else {
            Serial.println("Error al escribir en la SD.");
        }
        
        guardar_en_sd = false; 
    }

    // --- 3. TRANSICIÓN A SCREEN 2 ---
    if (millis() > 12000 && !ya_cambio) {
        lv_scr_load_anim(ui_Screen2, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, false);
        ya_cambio = true; 
    }

    delay(5);
}