#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <DHT.h>
#include <math.h>

// --- PINES DE LOS SENSORES ---
#define PIN_NTC_TBS 35   // NTC Bulbo Seco (Pin 35)
#define PIN_NTC_TBH 34   // NTC Bulbo Húmedo (Pin 34)
#define DHTPIN 23        // DHT11 (Pin 23)
#define DHTTYPE DHT11 
DHT dht(DHTPIN, DHTTYPE);

// MAC Address del ESP32 Receptor (Pantalla / SD) 
// 10:06:1C:83:A0:A0
uint8_t broadcastAddress[] = {0x10, 0x06, 0x1C, 0x83, 0xA0, 0xA0};

// Presión atmosférica en Texcoco (~2250-2557 msnm) [kPa]
const float P_ATM = 77; 

// =========================================================================
// ESTRUCTURA GEMELA (Idéntica a la de la CYD)
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

struct_message datos;
TaskHandle_t TareaSensores;
float suma_tbs = 0, suma_tbh = 0, suma_tdht = 0, suma_hrdht = 0;
int contador = 0;

// =========================================================================
// FUNCIÓN NTC CALIBRADA CON MILIVOLTIOS (NTC a VCC, Res a GND)
// =========================================================================
// =====================================
// COEFICIENTES DE CALIBRACION (Regresión Vale)
// =====================================
// NTC2 (Pin 35 -> TBS)
const float M_TBS = 1.1593;
const float B_TBS = 1.41;

// NTC1 (Pin 34 -> TBH)
const float M_TBH = 1.1119;
const float B_TBH = 2.6098;

// =========================================================================
// LECTURA ESTABILIZADA Y CALIBRADA (PULL-DOWN)
// =========================================================================
int leerADC_Promedio(int pin) {
  long suma = 0;
  for (int i = 0; i < 10; i++) {
    suma += analogRead(pin);
    vTaskDelay(5 / portTICK_PERIOD_MS); // Usamos delay de FreeRTOS
  }
  return suma / 10;
}

float obtenerTempNTC(int pin) {
  int adc = leerADC_Promedio(pin);
  if (adc <= 0 || adc >= 4095) return 0.0;

  // 1. Resistencia PULL-DOWN
  float resistencia = 10000.0 * (4095.0 - adc) / (float)adc;

  // 2. Modelo Beta
  float temperaturaK = 1.0 / ((1.0 / 298.15) + (1.0 / 3950.0) * log(resistencia / 10000.0));
  float tempRaw = temperaturaK - 273.15;

  // 3. Aplicar los coeficientes de Vale según el sensor
  if (pin == PIN_NTC_TBS) {
    return (M_TBS * tempRaw) + B_TBS;
  } else if (pin == PIN_NTC_TBH) {
    return (M_TBH * tempRaw) + B_TBH;
  }
  
  return tempRaw;
}

// =========================================================================
// CÁLCULOS PSICROMÉTRICOS (COMPLETOS CON ASHRAE)
// =========================================================================
float calculaPvs(float T){
  return 0.61078 * exp((17.27 * T) / (T + 237.3));
}

void calcularPsicrometria(float tbs, float tbh, struct_message &msg) {
  // 1. Presiones de vapor
  msg.pvs = calculaPvs(tbs); 
  float pvs_tbh = calculaPvs(tbh);
  msg.pv = pvs_tbh - (0.00066 * P_ATM * (tbs - tbh)); 
  
  // Déficit de Presión de Vapor (kPa)
  msg.dpv = msg.pvs - msg.pv;
  if (msg.dpv < 0) msg.dpv = 0.0; // Evitar valores negativos irreales
  
  // 2. Humedad Relativa (%)
  float hr = (msg.pv / msg.pvs) * 100.0;
  msg.hr_calc = constrain(hr, 0.0, 100.0);
  
  // 3. Relaciones de Humedad (kg agua / kg aire seco)
  msg.w = 0.62198 * (msg.pv / (P_ATM - msg.pv)); 
  msg.ws = 0.62198 * (msg.pvs / (P_ATM - msg.pvs));
  
  // 4. Grado de Saturación (adimensional)
  msg.mu = msg.w / msg.ws; 

  // 5. Volumen Específico (m³/kg) - Constante de gas R_a = 0.287042
  msg.v_eh = (0.287042 * (tbs + 273.15) * (1.0 + 1.6078 * msg.w)) / P_ATM;

  // 6. Entalpía (kJ/kg)
  msg.h = 1.006 * tbs + msg.w * (2501.0 + 1.86 * tbs);

  // 7. Temperatura de Punto de Rocío (°C)
  // Se calcula con la inversa de la fórmula de Magnus
  if (msg.pv > 0) {
    float alpha = log(msg.pv / 0.61078);
    msg.t_pr = (237.3 * alpha) / (17.27 - alpha);
  } else {
    msg.t_pr = tbs; // Si la pv es 0 o menor (error de lectura), el rocío cae a la tbs
  }
}

// =========================================================================
// TAREA PRINCIPAL EN FREERTOS (Cada 2 segundos)
// =========================================================================
void leerSensores(void * parameter) {
  for(;;) {
    // 1. Lectura directa pasándole el pin
    float tbs_vivo = obtenerTempNTC(PIN_NTC_TBS);
    float tbh_vivo = obtenerTempNTC(PIN_NTC_TBH);
    
    // Lectura del DHT11
    float t_dht = dht.readTemperature();
    float h_dht = dht.readHumidity();
    if (isnan(t_dht)) t_dht = 0.0;
    if (isnan(h_dht)) h_dht = 0.0;

    // Si la lectura del NTC falla (retorna -999.0), asignamos 0.0 para evitar desbordamiento
    if (tbs_vivo == -999.0) tbs_vivo = 0.0;
    if (tbh_vivo == -999.0) tbh_vivo = 0.0;

    // 2. Enviar datos instantáneos para PANTALLA (tipo_mensaje = 0)
    datos.tipo_mensaje = 0;
    datos.tbs = tbs_vivo;
    datos.tbh = tbh_vivo;
    datos.t_dht = t_dht;
    datos.hr_dht = h_dht;
    calcularPsicrometria(tbs_vivo, tbh_vivo, datos);
    esp_now_send(broadcastAddress, (uint8_t *) &datos, sizeof(datos));

    // 3. Acumular para el promedio
    suma_tbs += tbs_vivo;
    suma_tbh += tbh_vivo;
    suma_tdht += t_dht;
    suma_hrdht += h_dht;
    contador++;

    // 4. Enviar promedio de 10 minutos a la MEMORIA SD (tipo_mensaje = 1)
    if (contador >= 300) { // 300 lecturas x 2 seg = 600 s = 10 min
      struct_message promedio;
      promedio.tipo_mensaje = 1;
      promedio.tbs = suma_tbs / 300.0;
      promedio.tbh = suma_tbh / 300.0;
      promedio.t_dht = suma_tdht / 300.0;
      promedio.hr_dht = suma_hrdht / 300.0;
      
      calcularPsicrometria(promedio.tbs, promedio.tbh, promedio);
      esp_now_send(broadcastAddress, (uint8_t *) &promedio, sizeof(promedio));

      // Reiniciar acumuladores
      suma_tbs = 0; suma_tbh = 0; suma_tdht = 0; suma_hrdht = 0;
      contador = 0;
    }

    vTaskDelay(2000 / portTICK_PERIOD_MS); 
  }
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  
  // Resolución del ADC del ESP32 a 12 bits (0 - 4095)
  analogReadResolution(12);
  
  WiFi.mode(WIFI_STA); 
  esp_now_init();
  
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);

  // Crear tarea dedicada a la lectura de sensores
  xTaskCreatePinnedToCore(leerSensores, "Sensores", 4096, NULL, 1, &TareaSensores, 1);
}

void loop() { 
  vTaskDelete(NULL); // Libera memoria del loop habitual
}