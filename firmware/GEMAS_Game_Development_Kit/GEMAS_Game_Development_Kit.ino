
/*
 * G.E.M.A.S. GAME DEVELOPMENT KIT v1.0
 * Guereca Entertainment Mini Arcade System
 *
 * ESP32-S3-WROOM-1-N16R8
 * Arduino-ESP32 3.3.12
 *
 * Modificar GAME SECTION para crear videojuegos.
 * Conservar GEMAS CORE y el mapa de GPIO.
 */

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <atomic>
#include <math.h>

// ============================================
// GEMAS CORE - HARDWARE
// ============================================

constexpr int W = 160, H = 128;

constexpr int PIN_POWER = 15;
constexpr int PIN_BL = 21;

constexpr int PIN_CS = 10;
constexpr int PIN_MOSI = 11;
constexpr int PIN_SCK = 12;
constexpr int PIN_DC = 17;
constexpr int PIN_RST = 18;

constexpr int PIN_BCLK = 38;
constexpr int PIN_LRCLK = 39;
constexpr int PIN_DOUT = 40;
constexpr int PIN_AUDIO_EN = 44;

constexpr int VOLUME = 24;
constexpr uint32_t SPI_HZ = 27000000;
constexpr uint32_t SAMPLE_RATE = 24000;

constexpr uint32_t FRAME_MS = 33;
constexpr uint32_t DEBOUNCE_MS = 5;
constexpr uint32_t REPEAT_MS = 25;

// ============================================
// GEMAS CORE - CONTROLES
// ============================================

enum Key : uint8_t {
  K_UP, K_DOWN, K_LEFT, K_RIGHT,
  K_A, K_B, K_START, K_SELECT, KEY_COUNT
};

// S2, S7, S8, S3, S5, S10, S11, S6
const uint8_t keyPins[KEY_COUNT] = {
  6, 7, 5, 4, 42, 47, 9, 8
};

// S4 GPIO1 y S9 GPIO2 no se utilizan.

struct KeyState {
  bool raw = false;
  bool down = false;
  bool event = false;
  bool repeat = false;
  uint32_t changed = 0;
  uint32_t lastRepeat = 0;
};

KeyState keys[KEY_COUNT];

bool pressed(uint8_t k) {
  return keys[k].event;
}

bool held(uint8_t k) {
  return keys[k].down;
}

bool repeated(uint8_t k) {
  return keys[k].repeat;
}

bool anyPressed() {
  for (int i = 0; i < KEY_COUNT; i++)
    if (pressed(i)) return true;

  return false;
}

void resetKeys() {
  for (int i = 0; i < KEY_COUNT; i++) {
    keys[i].raw = keys[i].down =
      digitalRead(keyPins[i]) == LOW;

    keys[i].event = keys[i].repeat = false;
    keys[i].changed = millis();
    keys[i].lastRepeat = millis();
  }
}

void updateKeys(uint32_t now) {
  for (int i = 0; i < KEY_COUNT; i++) {
    KeyState &k = keys[i];

    k.event = k.repeat = false;

    bool reading =
      digitalRead(keyPins[i]) == LOW;

    if (reading != k.raw) {
      k.raw = reading;
      k.changed = now;
    }

    if (reading != k.down &&
        now - k.changed >= DEBOUNCE_MS) {

      k.down = reading;

      if (reading) {
        k.event = true;
        k.repeat = true;
        k.lastRepeat = now;
      }
    }

    // Solo el D-pad tiene repeticion automatica.
    if (i <= K_RIGHT && k.down &&
        now - k.lastRepeat >= REPEAT_MS) {

      k.repeat = true;
      k.lastRepeat = now;
    }
  }
}

// ============================================
// GEMAS CORE - GRAFICOS
// ============================================

Adafruit_ST7735 tft(
  &SPI, PIN_CS, PIN_DC, PIN_RST
);

GFXcanvas16 screen(W, H);

bool dirty = true;

uint16_t rgb(int r, int g, int b) {
  return tft.color565(r, g, b);
}

void invalidate() {
  dirty = true;
}

void present() {
  tft.drawRGBBitmap(
    0, 0, screen.getBuffer(), W, H
  );
}

void textAt(
  int x, int y, const char *text,
  uint16_t c = ST77XX_WHITE,
  uint8_t size = 1
) {
  screen.setTextColor(c);
  screen.setTextSize(size);
  screen.setCursor(x, y);
  screen.print(text);
}

void centerText(
  const char *text, int y,
  uint16_t c = ST77XX_WHITE,
  uint8_t size = 1
) {
  int width = strlen(text) * 6 * size;

  textAt((W - width) / 2, y, text, c, size);
}

// ============================================
// GAME SECTION - MUSICA DEL VIDEOJUEGO
// ============================================

// Frecuencia en Hz y duracion en milisegundos.
// Frecuencia 0 = silencio.
// Melodia original de ejemplo.

struct Note {
  uint16_t hz;
  uint16_t ms;
};

const Note gameMusic[] = {
  {523,200}, {659,200}, {784,300},
  {1047,400}, {0,100},
  {880,200}, {698,200}, {587,300},
  {784,400}, {0,100},
  {659,200}, {880,200}, {1047,200},
  {1319,500}, {1175,200}, {784,300},
  {523,500}, {0,200}
};

constexpr int NOTE_COUNT =
  sizeof(gameMusic) / sizeof(gameMusic[0]);

// ============================================
// GEMAS CORE - AUDIO I2S
// ============================================

i2s_chan_handle_t audioTX = nullptr;

bool audioOK = false;

std::atomic<bool> audioEnabled{false};
std::atomic<bool> musicEnabled{false};
std::atomic<uint32_t> toneRequest{0};
std::atomic<uint32_t> audioGeneration{0};

int16_t musicWave[256];
int16_t fxWave[256];

uint32_t frequencyStep(uint16_t hz) {
  return uint32_t(
    ((uint64_t)hz << 32) / SAMPLE_RATE
  );
}

void setMusic(bool enabled) {
  musicEnabled.store(enabled);
}

void playTone(uint16_t hz, uint16_t ms) {
  if (!audioOK || !audioEnabled.load()) return;

  // Guardar frecuencia y duracion en un solo valor.
  toneRequest.store(
    ((uint32_t)ms << 16) | hz
  );
}

void audioTask(void *) {
  constexpr int BLOCK = 256;

  int16_t pcm[BLOCK * 2];

  uint32_t phase = 0, step = 0;
  uint32_t position = 0, total = 1, gate = 1;

  uint32_t fxPhase = 0, fxStep = 0;
  uint32_t fxLeft = 0;

  uint32_t generation = 0;
  int note = 0;

  auto loadNote = [&]() {
    phase = position = 0;
    step = frequencyStep(gameMusic[note].hz);

    total = max(
      1UL,
      (unsigned long)SAMPLE_RATE *
      gameMusic[note].ms / 1000UL
    );

    gate = total * 9 / 10;
  };

  loadNote();

  for (;;) {
    bool enabled = audioEnabled.load();
    bool playing = enabled && musicEnabled.load();

    uint32_t currentGeneration =
      audioGeneration.load();

    if (generation != currentGeneration) {
      generation = currentGeneration;
      note = 0;
      fxLeft = 0;
      loadNote();
    }

    uint32_t request = toneRequest.exchange(0);

    if (request && enabled) {
      fxPhase = 0;
      fxStep = frequencyStep(request & 0xFFFF);

      fxLeft = SAMPLE_RATE *
        (request >> 16) / 1000;
    }

    if (!enabled) fxLeft = 0;

    for (int i = 0; i < BLOCK; i++) {
      int32_t sample = 0;

      if (playing) {
        if (step && position < gate) {
          // Envolvente suave para evitar clics.
          uint32_t env = 128;

          if (position < 128) env = position;

          uint32_t remaining = gate - position;

          if (remaining < env) env = remaining;

          sample =
            musicWave[phase >> 24] *
            (int32_t)env / 128;
        }

        phase += step;

        if (++position >= total) {
          note = (note + 1) % NOTE_COUNT;
          loadNote();
        }
      }

      if (enabled && fxLeft) {
        sample += fxWave[fxPhase >> 24];
        fxPhase += fxStep;
        fxLeft--;
      }

      sample = constrain(sample, -32767, 32767);

      pcm[i * 2] = pcm[i * 2 + 1] = sample;
    }

    size_t written = 0;

    if (i2s_channel_write(
          audioTX, pcm, sizeof(pcm),
          &written, portMAX_DELAY
        ) != ESP_OK) {
      vTaskDelay(pdMS_TO_TICKS(2));
    }
  }
}

bool initAudio() {
  // Precalcular ondas y volumen una sola vez.
  for (int i = 0; i < 256; i++) {
    float p = 6.28318530718f * i / 256.0f;

    musicWave[i] =
      (sinf(p) + 0.16f * sinf(2 * p)) *
      4200.0f * VOLUME / 100;

    fxWave[i] =
      sinf(p) * 3200.0f * VOLUME / 100;
  }

  i2s_chan_config_t c =
    I2S_CHANNEL_DEFAULT_CONFIG(
      I2S_NUM_AUTO, I2S_ROLE_MASTER
    );

  if (i2s_new_channel(
        &c, &audioTX, nullptr
      ) != ESP_OK) return false;

  i2s_std_config_t s = {};

  s.clk_cfg =
    I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE);

  s.slot_cfg =
    I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
      I2S_DATA_BIT_WIDTH_16BIT,
      I2S_SLOT_MODE_STEREO
    );

  s.gpio_cfg.mclk = I2S_GPIO_UNUSED;
  s.gpio_cfg.bclk = (gpio_num_t)PIN_BCLK;
  s.gpio_cfg.ws = (gpio_num_t)PIN_LRCLK;
  s.gpio_cfg.dout = (gpio_num_t)PIN_DOUT;
  s.gpio_cfg.din = I2S_GPIO_UNUSED;

  if (i2s_channel_init_std_mode(
        audioTX, &s
      ) != ESP_OK ||
      i2s_channel_enable(audioTX) != ESP_OK) {

    i2s_del_channel(audioTX);
    audioTX = nullptr;
    return false;
  }

  if (xTaskCreatePinnedToCore(
        audioTask, "GEMAS_AUDIO",
        4096, nullptr, 2, nullptr, 0
      ) != pdPASS) {

    i2s_channel_disable(audioTX);
    i2s_del_channel(audioTX);
    audioTX = nullptr;
    return false;
  }

  return true;
}

// ============================================
// GEMAS CORE - ESTADOS DEL VIDEOJUEGO
// ============================================

enum GameState : uint8_t {
  MENU, PLAYING, PAUSED, WIN, GAME_OVER
};

GameState state = MENU;

void gameEnd(bool victory) {
  state = victory ? WIN : GAME_OVER;

  setMusic(false);

  playTone(victory ? 1319 : 196, 350);

  invalidate();
}

// ============================================
// GAME SECTION - PERSONALIZAR DESDE AQUI
// ============================================

// Nombre del videojuego.
const char *GAME_TITLE = "GEM HUNTER";

// Variables propias del juego.
int playerX, playerY;
int enemyX, enemyDir;
int score, lives;

bool shield = false;

uint32_t lastEnemy = 0;
uint32_t lastAttack = 0;
uint32_t safeSince = 0;
uint32_t attackUntil = 0;

const int gemX[5] = {18, 135, 45, 126, 28};
const int gemY[5] = {40, 40, 84, 99, 102};

bool collected[5];

uint16_t groundColor;

// Inicializar o reiniciar el juego.
void gameBegin() {
  playerX = 72;
  playerY = 78;

  enemyX = 70;
  enemyDir = 1;

  score = 0;
  lives = 3;

  shield = false;

  lastEnemy = millis();
  lastAttack = millis() - 250;
  safeSince = millis() - 1000;
  attackUntil = 0;

  for (int i = 0; i < 5; i++)
    collected[i] = false;

  groundColor = rgb(22, 75, 42);

  setMusic(true);
  invalidate();
}

// Actualizar controles, colisiones y reglas.
// No utilizar delay() dentro de esta funcion.
void gameUpdate(uint32_t now) {
  int dx = 0, dy = 0;

  if (repeated(K_UP)) dy -= 2;
  if (repeated(K_DOWN)) dy += 2;
  if (repeated(K_LEFT)) dx -= 2;
  if (repeated(K_RIGHT)) dx += 2;

  if (dx || dy) {
    playerX = constrain(playerX + dx, 2, W - 14);
    playerY = constrain(playerY + dy, 20, H - 14);

    invalidate();
  }

  // A = ataque.
  if (pressed(K_A) &&
      now - lastAttack >= 250) {

    lastAttack = now;
    attackUntil = now + 180;

    playTone(1175, 90);

    if (abs(playerX - enemyX) < 25 &&
        abs(playerY - 52) < 25) {

      enemyX = enemyX > W / 2 ? 12 : 136;

      playTone(1568, 120);
    }

    invalidate();
  }

  // B = escudo mientras se mantiene pulsado.
  bool newShield = held(K_B);

  if (newShield != shield) {
    shield = newShield;
    invalidate();
  }

  if (attackUntil &&
      (int32_t)(now - attackUntil) >= 0) {

    attackUntil = 0;
    invalidate();
  }

  // Movimiento independiente del enemigo.
  if (now - lastEnemy >= 45) {
    lastEnemy = now;
    enemyX += enemyDir;

    if (enemyX >= 142 || enemyX <= 5)
      enemyDir = -enemyDir;

    invalidate();
  }

  // Recoleccion de gemas.
  for (int i = 0; i < 5; i++) {
    if (!collected[i] &&
        abs(playerX + 6 - gemX[i]) <= 9 &&
        abs(playerY + 6 - gemY[i]) <= 9) {

      collected[i] = true;
      score++;

      playTone(1568, 100);
      invalidate();

      if (score == 5) {
        gameEnd(true);
        return;
      }
    }
  }

  // Colision con enemigo.
  if (!shield &&
      abs(playerX - enemyX) < 11 &&
      abs(playerY - 52) < 11 &&
      now - safeSince >= 1000) {

    safeSince = now;

    lives--;

    playerX = 72;
    playerY = 78;

    playTone(196, 180);

    invalidate();

    if (lives <= 0) gameEnd(false);
  }
}

// Dibujar personajes, terreno e interfaz.
// Dibujar exclusivamente en screen.
void gameDraw() {
  screen.fillScreen(groundColor);

  // Gemas.
  for (int i = 0; i < 5; i++) {
    if (!collected[i]) {
      screen.fillCircle(
        gemX[i], gemY[i], 4, ST77XX_CYAN
      );

      screen.drawPixel(
        gemX[i], gemY[i] - 2, ST77XX_WHITE
      );
    }
  }

  // Enemigo.
  screen.fillRect(
    enemyX, 52, 12, 12, ST77XX_RED
  );

  // Personaje de ejemplo.
  screen.fillRect(
    playerX, playerY, 12, 12, ST77XX_YELLOW
  );

  // Ataque.
  if (attackUntil) {
    screen.drawCircle(
      playerX + 6, playerY + 6,
      15, ST77XX_WHITE
    );
  }

  // Escudo.
  if (shield) {
    screen.drawCircle(
      playerX + 6, playerY + 6,
      10, ST77XX_CYAN
    );
  }

  // Barra de informacion.
  screen.fillRect(0, 0, W, 18, ST77XX_BLACK);

  textAt(3, 5, "GEMAS:");

  screen.print(score);
  screen.print("/5");

  textAt(91, 5, "VIDAS:");
  screen.print(lives);
}

// Permite ajustar los temporizadores del juego
// cuando se reanuda una partida pausada.
void gameResume(uint32_t pausedMs) {
  lastEnemy += pausedMs;
  lastAttack += pausedMs;
  safeSince += pausedMs;

  if (attackUntil) attackUntil += pausedMs;
}

// ============================================
// FIN DE GAME SECTION
// ============================================

// ============================================
// GEMAS CORE - MENU Y RENDERIZADO
// ============================================

void drawInterface() {
  if (state == MENU) {
    screen.fillScreen(ST77XX_BLACK);

    centerText("G.E.M.A.S.", 15, ST77XX_GREEN, 2);
    centerText(GAME_TITLE, 45, ST77XX_YELLOW);
    centerText("PRESIONA UN BOTON", 70);
    centerText("A: ATAQUE  B: ESCUDO", 88);
    centerText("SELECT: PAUSA", 103);

  } else if (state == WIN || state == GAME_OVER) {
    screen.fillScreen(ST77XX_BLACK);

    centerText(
      state == WIN ? "GANASTE!" : "GAME OVER",
      36, ST77XX_YELLOW, 2
    );

    centerText("START: REINICIAR", 82);

  } else {
    gameDraw();

    if (state == PAUSED) {
      screen.fillRect(
        18, 43, 124, 42, ST77XX_BLACK
      );

      screen.drawRect(
        18, 43, 124, 42, ST77XX_YELLOW
      );

      centerText("PAUSA", 51, ST77XX_YELLOW);
      centerText("SELECT CONTINUAR", 69);
    }
  }
}

void startGame() {
  state = PLAYING;

  gameBegin();

  invalidate();
}

// ============================================
// GEMAS CORE - ENCENDIDO
// ============================================

bool powered = false;
bool rawPower = false;

uint32_t powerChanged = 0;
uint32_t lastFrame = 0;
uint32_t pausedAt = 0;

void powerOn() {
  powered = true;

  resetKeys();

  digitalWrite(PIN_BL, HIGH);

  audioGeneration.fetch_add(1);
  toneRequest.store(0);

  audioEnabled.store(audioOK);
  digitalWrite(
    PIN_AUDIO_EN, audioOK ? HIGH : LOW
  );

  setMusic(true);

  state = MENU;
  invalidate();

  playTone(880, 100);
}

void powerOff() {
  powered = false;

  setMusic(false);
  audioEnabled.store(false);
  toneRequest.store(0);

  digitalWrite(PIN_AUDIO_EN, LOW);

  tft.fillScreen(ST77XX_BLACK);

  digitalWrite(PIN_BL, LOW);

  state = MENU;
}

// ============================================
// GEMAS CORE - SETUP
// ============================================

void setup() {
  pinMode(PIN_POWER, INPUT_PULLUP);

  pinMode(PIN_BL, OUTPUT);
  pinMode(PIN_AUDIO_EN, OUTPUT);

  digitalWrite(PIN_BL, LOW);
  digitalWrite(PIN_AUDIO_EN, LOW);

  // Configuracion de sensibilidad validada.
  for (int i = 0; i < KEY_COUNT; i++)
    pinMode(keyPins[i], INPUT);

  resetKeys();

  SPI.begin(
    PIN_SCK, -1, PIN_MOSI, PIN_CS
  );

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.setSPISpeed(SPI_HZ);
  tft.fillScreen(ST77XX_BLACK);

  // Comprobar memoria del framebuffer.
  if (!screen.getBuffer()) {
    digitalWrite(PIN_BL, HIGH);

    tft.setCursor(5, 20);
    tft.setTextColor(ST77XX_RED);
    tft.print("ERROR FRAMEBUFFER");

    while (true) delay(1000);
  }

  screen.setTextWrap(false);

  audioOK = initAudio();

  rawPower = digitalRead(PIN_POWER) == LOW;
  powerChanged = millis();
}

// ============================================
// GEMAS CORE - LOOP PRINCIPAL
// ============================================

void loop() {
  uint32_t now = millis();

  bool reading = digitalRead(PIN_POWER) == LOW;

  if (reading != rawPower) {
    rawPower = reading;
    powerChanged = now;
  }

  if (reading != powered &&
      now - powerChanged >= 30) {

    if (reading) powerOn();
    else powerOff();
  }

  if (!powered) {
    delay(1);
    return;
  }

  updateKeys(now);

  switch (state) {
    case MENU:
      // Cualquier boton activo inicia el juego.
      if (anyPressed()) startGame();
      break;

    case PLAYING:
      if (pressed(K_SELECT)) {
        pausedAt = now;

        state = PAUSED;
        setMusic(false);
        invalidate();

      } else {
        gameUpdate(now);
      }
      break;

    case PAUSED:
      if (pressed(K_SELECT)) {
        gameResume(now - pausedAt);

        state = PLAYING;
        setMusic(true);
        invalidate();
      }
      break;

    case WIN:
    case GAME_OVER:
      if (pressed(K_START)) startGame();
      break;
  }

  // Solo dibujar cuando existen cambios.
  if (dirty && now - lastFrame >= FRAME_MS) {
    lastFrame = now;

    drawInterface();
    present();

    dirty = false;
  }

  delay(1);
}
