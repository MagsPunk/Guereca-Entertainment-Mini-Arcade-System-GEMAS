# Pinout e interfaces

Los números de las tablas identifican **GPIO del ESP32‑S3**. La columna «Etiqueta en el esquemático» permite localizar la señal aunque su nombre difiera de la función que tiene en el firmware.

## Controles

| Función en G.E.M.A.S | Referencia | GPIO | Etiqueta en el esquemático |
|---|---|---:|---|
| Arriba | S2 | 6 | `BTN_UP` |
| Abajo | S7 | 7 | `BTN_DOWN` |
| Izquierda | S8 | 5 | `BTN_RIGHT` |
| Derecha | S3 | 4 | `BTN_LEFT` |
| A | S5 | 42 | `BTN_X` |
| B | S10 | 47 | `BTN_Y` |
| Start | S11 | 9 | `BTN_SELECT` |
| Select | S6 | 8 | `BTN_START` |
| Encendido lógico | S1 | 15 | `POWER_SWITCH` |

Los ocho controles de juego son activos en bajo y usan resistencias externas de *pull-up* de 10 kΩ. El firmware de demostración mantiene deshabilitados S4 y S9, asociados a GPIO1 y GPIO2.

## Pantalla y microSD

| Señal | GPIO | Uso |
|---|---:|---|
| `SPI_SCK` | 12 | Reloj compartido entre la TFT y la microSD. |
| `SPI_MOSI` | 11 | Datos del ESP32‑S3 hacia los periféricos. |
| `SPI_MISO` | 13 | Retorno de datos previsto en la PCB. El firmware de la TFT deja MISO sin configurar. |
| `LCD_CS` | 10 | Selección de la pantalla. |
| `LCD_DC` | 17 | Selección de comando o datos de la pantalla. |
| `LCD_RST` | 18 | Reinicio de la pantalla. |
| `LCD_BL` | 21 | Control de retroiluminación previsto en la PCB. El pin BL del módulo TFT del prototipo quedó sin conectar. |
| `SD_CS` | 14 | Selección de la microSD. El firmware de demostración no usa esta interfaz. |

## Audio y USB‑C

| Interfaz | Señal | GPIO | Función |
|---|---|---:|---|
| I²S | `I2S_BCLK` | 38 | Reloj de bits del MAX98357A. |
| I²S | `I2S_LRCLK` | 39 | Selección de canal. |
| I²S | `I2S_DOUT` | 40 | Datos de audio del ESP32‑S3 hacia el amplificador. |
| Audio | `AUDIO_EN` | 44 | Control de habilitación del amplificador. |
| USB | `USB_DM` | 19 | Línea de datos D−. |
| USB | `USB_DP` | 20 | Línea de datos D+. |

*Fuentes: esquemáticos de Altium y firmware `firmware/prueba/prueba.ino`.*

---

[Inicio](Home.md) · [Anterior · Arquitectura y alimentación](Arquitectura_y_alimentacion.md) · [Siguiente · PCB, BOM y fabricación](PCB_BOM_y_fabricacion.md)
