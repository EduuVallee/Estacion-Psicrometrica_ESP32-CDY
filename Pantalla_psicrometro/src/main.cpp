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

// Resolución de tu CYD
static const uint16_t screenWidth  = 320;
static const uint16_t screenHeight = 240;
TFT_eSPI tft = TFT_eSPI();

// Buffer de memoria para LVGL
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[ screenWidth * screenHeight / 10 ];

// --- VARIABLES GLOBALES PARA LA GRÁFICA ---
lv_chart_series_t * serie_dht;
lv_chart_series_t * serie_bulbo;
// lv_img_dsc_t foto_corregida;

// =========================================================================
//                             RELOJ Y SD
// =========================================================================
RTC_DS3231 rtc;


SPIClass sdSPI(HSPI); // Creamos un bus SPI exclusivo para la SD y sirva el touch

void SDCardInit(){
    // Pines de la SD en la CYD: SCK=18, MISO=19, MOSI=23, CS=5
    sdSPI.begin(18, 19, 23, SD_CS); 
    delay(500);
    
    if(!SD.begin(SD_CS, sdSPI, 2000000)) {
        Serial.println("SD no detectada.");
        return;
    }
    Serial.println("SD conectada correctamente.");
}

// =========================================================================
//                             ESP - NOW
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

// =========================================================================
//                           PANTALLA Y TOUCH
// =========================================================================
void my_disp_flush( lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p ) {
    uint32_t w = ( area->x2 - area->x1 + 1 );
    uint32_t h = ( area->y2 - area->y1 + 1 );
    tft.startWrite();
    tft.setAddrWindow( area->x1, area->y1, w, h );
    tft.pushColors( ( uint16_t * )&color_p->full, w * h, false );
    tft.endWrite();
    lv_disp_flush_ready( disp_drv );
}

void my_touchpad_read( lv_indev_drv_t * indev_drv, lv_indev_data_t * data ) {
    uint16_t touchX = 0, touchY = 0;
    bool touched = tft.getTouch( &touchX, &touchY );
    if( !touched ) {
        data->state = LV_INDEV_STATE_REL;
    } else {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = touchX;
        data->point.y = touchY;
    }
}

// =========================================================================
//                               SETUP
// =========================================================================
void setup() {
    Serial.begin( 115200 );

    // 1. Iniciar ESP-NOW PRIMERO (Para asegurar RAM del Wi-Fi)
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error iniciando ESP-NOW");
    } else {
        esp_now_register_recv_cb(OnDataRecv);
        Serial.println("ESP-NOW listo. Escuchando toda la psicrometría...");
    }

    // 2. Iniciar Reloj RTC 
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

    // 3. Iniciar SD (Antes de que LVGL se acabe la RAM)
    SDCardInit();
    File archivo = SD.open("/datos_psicrometro.csv", FILE_APPEND);
    if (archivo) {
        archivo.println("Fecha,Hora,Tbs,Tbh,Pvs,Pv,DPVa,W,Ws,HR_Calc,Veh,Entalpia,Tpr,Temp_DHT11,HR_DHT11");
        archivo.close();
    }

    // 4. Iniciar Pantalla y Touch
    pinMode(27, OUTPUT); digitalWrite(27, HIGH);
    tft.init();
    tft.setRotation(1); 
    tft.invertDisplay(true); 

    uint16_t calData[5] = { 185, 3538, 228, 3677, 0 };
    tft.setTouch(calData);

    // 5. Iniciar LVGL y UI
    lv_init();
    lv_disp_draw_buf_init( &draw_buf, buf, NULL, screenWidth * screenHeight / 10 );

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init( &disp_drv );
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register( &disp_drv );

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init( &indev_drv );
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register( &indev_drv );

    ui_init();

    // Configuración de las gráficas
    serie_dht = lv_chart_add_series(ui_Chart2, lv_color_hex(0xFF0000), LV_CHART_AXIS_PRIMARY_Y); 
    serie_bulbo = lv_chart_add_series(ui_Chart2, lv_color_hex(0x0088FF), LV_CHART_AXIS_PRIMARY_Y);

    Serial.println( "Todo el sistema listo." );
}

// =========================================================================
//                                LOOP
// =========================================================================
void loop() {
    lv_timer_handler(); 
    delay(5);

    // --- 1. ACTUALIZAR TEXTOS Y RELOJ (CADA 1 SEGUNDO) ---
    static uint32_t ultimo_reloj = 0;
    if (millis() - ultimo_reloj > 1000) {
        ultimo_reloj = millis();
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            char texto_fecha[16];
            char texto_hora[16];
            snprintf(texto_fecha, sizeof(texto_fecha), "%04d/%02d/%02d", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
            snprintf(texto_hora, sizeof(texto_hora), "%02d:%02d hrs", timeinfo.tm_hour, timeinfo.tm_min);
            
            // Revisa si estamos en la Screen 2 para actualizar textos
            if (lv_scr_act() == ui_Screen2) {
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

            }
        }
    }

    // --- 2. ACTUALIZAR GRÁFICA (CADA 1 SEGUNDO PARA VER EL AVANCE RÁPIDO) ---
    static uint32_t ultimo_grafica = 0;
    if(millis() - ultimo_grafica > 1000) { 
        ultimo_grafica = millis();
        
        // Solo inyectar a la gráfica si la pantalla activa es donde está la gráfica
        if (lv_scr_act() == ui_Screen2) {
            lv_chart_set_next_value(ui_Chart2, serie_dht, datos_pantalla.t_dht);
            lv_chart_set_next_value(ui_Chart2, serie_bulbo, datos_pantalla.tbs);
        }
    }

    // --- 3. GUARDADO EN SD (CUANDO LLEGA EL MENSAJE 1) ---
    if (guardar_en_sd) {
        struct tm timeinfo;
        getLocalTime(&timeinfo);
        char registro_sd[300]; 
        
        snprintf(registro_sd, sizeof(registro_sd), 
            "%04d/%02d/%02d,%02d:%02d:%02d,%.2f,%.2f,%.4f,%.4f,%.4f,%.5f,%.5f,%.2f,%.5f,%.2f,%.2f,%.2f,%.2f",
            timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
            timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
            datos_sd.tbs, datos_sd.tbh, datos_sd.pvs, datos_sd.pv, 
            datos_sd.dpv, datos_sd.w, datos_sd.ws, datos_sd.mu, 
            datos_sd.v_eh, datos_sd.h, datos_sd.t_pr,
            datos_sd.t_dht, datos_sd.hr_dht);

        File archivo = SD.open("/datos_psicrometro.csv", FILE_APPEND);
        if (archivo) {
            archivo.println(registro_sd);
            archivo.close();
            Serial.println(registro_sd);
            Serial.println("¡Línea CSV guardada en la memoria SD!");
        } else {
            Serial.println("Error al escribir en la SD.");
        }
        guardar_en_sd = false; 
    }
}
