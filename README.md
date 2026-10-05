# 🌦️ Estación Psicrométrica Digital Inalámbrica (ESP32 + CYD)

<p align="center">
  <img src="https://img.shields.io/badge/Hardware-ESP32%20%7C%20CYD-ff69b4?style=for-the-badge&logo=espressif" alt="Hardware">
  <img src="https://img.shields.io/badge/Lenguaje-C%2B%2B-00599C?style=for-the-badge&logo=c%2B%2B" alt="C++">
  <img src="https://img.shields.io/badge/Entorno-PlatformIO-orange?style=for-the-badge&logo=platformio" alt="PlatformIO">
  <img src="https://img.shields.io/badge/OS-FreeRTOS-222222?style=for-the-badge&logo=rtos" alt="FreeRTOS">
  <br>
  <img src="https://img.shields.io/badge/GUI-LVGL%208.3-1769ff?style=for-the-badge" alt="LVGL">
  <img src="https://img.shields.io/badge/Protocolo-ESP--NOW-00b4d8?style=for-the-badge" alt="ESP-NOW">
  <img src="https://img.shields.io/badge/Memoria-MicroSD%20(CSV)-10a37f?style=for-the-badge" alt="MicroSD">
  <img src="https://img.shields.io/badge/Status-Completado-success?style=for-the-badge" alt="Status">
</p>

**Automatización de Biosistemas | Ingeniería Mecatrónica Agrícola**

Este repositorio contiene el código fuente (para ambos nodos) y la documentación técnica de una **Estación Psicrométrica de alta precisión**. El sistema captura, calcula, transmite y almacena variables termodinámicas del aire utilizando el modelo matemático de ASHRAE y una arquitectura IoT de doble núcleo.

---

## ⚙️ Arquitectura del Sistema

El proyecto opera bajo un modelo de comunicación inalámbrica de baja latencia utilizando el protocolo **ESP-NOW**. Todo el software está desarrollado en **C++** sobre el framework de Arduino, utilizando tareas en paralelo mediante **FreeRTOS** para evitar cuellos de botella en la memoria y en la interfaz visual.

### 📡 1. Nodo Emisor (Adquisición y Cálculo)
* **Hardware:** ESP32 (Dev Board) + 2x NTC 10K (B3435) sumergibles + DHT11.
* **Función:** Tarea fijada al núcleo 1 de la ESP32 para lectura de sensores cada 2 segundos mediante un circuito **Pull-Down** (NTC a 3.3V, R fija a GND). 
* **Procesamiento:** Implementa la **Ecuación de Steinhart-Hart** con un ajuste lineal de calibración a dos puntos y ejecuta algoritmos psicrométricos de la ASHRAE para deducir propiedades físicas del aire.
* **Transmisión:** Empaqueta 15 variables en un `struct` sincronizado y las envía al nodo receptor.

### 🖥️ 2. Nodo Receptor (Interfaz y Datalogger)
* **Hardware:** Pantalla táctil CYD (Cheap Yellow Display - ESP32) + Módulo RTC DS3231 + Tarjeta microSD.
* **Función:** Recibe telemetría vía ESP-NOW y dibuja gráficas fluidas utilizando la librería gráfica **LVGL** y UI generada.
* **Almacenamiento:** Cada 10 minutos recibe el paquete de promedios, inyecta una marca de tiempo exacta (YYYY/MM/DD HH:MM:SS) mediante el RTC, y escribe el historial en un datalogger `.csv` directo a la microSD.

---

## 📊 Variables Psicrométricas Calculadas (Modelo ASHRAE)

El firmware del nodo emisor está programado para calcular las siguientes propiedades bajo la presión atmosférica local de Texcoco (77 kPa a 2250 msnm):

- Presión de Saturación de Vapor (Pvs)
- Presión Real de Vapor (Pv)
- Déficit de Presión de Vapor (DPVa)
- Razón de Humedad (W y Ws)
- Humedad Relativa calculada matemáticamente (μ)
- Volumen Específico del aire húmedo (Veh)
- Entalpía (h)
- Temperatura de Punto de Rocío (Tpr)

---

## 🔌 Esquema de Conexiones (Pinout)

### Nodo 1: Sensores (ESP32)
| Componente | Pin ESP32 | Configuración |
| :--- | :--- | :--- |
| **NTC Bulbo Seco** | `GPIO 35` (ADC) | Divisor de tensión Pull-Down (10kΩ a GND) |
| **NTC Bulbo Húmedo**| `GPIO 34` (ADC) | Divisor de tensión Pull-Down (10kΩ a GND) |
| **DHT11 (Referencia)**| `GPIO 23` | Señal de datos (Pull-Up interno o 10kΩ a VCC) |

### Nodo 2: Interfaz (Pantalla CYD)
| Componente | Pin CYD | Función |
| :--- | :--- | :--- |
| **RTC DS3231 (SDA)**| `GPIO 22` | Comunicación I2C |
| **RTC DS3231 (SCL)**| `GPIO 21` | Comunicación I2C |
| **Módulo MicroSD** | `Integrado` | SPI a 4MHz (Pin CS: `GPIO 5`) |

---

## 💾 Formato de Salida (Datalogger CSV)

Los registros de la tarjeta microSD generan una tabla `.csv` lista para su análisis y graficación en Excel o Python. Incluye validación contra el sensor de referencia (DHT11).

```csv
Fecha,Hora,Tbs,Tbh,Pvs,Pv,DPVa,W,Ws,HR_Calc,Veh,Entalpia,Tpr,Temp_DHT11,HR_DHT11
2026/10/04,14:30:00,22.15,18.50,2.66,2.01,0.65,0.012,0.015,75.50,0.92,54.20,17.50,22.30,76.00
