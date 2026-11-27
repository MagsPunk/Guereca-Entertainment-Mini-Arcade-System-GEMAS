# Pruebas y solución de problemas

## Orden de soldadura y pruebas por etapa

Desconecta el USB y la batería antes de soldar. Monta los componentes de una etapa a la vez. Antes de energizar, inspecciona la soldadura y comprueba que las líneas de alimentación estén libres de cortos. Continúa cuando la prueba de esa etapa sea satisfactoria.

| Etapa | Qué soldar o conectar | Prueba antes de continuar |
|---|---|---|
| 0. PCB sin componentes | Inspecciona la placa antes de soldar. | Comprueba que no haya cortos entre GND y `VBUS_V5`, `VSYS`, `+3V3` o `VBAT`. |
| 1. Entrada USB‑C | Monta el conector **J2**, las resistencias **R12/R13** de 5.1 kΩ y la protección **CR1**. | Conecta el USB‑C y mide cerca de **5 V** en `VBUS_V5` respecto a GND. Repite con el conector invertido. |
| 2. Cargador y salida del sistema | Monta el **BQ24074 (U1)**, sus resistencias y capacitores asociados y el conector **J1**. Deja la batería desconectada. | Alimenta por USB‑C y mide `VSYS` en **C2**. La salida nominal del BQ24074 es **4.4 V** en regulación. Si hay tensión en `VBUS_V5` y `VSYS` sigue en cero, revisa esta etapa antes de montar U2. |
| 3. Regulación a 3.3 V | Monta el **TPS63070 (U2)**, el inductor **L1** y los componentes de entrada, salida y realimentación. | Comprueba que haya **3.3 V estables** en `+3V3`, por ejemplo en **C3**. Si `VSYS` está presente y `+3V3` no aparece, revisa la entrada `EN` de U2 y la realimentación. |
| 4. ESP32‑S3 y programación | Monta **U3**, los capacitores de desacoplo, el circuito de `EN`, **SW1 (BOOT)**, **SW2 (RESET)**, **S1** y las resistencias USB **R16/R17**. | Mide de nuevo `+3V3` con U3 montado. Comprueba que la computadora detecte el puerto USB y que puedas cargar un sketch de prueba. |
| 5. Pantalla TFT | Conecta el módulo TFT SPI de 1.8″ según la tabla de pinout del prototipo. | Ejecuta una prueba de colores o texto. Si se ilumina sin mostrar imagen, revisa primero **SCK** y **MOSI**. |
| 6. Controles | Suelda las resistencias de los controles que falten y coloca la membrana. | Prueba cada botón por separado. Confirma que el GPIO pase de alto a bajo al pulsar y vuelva a alto al soltar. Repite con la tapa colocada para comprobar la alineación. |
| 7. Audio | Monta el **MAX98357A (U4)**, sus componentes asociados y **J5**. Conecta el altavoz. | Reproduce un tono de prueba. Comprueba que se escuche y que la pantalla y los controles sigan funcionando durante la reproducción. |
| 8. microSD | Monta el zócalo **J4** y sus componentes asociados. | Usa un sketch específico para montar la tarjeta, escribir un archivo y leerlo. Comprueba después que la pantalla y la microSD operen en el mismo bus SPI. |
| 9. Batería e integración | Conecta una **LiPo 1S compatible** a **J1** después de comprobar la polaridad y que la corriente de carga configurada sea adecuada para la batería. | Comprueba el funcionamiento con batería sola y luego la carga por USB‑C. Registra `VBAT`, `VSYS` y `+3V3` en ambas condiciones. |

**Puntos de medición.** C2 corresponde a `VSYS`, C5 a `VBAT` y C3 a `+3V3`. Para la prueba inicial del BQ24074 por USB‑C con la batería desconectada, mide `VSYS` en **C2** respecto a GND.

Referencias de alimentación. Esquemáticos del proyecto y hojas de datos de Texas Instruments del [BQ24074](https://www.ti.com/lit/ds/symlink/bq24074.pdf) y el [TPS63070](https://www.ti.com/lit/ds/symlink/tps63070.pdf).

## Resultados del prototipo

| Subsistema | Resultado registrado |
|---|---|
| Alimentación por USB‑C | Se midieron **4.82 V en VBUS** durante la prueba inicial. Queda pendiente documentar una prueba completa de carga de la batería. |
| Pantalla TFT | En la primera prueba se encendió la retroiluminación, pero la pantalla no mostraba imagen. La imagen apareció al corregir el intercambio entre **SCK** y **MOSI**. |
| Controles | Se comprobó que los ocho controles habilitados podían enviar señal al ESP32‑S3. La respuesta de algunos contactos dependía de la presión y la alineación de la membrana. |
| Audio | El firmware incluye salida I²S al MAX98357A. Queda pendiente registrar una prueba de reproducción en el altavoz. |
| microSD | El zócalo está previsto en la PCB. La lectura y escritura quedan pendientes de validación. |

## Diagnóstico por síntoma

| Síntoma | Comprobación |
|---|---|
| El equipo no aparece como puerto USB | Comprueba que el cable transmita datos. Si hace falta, sigue la secuencia **BOOT + RESET** de la [guía de puesta en marcha](Puesta_en_marcha_y_firmware.md). |
| La pantalla se ilumina sin mostrar imagen | Revisa primero **SCK → GPIO12** y **MOSI → GPIO11**. Después comprueba **CS → GPIO10**, **DC → GPIO17** y **RST → GPIO18**. |
| Un botón solo responde al presionar con fuerza | Revisa que la membrana esté centrada sobre los contactos. Compara la respuesta antes y después de colocar la tapa para identificar si el montaje afecta el recorrido. |
| La demostración no inicia | Comprueba que **S1** esté activo y pulsa uno de los ocho controles habilitados. Consulta [Pinout e interfaces](Pinout_e_interfaces.md) si el botón se identifica con otro nombre en el esquemático. |
| No se escucha audio | Coteja `I2S_BCLK`, `I2S_LRCLK`, `I2S_DOUT` y `AUDIO_EN` con la tabla de pinout. Revisa también la conexión del altavoz en **J5**. |

Quedan pendientes la prueba de carga de batería, el audio por altavoz, la microSD y la compilación y carga de la plantilla.

---

[Inicio](Home.md) · [Anterior · Puesta en marcha y firmware](Puesta_en_marcha_y_firmware.md)
