[readme_del_proyecto.md](https://github.com/user-attachments/files/32452085/readme_del_proyecto.md)

# 🌦️ Estación Psicrométrica Digital Inalámbrica (ESP32 + CYD)

**Automatización de Biosistemas | Ingeniería Mecatrónica Agrícola**

Este repositorio contiene el código fuente y la documentación para una **Estación Psicrométrica de alta precisión** basada en una arquitectura IoT de doble núcleo. El sistema captura, calcula, transmite y almacena variables termodinámicas del aire utilizando el modelo matemático de ASHRAE.

---

## ⚙️ Arquitectura del Sistema

El proyecto opera bajo un modelo de comunicación inalámbrica de baja latencia utilizando el protocolo **ESP-NOW**, dividiendo el trabajo en dos microcontroladores para optimizar el rendimiento y evitar bloqueos en la interfaz gráfica.

### 📡 1. Nodo Emisor (Adquisición y Cálculo)
* **Hardware:** ESP32 (Dev Board) + 2x NTC 10K (B3950) + DHT11.
* **Función:** Lectura de sensores en tiempo real (cada 2 segundos) mediante un circuito **Pull-Down** (NTC a 3.3V, R fija a GND). 
* **Procesamiento:** Implementa la **Ecuación de Steinhart-Hart** con un ajuste lineal de calibración a dos puntos y ejecuta algoritmos psicrométricos de la ASHRAE para deducir propiedades físicas del aire a partir de la temperatura de bulbo seco ($T_{bs}$) y bulbo húmedo ($T_{bh}$).
* **Transmisión:** Empaqueta 15 variables en una estructura y las envía al nodo receptor.

### 🖥️ 2. Nodo Receptor (Interfaz y Datalogger)
* **Hardware:** Pantalla táctil CYD (Cheap Yellow Display - ESP32) + Módulo RTC DS3231 + Tarjeta microSD.
* **Función:** Recibe telemetría vía ESP-NOW y dibuja gráficas fluidas utilizando la librería gráfica **LVGL**.
* **Almacenamiento:** Cada 10 minutos promedia los datos acumulados y escribe un archivo `.csv` en la microSD, inyectando una marca de tiempo exacta proporcionada por el reloj RTC para facilitar el análisis posterior en Excel o Python.

---

## 📊 Variables Psicrométricas Calculadas (Modelo ASHRAE)

El firmware del nodo emisor está programado para calcular las siguientes propiedades bajo una presión atmosférica local (ej. 77 kPa para 2250 msnm):

- Presión de Saturación de Vapor ($P_{vs}$)
- Presión Real de Vapor ($P_v$)
- Déficit de Presión de Vapor ($DPV_a$)
- Razón de Humedad ($W$ y $W_s$)
- Humedad Relativa calculada matemáticamente ($\mu$)
- Volumen Específico del aire húmedo ($V_{eh}$)
- Entalpía ($h$)
- Temperatura de Punto de Rocío ($T_{pr}$)

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

Los registros almacenados en la tarjeta microSD tienen un formato limpio separado por comas, listo para análisis científico y comparación contra el sensor de referencia (DHT11).

```csv
Fecha,Hora,Tbs,Tbh,Pvs,Pv,DPVa,W,Ws,HR_Calc,Veh,Entalpia,Tpr,Temp_DHT11,HR_DHT11
2026/10/04,14:30:00,22.15,18.50,2.66,2.01,0.65,0.012,0.015,75.50,0.92,54.20,17.50,22.30,76.00
