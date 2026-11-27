
/*
  G.E.M.A.S. - ZELDA MINI QUEST v4.2
  ESP32-S3-WROOM-1-N16R8
  Arduino ESP32 3.3.12

  MODO 1 CONFIRMADO:
  INPUT + pull-ups externas de 10k
  Antirrebote: 5ms
  Repeticion D-pad: 25ms

  Cualquier boton activo inicia el juego.

  S2  GPIO6  ARRIBA
  S7  GPIO7  ABAJO
  S8  GPIO5  IZQUIERDA
  S3  GPIO4  DERECHA
  S5  GPIO42 A
  S10 GPIO47 B
  S11 GPIO9  START
  S6  GPIO8  SELECT

  S4 GPIO1 y S9 GPIO2 deshabilitados.
  S1 GPIO15 encendido logico.

  ST7735 horizontal 160x128
  SPI hardware + framebuffer
  Audio MAX98357A I2S
  Sin SD ni LittleFS.
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
#include <string.h>

// =====================================
// CONFIGURACION
// =====================================

constexpr int W = 160, H = 128;
constexpr int TOP = 20, BOTTOM = 114;

constexpr int S1 = 15;
constexpr int BL = 21;
constexpr int AUDIO_EN = 44;

constexpr uint32_t FRAME_MS = 33;
constexpr uint32_t KEY_MS = 5;
constexpr uint32_t MOVE_MS = 25;

constexpr int VOLUME = 24;
constexpr uint32_t RATE = 24000;
constexpr int BLOCK = 256;

// =====================================
// BOTONES
// =====================================

enum Key : uint8_t {
  K_UP, K_DOWN, K_LEFT, K_RIGHT,
  K_A, K_B, K_START, K_SELECT, NKEY
};

const uint8_t pins[NKEY] = {
  6, 7, 5, 4, 42, 47, 9, 8
};

const char *const labels[NKEY] = {
  "S2 ARRIBA",
  "S7 ABAJO",
  "S8 IZQUIERDA",
  "S3 DERECHA",
  "S5 A",
  "S10 B",
  "S11 START",
  "S6 SELECT"
};

const uint16_t tones[NKEY] = {
  523, 587, 659, 698,
  988, 784, 740, 392
};

struct Button {
  bool raw = false;
  bool down = false;
  uint32_t changed = 0;
};

Button key[NKEY];

const char *lastButton = "NINGUNO";

// =====================================
// PANTALLA OPTIMIZADA
// =====================================

Adafruit_ST7735 tft(&SPI, 10, 17, 18);

GFXcanvas16 *canvas = nullptr;
Adafruit_GFX *gfx = &tft;

bool rows[H] = {};
bool dirty = true;

uint32_t lastFrame = 0;

uint16_t pal[5];

uint16_t color(uint8_t r, uint8_t g, uint8_t b) {
  return tft.color565(r, g, b);
}

// Marcar solamente las franjas modificadas.
void mark(int a, int b) {
  a = max(0, a);
  b = min(H - 1, b);

  for (int y = a; y <= b; y++) {
    rows[y] = true;
  }

  dirty = true;
}

void markAll() {
  mark(0, H - 1);
}

void markPlayer(int y) {
  mark(y - 12, y + 29);
}

void markEnemy(int y) {
  mark(y, y + 16);
}

void present() {
  if (!canvas) return;

  uint16_t *pixels = canvas->getBuffer();

  for (int y = 0; y < H;) {
    if (!rows[y]) {
      y++;
      continue;
    }

    int begin = y;

    while (y < H && rows[y]) {
      rows[y++] = false;
    }

    tft.drawRGBBitmap(
      0, begin,
      pixels + begin * W,
      W, y - begin
    );
  }
}

// =====================================
// AUDIO I2S
// =====================================

struct Note {
  uint16_t hz;
  uint16_t ms;
};

// Melodia original de aventura.
// Incluye notas graves, medias y agudas.

const Note song[] = {
  {494,380},{587,390},{440,650},
  {494,380},{587,390},{440,720},{0,180},

  {659,260},{784,260},{988,390},
  {1175,540},{1047,300},{988,310},
  {880,560},{0,170},

  {587,280},{698,280},{880,390},
  {1047,480},{1175,360},{1319,640},
  {1175,260},{1047,290},{880,610},{0,180},

  {784,300},{880,300},{988,390},
  {1175,440},{1319,500},{1568,650},
  {1319,290},{1175,290},{1047,560},{0,190},

  {659,290},{784,290},{880,390},
  {988,440},{1047,540},{988,280},
  {880,350},{784,560},{0,180},

  {988,300},{1047,300},{1175,440},
  {1319,570},{1175,280},{1047,290},
  {988,430},{880,650},{587,370},
  {494,760},{0,460}
};

constexpr uint16_t NNOTES =
  sizeof(song) / sizeof(song[0]);

int16_t wave[256];

i2s_chan_handle_t tx = nullptr;

bool audioOK = false;

std::atomic<bool> audioOn{false};
std::atomic<bool> musicOn{false};
std::atomic<uint32_t> fxRequest{0};

uint32_t stepFor(uint16_t hz) {
  return (uint32_t)(((uint64_t)hz << 32) / RATE);
}

void playFX(uint16_t hz) {
  if (audioOK && audioOn.load()) {
    fxRequest.store(hz);
  }
}

void audioTask(void*) {
  int16_t pcm[BLOCK * 2];

  uint16_t note = 0;

  uint32_t position = 0;
  uint32_t phase = 0;
  uint32_t step = 0;
  uint32_t total = 1;
  uint32_t gate = 1;

  uint32_t fxPhase = 0;
  uint32_t fxStep = 0;
  uint32_t fxLeft = 0;

  bool wasOn = false;

  auto setNote = [&]() {
    step = stepFor(song[note].hz);

    total =
      RATE * song[note].ms / 1000UL;

    if (!total) total = 1;

    gate = total * 9 / 10;
    position = phase = 0;
  };

  setNote();

  for (;;) {
    bool enabled = audioOn.load();
    bool playing = enabled && musicOn.load();

    if (enabled && !wasOn) {
      note = 0;
      setNote();
    }

    wasOn = enabled;

    uint32_t request = fxRequest.exchange(0);

    if (request && enabled) {
      fxStep = stepFor((uint16_t)request);
      fxPhase = 0;
      fxLeft = RATE * 80 / 1000;
    }

    for (int i = 0; i < BLOCK; i++) {
      int32_t mix = 0;

      // Musica de fondo
      if (playing) {
        if (song[note].hz && position < gate) {
          int32_t env = 256;

          // Ataque suave
          if (position < 120) {
            env = (int32_t)position * 256 / 120;
          }

          // Salida suave
          uint32_t left = gate - position;

          if (left < 240) {
            int32_t release =
              (int32_t)left * 256 / 240;

            if (release < env) env = release;
          }

          // Fundamental + armonico agudo
          int32_t s =
            wave[phase >> 24] +
            wave[(phase >> 23) & 255] / 6;

          mix = (s * 4200L / 32768L)
                * env / 256;
        }

        phase += step;

        if (++position >= total) {
          note = (note + 1) % NNOTES;
          setNote();
        }
      }

      // Efectos de sonido independientes
      if (enabled && fxLeft) {
        mix +=
          wave[fxPhase >> 24] * 3200L / 32768L;

        fxPhase += fxStep;
        fxLeft--;
      }

      if (!enabled) {
        mix = 0;
        fxLeft = 0;
      }

      mix = mix * VOLUME / 100;

      if (mix > 16000) mix = 16000;
      if (mix < -16000) mix = -16000;

      pcm[2 * i] = (int16_t)mix;
      pcm[2 * i + 1] = (int16_t)mix;
    }

    size_t written = 0;

    if (
      i2s_channel_write(
        tx, pcm, sizeof(pcm),
        &written, portMAX_DELAY
      ) != ESP_OK
    ) {
      vTaskDelay(pdMS_TO_TICKS(2));
    }
  }
}

bool initAudio() {
  // Tabla precalculada: evita sinf()
  // durante la reproduccion de audio.
  for (int i = 0; i < 256; i++) {
    wave[i] = (int16_t)(
      sinf(6.2831853f * i / 256) * 32767
    );
  }

  i2s_chan_config_t c =
    I2S_CHANNEL_DEFAULT_CONFIG(
      I2S_NUM_AUTO,
      I2S_ROLE_MASTER
    );

  if (
    i2s_new_channel(&c, &tx, nullptr) != ESP_OK
  ) return false;

  i2s_std_config_t s = {};

  s.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(RATE);

  s.slot_cfg =
    I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
      I2S_DATA_BIT_WIDTH_16BIT,
      I2S_SLOT_MODE_STEREO
    );

  s.gpio_cfg.mclk = I2S_GPIO_UNUSED;
  s.gpio_cfg.bclk = (gpio_num_t)38;
  s.gpio_cfg.ws = (gpio_num_t)39;
  s.gpio_cfg.dout = (gpio_num_t)40;
  s.gpio_cfg.din = I2S_GPIO_UNUSED;

  if (
    i2s_channel_init_std_mode(tx, &s) != ESP_OK ||
    i2s_channel_enable(tx) != ESP_OK
  ) {
    i2s_del_channel(tx);
    tx = nullptr;
    return false;
  }

  if (
    xTaskCreatePinnedToCore(
      audioTask,
      "GEMAS_AUDIO",
      4096,
      nullptr,
      2,
      nullptr,
      0
    ) != pdPASS
  ) {
    i2s_channel_disable(tx);
    i2s_del_channel(tx);
    tx = nullptr;
    return false;
  }

  return true;
}

// =====================================
// SPRITES LINK 16x16
// =====================================

const char *const downSprite[16] = {
  ".......GG.......",
  "......GGGG......",
  ".....GGGGGG.....",
  "....GYYYYYYG....",
  "...GYYPPPPYYG...",
  "...YPPKPPKPPY...",
  "...YPPPPPPPPY...",
  "....YPPPPPPY....",
  ".....GGGGGG.....",
  "...PGGGKGGGP....",
  "..PPGGGKGGGPP...",
  "...PGGGKGGGP....",
  "....GGKKGG......",
  ".....B..B.......",
  "....BB..BB......",
  "................"
};

const char *const upSprite[16] = {
  "......GGGG......",
  ".....GGGGGG.....",
  "....GGGGGGGG....",
  "....GGGGGGGG....",
  ".....YYYYYY.....",
  "....YYKKKKYY....",
  "....GGGGGGGG....",
  "...PGGGGGGGGP...",
  "..PPGGGGGGGGPP..",
  "...PGGGKKGGGP...",
  "....GGGKKGGG....",
  "....GGGKKGGG....",
  ".....B..B.......",
  "....BB..BB......",
  "....BB..BB......",
  "................"
};

const char *const leftSprite[16] = {
  "......GGG.......",
  ".....GGGGG......",
  "....GGGGGG......",
  "...YYYYGGGG.....",
  "..YYPPPPGG......",
  "..YPKPPPPG......",
  "...YPPPPPP......",
  "....YPPPGG......",
  "...PGGGGGG......",
  "..PPGGGKGG......",
  "...PGGGKGG......",
  "....GGGKKG......",
  ".....GGKBB......",
  ".....BB.B.......",
  "....BB..B.......",
  "................"
};

uint16_t pixel(char c) {
  switch (c) {
    case 'G': return pal[0];
    case 'Y': return pal[1];
    case 'P': return pal[2];
    case 'K': return pal[3];
    default: return pal[4];
  }
}

void drawLink(
  int x, int y,
  uint8_t direction,
  bool walk
) {
  const char *const *s =
    direction == K_UP ? upSprite :
    direction == K_DOWN ? downSprite :
    leftSprite;

  for (int r = 0; r < 16; r++) {
    for (int col = 0; col < 16;) {
      int source =
        direction == K_RIGHT ? 15 - col : col;

      char c = s[r][source];

      if (c == '.') {
        col++;
        continue;
      }

      if (c == 'B' && walk && r >= 12) {
        gfx->drawPixel(
          x + col,
          y + r + ((col & 1) ? 1 : -1),
          pixel(c)
        );

        col++;
        continue;
      }

      // Dibujar pixeles iguales como una linea
      int len = 1;

      while (
        col + len < 16 &&
        s[r][direction == K_RIGHT
          ? 15 - col - len
          : col + len] == c
      ) {
        len++;
      }

      gfx->drawFastHLine(
        x + col, y + r, len, pixel(c)
      );

      col += len;
    }
  }
}

// =====================================
// ESTADO DEL JUEGO
// =====================================

enum Mode : uint8_t {
  MENU, PLAY, PAUSE, WIN, LOSE
};

Mode mode = MENU;

bool power = false;
bool s1Raw = false;

uint32_t s1Changed = 0;

uint32_t lastMove = 0;
uint32_t lastWalk = 0;
uint32_t lastEnemy = 0;

uint32_t swordEnd = 0;
uint32_t immuneEnd = 0;
uint32_t attackReady = 0;
uint32_t nextBlink = 0;
uint32_t pauseStart = 0;

int px = 72, py = 78;
int ex = 70, ey = 52;

int enemyDir = 1;
int lives = 3, gems = 0;

uint8_t facing = K_DOWN;

bool walkFrame = false;
bool gemTaken[5] = {};

const int gemX[5] = {
  19, 130, 52, 128, 27
};

const int gemY[5] = {
  40, 45, 82, 97, 101
};

// =====================================
// INTERFAZ
// =====================================

void textCenter(
  const char *s,
  int y,
  uint16_t c
) {
  gfx->setTextColor(c);
  gfx->setTextSize(1);

  gfx->setCursor(
    (W - (int)strlen(s) * 6) / 2, y
  );

  gfx->print(s);
}

void footer() {
  gfx->fillRect(
    0, BOTTOM, W, H - BOTTOM,
    color(8,22,38)
  );

  gfx->setTextColor(ST77XX_WHITE);
  gfx->setTextSize(1);
  gfx->setCursor(3, BOTTOM + 3);

  gfx->print(lastButton);
}

void drawScene() {
  gfx->fillScreen(ST77XX_BLACK);

  if (mode == MENU) {
    gfx->fillRect(
      0, 0, W, TOP, color(14,61,46)
    );

    textCenter("G.E.M.A.S.",6,ST77XX_WHITE);

    drawLink(72,24,K_DOWN,false);

    textCenter(
      "ZELDA MINI QUEST",45,ST77XX_YELLOW
    );

    textCenter(
      "PRESIONA UN BOTON",61,ST77XX_WHITE
    );

    textCenter("A: ESPADA",76,ST77XX_GREEN);
    textCenter("B: ESCUDO",88,ST77XX_CYAN);
    textCenter("SELECT: PAUSA",101,ST77XX_WHITE);

  } else if (mode == WIN || mode == LOSE) {
    textCenter("G.E.M.A.S.",14,ST77XX_GREEN);

    textCenter(
      mode == WIN ? "GANASTE!" : "GAME OVER",
      44, ST77XX_YELLOW
    );

    textCenter(
      mode == WIN
        ? "5 GEMAS CONSEGUIDAS"
        : "SIN VIDAS",
      64, ST77XX_WHITE
    );

    textCenter(
      "START: REINICIAR",91,ST77XX_WHITE
    );

  } else {
    // Terreno
    gfx->fillRect(
      0, TOP, W, BOTTOM - TOP,
      color(30,90,37)
    );

    gfx->drawRect(
      0, TOP, W, BOTTOM - TOP,
      color(12,57,24)
    );

    // Decoracion
    for (int i = 0; i < 12; i++) {
      gfx->drawPixel(
        7 + (i * 37) % 146,
        28 + (i * 23) % 75,
        color(42,111,46)
      );
    }

    // Gemas
    for (int i = 0; i < 5; i++) {
      if (!gemTaken[i]) {
        gfx->fillCircle(
          gemX[i], gemY[i], 4,
          ST77XX_CYAN
        );

        gfx->drawPixel(
          gemX[i], gemY[i] - 2,
          ST77XX_WHITE
        );
      }
    }

    // Enemigo
    gfx->fillRect(
      ex + 2, ey + 2, 12, 12, ST77XX_RED
    );

    gfx->fillRect(
      ex + 4, ey + 4, 3, 3, ST77XX_WHITE
    );

    gfx->fillRect(
      ex + 10, ey + 4, 3, 3, ST77XX_WHITE
    );

    gfx->drawPixel(
      ex + 5, ey + 5, ST77XX_BLACK
    );

    gfx->drawPixel(
      ex + 11, ey + 5, ST77XX_BLACK
    );

    uint32_t now =
      mode == PAUSE ? pauseStart : millis();

    // Link y parpadeo por dano
    if (!immuneEnd || ((now / 120) & 1) == 0) {
      drawLink(px, py, facing, walkFrame);
    }

    // Escudo
    if (key[K_B].down) {
      gfx->drawCircle(
        px + 8, py + 8, 12, ST77XX_CYAN
      );
    }

    // Espada
    if (swordEnd) {
      switch (facing) {
        case K_UP:
          gfx->fillRect(
            px+7,py-9,3,12,ST77XX_WHITE
          );
          break;

        case K_DOWN:
          gfx->fillRect(
            px+7,py+14,3,12,ST77XX_WHITE
          );
          break;

        case K_LEFT:
          gfx->fillRect(
            px-9,py+7,12,3,ST77XX_WHITE
          );
          break;

        case K_RIGHT:
          gfx->fillRect(
            px+14,py+7,12,3,ST77XX_WHITE
          );
          break;
      }
    }

    // Barra superior
    gfx->fillRect(
      0, 0, W, TOP, color(14,61,46)
    );

    gfx->setTextColor(ST77XX_WHITE);
    gfx->setTextSize(1);

    gfx->setCursor(3,6);
    gfx->print("G.E.M.A.S.");

    gfx->setCursor(91,2);
    gfx->print("VIDAS:");
    gfx->print(lives);

    gfx->setCursor(103,11);
    gfx->print("G:");
    gfx->print(gems);
    gfx->print("/5");

    // Pantalla de pausa
    if (mode == PAUSE) {
      gfx->fillRect(
        26,46,108,35,ST77XX_BLACK
      );

      gfx->drawRect(
        26,46,108,35,ST77XX_YELLOW
      );

      textCenter("PAUSA",51,ST77XX_YELLOW);

      textCenter(
        "SELECT CONTINUAR",66,ST77XX_WHITE
      );
    }
  }

  footer();
}

// =====================================
// INICIAR PARTIDA
// =====================================

void newGame(uint32_t now) {
  px = 72;
  py = 78;

  ex = 70;
  ey = 52;

  enemyDir = 1;
  facing = K_DOWN;
  walkFrame = false;

  lives = 3;
  gems = 0;

  memset(gemTaken, 0, sizeof(gemTaken));

  swordEnd = 0;
  immuneEnd = 0;
  attackReady = 0;
  nextBlink = 0;

  lastMove = lastWalk = lastEnemy = now;

  mode = PLAY;
  musicOn = true;

  markAll();
}

// =====================================
// MOVIMIENTO DE LINK
// =====================================

void movePlayer(uint8_t dir, uint32_t now) {
  if (mode != PLAY) return;

  markPlayer(py);

  facing = dir;

  if (dir == K_UP) py -= 2;
  if (dir == K_DOWN) py += 2;
  if (dir == K_LEFT) px -= 2;
  if (dir == K_RIGHT) px += 2;

  px = constrain(px, 2, W - 18);
  py = constrain(py, TOP + 3, BOTTOM - 18);

  if (now - lastWalk >= 90) {
    walkFrame = !walkFrame;
    lastWalk = now;
  }

  lastMove = now;

  markPlayer(py);
}

// =====================================
// ATAQUE
// =====================================

void attack(uint32_t now) {
  if (mode != PLAY) return;

  if (
    attackReady &&
    (int32_t)(now - attackReady) < 0
  ) return;

  swordEnd = now + 200;
  attackReady = now + 280;

  markPlayer(py);
  playFX(1175);

  int dx = ex - px;
  int dy = ey - py;

  bool hit =
    (facing == K_UP &&
     dy < 0 && dy > -27 && abs(dx) < 16) ||

    (facing == K_DOWN &&
     dy > 0 && dy < 27 && abs(dx) < 16) ||

    (facing == K_LEFT &&
     dx < 0 && dx > -27 && abs(dy) < 16) ||

    (facing == K_RIGHT &&
     dx > 0 && dx < 27 && abs(dy) < 16);

  if (hit) {
    markEnemy(ey);

    ex = ex > 80 ? 18 : 124;
    ey = 52;

    markEnemy(ey);
    playFX(1568);
  }
}

// =====================================
// LOGICA DEL JUEGO
// =====================================

void updateGame(uint32_t now) {
  if (mode != PLAY) return;

  // Movimiento enemigo
  if (now - lastEnemy >= 45) {
    markEnemy(ey);

    lastEnemy = now;

    ex += enemyDir;

    if (ex >= 126) {
      ex = 126;
      enemyDir = -1;
    }

    if (ex <= 17) {
      ex = 17;
      enemyDir = 1;
    }

    markEnemy(ey);
  }

  // Recoleccion de gemas
  for (int i = 0; i < 5; i++) {
    if (
      !gemTaken[i] &&
      abs(px + 8 - gemX[i]) <= 9 &&
      abs(py + 8 - gemY[i]) <= 9
    ) {
      gemTaken[i] = true;
      gems++;

      mark(gemY[i]-5, gemY[i]+5);
      mark(0, TOP-1);

      playFX(1568);

      if (gems == 5) {
        mode = WIN;
        musicOn = false;
        markAll();
        return;
      }
    }
  }

  // Terminar periodo de inmunidad
  if (immuneEnd) {
    if ((int32_t)(now - immuneEnd) >= 0) {
      immuneEnd = 0;
      markPlayer(py);

    } else if (
      (int32_t)(now - nextBlink) >= 0
    ) {
      nextBlink = now + 120;
      markPlayer(py);
    }
  }

  // Colision con enemigo
  if (
    abs(px - ex) < 11 &&
    abs(py - ey) < 11 &&
    !key[K_B].down &&
    !immuneEnd
  ) {
    markPlayer(py);

    lives--;

    immuneEnd = now + 1200;
    nextBlink = now + 120;

    px = 72;
    py = 78;

    markPlayer(py);
    mark(0, TOP-1);

    playFX(196);

    if (lives <= 0) {
      mode = LOSE;
      musicOn = false;
      markAll();
      return;
    }
  }

  // Eliminar espada al terminar ataque
  if (
    swordEnd &&
    (int32_t)(now - swordEnd) >= 0
  ) {
    swordEnd = 0;
    markPlayer(py);
  }
}

// =====================================
// EVENTOS DE BOTONES
// =====================================

void press(uint8_t id, uint32_t now) {
  lastButton = labels[id];

  mark(BOTTOM, H-1);
  playFX(tones[id]);

  // NUEVO:
  // CUALQUIER BOTON INICIA EL JUEGO.
  if (mode == MENU) {
    newGame(now);
    return;
  }

  // Reiniciar con START
  if (mode == WIN || mode == LOSE) {
    if (id == K_START) {
      newGame(now);
    }

    return;
  }

  if (id == K_SELECT) {
    if (mode == PLAY) {
      mode = PAUSE;
      pauseStart = now;
      musicOn = false;

      markAll();

    } else if (mode == PAUSE) {
      uint32_t pauseTime = now - pauseStart;

      if (swordEnd) swordEnd += pauseTime;

      if (immuneEnd) {
        immuneEnd += pauseTime;
        nextBlink += pauseTime;
      }

      if (attackReady) {
        attackReady += pauseTime;
      }

      lastEnemy = lastMove = now;

      mode = PLAY;
      musicOn = true;

      markAll();
    }

    return;
  }

  if (mode != PLAY) return;

  if (id <= K_RIGHT) {
    movePlayer(id, now);

  } else if (id == K_A) {
    attack(now);

  } else if (id == K_B) {
    markPlayer(py);
  }
}

// =====================================
// LECTURA DE BOTONES - MODO 1
// =====================================

void scanButtons(uint32_t now) {
  for (uint8_t i = 0; i < NKEY; i++) {
    bool reading =
      digitalRead(pins[i]) == LOW;

    if (reading != key[i].raw) {
      key[i].raw = reading;
      key[i].changed = now;
    }

    // Antirrebote de 5ms
    if (
      reading != key[i].down &&
      now - key[i].changed >= KEY_MS
    ) {
      key[i].down = reading;

      if (reading) {
        press(i, now);

      } else if (i == K_B) {
        markPlayer(py);
      }
    }
  }

  // Repeticion D-pad: 25ms
  if (
    mode == PLAY &&
    now - lastMove >= MOVE_MS
  ) {
    for (uint8_t i = K_UP; i <= K_RIGHT; i++) {
      if (key[i].down) {
        movePlayer(i, now);
        break;
      }
    }
  }

  // Terminar animacion al soltar el D-pad
  if (
    mode == PLAY &&
    walkFrame &&
    !key[K_UP].down &&
    !key[K_DOWN].down &&
    !key[K_LEFT].down &&
    !key[K_RIGHT].down
  ) {
    walkFrame = false;
    markPlayer(py);
  }
}

// =====================================
// INTERRUPTOR S1
// =====================================

void powerOn() {
  power = true;

  digitalWrite(BL, HIGH);
  digitalWrite(AUDIO_EN, audioOK ? HIGH : LOW);

  // Sincronizar botones
  for (int i = 0; i < NKEY; i++) {
    key[i].raw =
      digitalRead(pins[i]) == LOW;

    key[i].down = key[i].raw;
    key[i].changed = millis();
  }

  audioOn = true;
  musicOn = true;

  lastButton = "S1 ENCENDIDO";

  // Prueba inicial RGB
  tft.fillScreen(ST77XX_RED);
  delay(65);

  tft.fillScreen(ST77XX_GREEN);
  delay(65);

  tft.fillScreen(ST77XX_BLUE);
  delay(65);

  playFX(880);

  mode = MENU;
  markAll();
}

void powerOff() {
  power = false;

  audioOn = false;
  musicOn = false;
  fxRequest = 0;

  digitalWrite(AUDIO_EN, LOW);
  digitalWrite(BL, LOW);

  mode = MENU;
}

// =====================================
// SETUP
// =====================================

void setup() {
  // Interruptor logico
  pinMode(S1, INPUT_PULLUP);

  pinMode(BL, OUTPUT);
  pinMode(AUDIO_EN, OUTPUT);

  digitalWrite(BL, LOW);
  digitalWrite(AUDIO_EN, LOW);

  // MODO 1 CONFIRMADO
  // Todos los botones con pull-up EXTERNA.
  for (int i = 0; i < NKEY; i++) {
    pinMode(pins[i], INPUT);

    key[i].raw =
      digitalRead(pins[i]) == LOW;

    key[i].down = key[i].raw;
    key[i].changed = millis();
  }

  // GPIO1 y GPIO2 NO se utilizan.

  // Pantalla SPI por hardware
  SPI.begin(12, -1, 11, 10);

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.setSPISpeed(27000000);
  tft.fillScreen(ST77XX_BLACK);

  // Framebuffer de 40 KB
  static GFXcanvas16 offscreen(W, H);

  if (offscreen.getBuffer()) {
    canvas = &offscreen;
    gfx = &offscreen;
  }

  gfx->setTextWrap(false);

  // Colores precalculados
  pal[0] = color(27,178,54);
  pal[1] = color(232,190,53);
  pal[2] = color(249,177,118);
  pal[3] = color(37,50,37);
  pal[4] = color(128,76,36);

  // Audio
  audioOK = initAudio();

  s1Raw = digitalRead(S1) == LOW;
  s1Changed = millis();
}

// =====================================
// LOOP
// =====================================

void loop() {
  uint32_t now = millis();

  bool on = digitalRead(S1) == LOW;

  // Antirrebote de S1
  if (on != s1Raw) {
    s1Raw = on;
    s1Changed = now;
  }

  if (
    on != power &&
    now - s1Changed >= 30
  ) {
    if (on) {
      powerOn();
    } else {
      powerOff();
    }
  }

  if (!power) {
    delay(1);
    return;
  }

  now = millis();

  scanButtons(now);
  updateGame(now);

  // Refresco optimizado.
  // Solo enviar franjas modificadas.
  if (
    dirty &&
    now - lastFrame >= FRAME_MS
  ) {
    lastFrame = now;

    drawScene();
    present();

    dirty = false;
  }

  delay(1);
}
