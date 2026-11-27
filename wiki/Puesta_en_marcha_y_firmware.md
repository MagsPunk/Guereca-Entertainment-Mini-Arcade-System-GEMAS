# Puesta en marcha y firmware

## Entorno de desarrollo

| Recurso | Configuración |
|---|---|
| Código de demostración | [`firmware/prueba/prueba.ino`](../firmware/prueba/prueba.ino) |
| Editor | Arduino IDE 2 |
| Núcleo de placas | `esp32` de Espressif Systems. El código indica la versión **3.3.12**. |
| Bibliotecas | `Adafruit GFX Library` y `Adafruit ST7735 and ST7789 Library`. |
| Conexión | Puerto USB‑C de G.E.M.A.S y cable USB con datos. |

## Compilación y carga

1. Abre `prueba.ino` en Arduino IDE e instala el núcleo y las bibliotecas indicados.
2. Conecta G.E.M.A.S por USB‑C. Selecciona **ESP32S3 Dev Module** como perfil genérico y el puerto que aparezca al conectar la consola.
3. Para programar por el USB nativo del ESP32‑S3, configura **USB CDC On Boot → Enabled** y **Upload Mode → UART0 / Hardware CDC**. Si el puerto no aparece, mantén presionado **BOOT (SW1)**, pulsa y suelta **RESET (SW2)** y luego suelta **BOOT**. Selecciona el puerto que aparezca.
4. Ejecuta **Verificar** para compilar y después **Subir** para cargar el programa.

Referencias de carga: [Espressif, USB CDC y DFU](https://docs.espressif.com/projects/arduino-esp32/en/latest/tutorials/cdc_dfu_flash.html) y [Arduino, carga de un sketch](https://docs.arduino.cc/software/ide-v2/tutorials/getting-started/ide-v2-uploading-a-sketch/).

## Primera ejecución

Activa el encendido lógico con **S1**. Cuando aparezca la pantalla inicial, pulsa cualquiera de los ocho controles habilitados para iniciar la demostración. Comprueba la respuesta de la cruceta y de los botones A, B, Start y Select, además de la salida por el altavoz. La demostración funciona sin tarjeta microSD.

## Plantilla para crear videojuegos

Abre [`firmware/GEMAS_Game_Development_Kit/GEMAS_Game_Development_Kit.ino`](../firmware/GEMAS_Game_Development_Kit/GEMAS_Game_Development_Kit.ino) en Arduino IDE. Para crear un juego, modifica **`GAME SECTION`**. Ahí puedes definir la inicialización (`gameBegin()`), la lógica (`gameUpdate()`) y el dibujo (`gameDraw()`).

Conserva **`GEMAS CORE`** y el mapa de GPIO. La plantilla incluye el manejo de la pantalla, los botones, el audio y el encendido lógico.

La configuración se cotejó con el código. La compilación de ambos sketches y la carga física con esta guía quedan pendientes de verificar.

---

[Inicio](Home.md) · [Anterior · Carcasa y ensamblaje](Carcasa_y_ensamblaje.md) · [Siguiente · Pruebas y solución de problemas](Pruebas_y_solucion_de_problemas.md)
