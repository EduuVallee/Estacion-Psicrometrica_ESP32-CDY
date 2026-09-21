[readme_del_proyecto.md](https://github.com/user-attachments/files/32452085/readme_del_proyecto.md)


# Estación Psicrométrica Inalámbrica con ESP32 y Pantalla CYD

![ESP32](https://img.shields.io/badge/Device-ESP32-orange)
![LVGL](https://img.shields.io/badge/GUI-LVGL_8.3-blue)
![ESP-NOW](https://img.shields.io/badge/Protocol-ESP_NOW-green)
![Status](https://img.shields.io/badge/Status-Completed-success)

##  Descripción del Proyecto
Este repositorio contiene el código fuente y los archivos de interfaz gráfica de una **Estación Psicrométrica Inalámbrica de Doble Núcleo**. El sistema mide variables termodinámicas en tiempo real y calcula todas las propiedades psicrométricas del aire húmedo utilizando los **polinomios exactos de ASHRAE**, ajustados a la presión atmosférica local.

El proyecto está diseñado con una arquitectura distribuida en dos nodos que se comunican sin WiFi tradicional, utilizando el protocolo ultrarrápido **ESP-NOW**:

1. **Nodo Emisor (Sensores):** Un ESP32 estándar que lee las temperaturas de Bulbo Seco y Bulbo Húmedo (Termistores NTC) y humedad de un DHT11. Utiliza *FreeRTOS* para realizar el procesamiento matemático pesado en segundo plano y transmitir los paquetes de datos.
2. **Nodo Receptor (Datalogger y UI):** Una placa CYD (*Cheap Yellow Display* - ESP32-2432S028R) que renderiza la interfaz gráfica programada en *SquareLine Studio* (LVGL). Muestra la información de forma fluida y guarda un registro histórico (promedios de 10 minutos) en una tarjeta microSD, etiquetado con un reloj RTC de alta precisión.

##  Variables Calculadas y Registradas
El sistema calcula y almacena en la SD las siguientes propiedades psicrométricas:
* **$T_{bs}$ y $T_{bh}$:** Temperaturas de Bulbo Seco y Húmedo (°C)
* **$P_{vs}$ y $P_v$:** Presión de Saturación y Presión Real de Vapor (kPa)
* **$W$ y $W_s$:** Razón de Humedad Real y de Saturación (kg/kg)
* **$\mu$:** Grado de Saturación (%)
* **$v_{eh}$:** Volumen Específico del Aire Húmedo (m³/kg)
* **$h$:** Entalpía (kJ/kg)
* **$T_{pr}$:** Temperatura de Punto de Rocío (°C)

##  Hardware Utilizado
* Microcontrolador Principal: ESP32 (WROOM-32)
* Interfaz HMI: Pantalla CYD (ESP32-2432S028R) 2.8" TFT Touch
* Sensores NTC: 2x Termistor de 10kΩ
* Sensor de Referencia: DHT11
* Reloj en Tiempo Real: Módulo RTC DS3231 (I2C)
* Almacenamiento: Lector MicroSD (Integrado vía SPI)

##  Dependencias y Librerías
Para compilar la interfaz visual (`/src`) en Visual Studio Code o Arduino IDE, asegúrate de tener instaladas las siguientes librerías:
* `TFT_eSPI` (Configurada específicamente para los pines de la CYD)
* `lvgl` (Versión 8.3.x estricta)
* `RTClib` (Adafruit)
* `DHT sensor library`

##  Desarrollo
Diseñado e implementado para el análisis termodinámico y recolección de datos ambientales. Interfaz gráfica exportada desde **SquareLine Studio**.
