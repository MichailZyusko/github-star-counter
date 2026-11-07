// ============================================================
//  GitHub Stars Counter — матрица 8x40 (5 x MAX7219)
//  DEMO_MODE 1 — имитация звёзд, DEMO_MODE 0 — реальные данные с GitHub
//  Плата: NodeMCU v3 (ESP8266, ESP-12E)
// ============================================================

#include <MD_MAX72xx.h>
#include <SPI.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>

#define CS_PIN D8            // DIN -> D7 (MOSI), CLK -> D5 (SCK)

// ------------------- Настройки -------------------
#define DEMO_MODE 0          // 1 = быстрые смены + имитация новых звёзд

// Wi-Fi и токен хранятся в secrets.h рядом со скетчем (он в .gitignore).
// Создай его из шаблона secrets.example.h.
#if __has_include("secrets.h")
  #include "secrets.h"
#else
  #error "Нет secrets.h: скопируй secrets.example.h в secrets.h и впиши данные Wi-Fi"
#endif
#ifndef GITHUB_TOKEN
  #define GITHUB_TOKEN ""    // необязательно: без токена лимит 60 запросов/час
#endif

#if DEMO_MODE
const uint32_t PROJECT_DURATION_MS = 20UL * 1000;       // смена проекта
const uint32_t DEMO_CHANGE_MS      = 6UL * 1000;        // "новая звезда"
#else
const uint32_t PROJECT_DURATION_MS = 5UL * 60 * 1000;   // 5 минут
const uint32_t FETCH_INTERVAL_MS   = 3UL * 60 * 1000;   // опрос GitHub
#endif

const uint8_t BRIGHTNESS = 2;                           // 0..15

// Если картинка перевёрнута или зеркальная — поменяй эти флаги
#define FLIP_VERTICAL 1
#define MIRROR        1

// ------------------- Железо -------------------
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES   5
const int W = MAX_DEVICES * 8;   // 40
const int H = 8;

MD_MAX72XX mx(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

// ------------------- Графика -------------------
// Логотипы 11x8, старший бит = левый столбец
const int LOGO_W = 11;

const uint16_t LOGO_KEYBOARD[8] = {
  0b01111111110,
  0b10000000001,
  0b10101010101,
  0b10000000001,
  0b10010101001,
  0b10000000001,
  0b10011111001,
  0b01111111110,
};

const uint16_t LOGO_UFO[8] = {   // "телепорт" — летающая тарелка с лучами
  0b00001110000,
  0b00011111000,
  0b01111111110,
  0b11011011011,
  0b01111111110,
  0b00100100100,
  0b01000100010,
  0b10000100001,
};

// Жирные цифры 6x8 — для чисел до 4 знаков
const uint8_t FONT_BIG[10][8] = {
  {0b011110,0b110011,0b110011,0b110011,0b110011,0b110011,0b110011,0b011110}, // 0
  {0b001100,0b011100,0b111100,0b001100,0b001100,0b001100,0b001100,0b111111}, // 1
  {0b011110,0b110011,0b000011,0b000110,0b001100,0b011000,0b110000,0b111111}, // 2
  {0b011110,0b110011,0b000011,0b001110,0b000011,0b000011,0b110011,0b011110}, // 3
  {0b000110,0b001110,0b011110,0b110110,0b111111,0b000110,0b000110,0b000110}, // 4
  {0b111111,0b110000,0b111110,0b000011,0b000011,0b000011,0b110011,0b011110}, // 5
  {0b011110,0b110000,0b110000,0b111110,0b110011,0b110011,0b110011,0b011110}, // 6
  {0b111111,0b000011,0b000110,0b000110,0b001100,0b001100,0b011000,0b011000}, // 7
  {0b011110,0b110011,0b110011,0b011110,0b110011,0b110011,0b110011,0b011110}, // 8
  {0b011110,0b110011,0b110011,0b110011,0b011111,0b000011,0b000011,0b011110}, // 9
};

// Узкие цифры 4x7 — запасной вариант для 5-значных чисел
const uint8_t FONT_SMALL[10][8] = {
  {0b0110,0b1001,0b1001,0b1001,0b1001,0b1001,0b0110,0}, // 0
  {0b0010,0b0110,0b0010,0b0010,0b0010,0b0010,0b0111,0}, // 1
  {0b0110,0b1001,0b0001,0b0010,0b0100,0b1000,0b1111,0}, // 2
  {0b1110,0b0001,0b0001,0b0110,0b0001,0b0001,0b1110,0}, // 3
  {0b0010,0b0110,0b1010,0b1010,0b1111,0b0010,0b0010,0}, // 4
  {0b1111,0b1000,0b1110,0b0001,0b0001,0b1001,0b0110,0}, // 5
  {0b0110,0b1000,0b1000,0b1110,0b1001,0b1001,0b0110,0}, // 6
  {0b1111,0b0001,0b0010,0b0010,0b0100,0b0100,0b0100,0}, // 7
  {0b0110,0b1001,0b1001,0b0110,0b1001,0b1001,0b0110,0}, // 8
  {0b0110,0b1001,0b1001,0b0111,0b0001,0b0001,0b0110,0}, // 9
};

struct Font {
  const uint8_t (*glyphs)[8];
  uint8_t w;       // ширина цифры
  uint8_t h;       // высота
  uint8_t y;       // отступ сверху
};
const Font BIG_FONT   = {FONT_BIG,   6, 8, 0};
const Font SMALL_FONT = {FONT_SMALL, 4, 7, 1};

// Иконка Wi-Fi 9x8 и "уровень" каждой строки для анимации подключения
const uint16_t WIFI_ICON[8] = {
  0b000000000,
  0b011111110,
  0b100000001,
  0b001111100,
  0b010000010,
  0b000111000,
  0b001000100,
  0b000010000,
};
const uint8_t WIFI_LEVEL[8] = {9, 3, 3, 2, 2, 1, 1, 0};

// Пиксель в анимации морфинга: откуда (sx, sy) -> куда (dx, dy).
// Объявлен здесь, до всех функций: Arduino IDE вставляет прототипы
// функций перед первой из них, и тип должен быть уже известен.
struct Particle { int8_t sx, sy, dx, dy; };

// Прямоугольная рамка (включительные границы) — для этапа масштабирования
struct Frame8 { int8_t l, r, t, b; };

// Этап анимации логотипа: либо попиксельный морфинг двух картинок,
// либо плавное масштабирование рамки
enum StageType : uint8_t { STAGE_MORPH, STAGE_SCALE };
struct LogoStage {
  StageType type;
  const uint16_t *from, *to;   // для STAGE_MORPH
  Frame8 fa, fb;               // для STAGE_SCALE
};

// ------------------- Проекты -------------------
struct Project {
  const char *owner;
  const char *repo;
  const uint16_t *logo;
  uint32_t stars;
};

// Звёзды: в демо-режиме — стартовые значения для имитации,
// в реальном — запасные, если GitHub недоступен при старте
Project projects[] = {
  {"kirillzyusko", "react-native-keyboard-controller", LOGO_KEYBOARD, 2489},
  {"kirillzyusko", "react-native-teleport",            LOGO_UFO,      318},
};
const int PROJECT_COUNT = sizeof(projects) / sizeof(projects[0]);
int currentProject = 0;

// ------------------- Буфер кадра -------------------
// buf[x] — столбец, бит y — строка (x=0 слева, y=0 сверху)
uint8_t frame[W];

void clearBuf(uint8_t *buf) { memset(buf, 0, W); }

void setPixel(uint8_t *buf, int x, int y) {
  if (x >= 0 && x < W && y >= 0 && y < H) buf[x] |= (1 << y);
}

void drawBitmap(uint8_t *buf, int x0, int y0, const uint16_t *rows, int h, int w) {
  for (int r = 0; r < h; r++)
    for (int c = 0; c < w; c++)
      if ((rows[r] >> (w - 1 - c)) & 1) setPixel(buf, x0 + c, y0 + r);
}

const Font *font = &BIG_FONT;   // текущий шрифт (выбирается по длине числа)

void drawGlyph(uint8_t *buf, int x0, int y0, int8_t d) {
  if (d < 0 || d > 9) return;                 // -1 = пустое место
  for (int r = 0; r < font->h; r++)
    for (int c = 0; c < font->w; c++)
      if ((font->glyphs[d][r] >> (font->w - 1 - c)) & 1) setPixel(buf, x0 + c, y0 + r);
}

void pushToMatrix(const uint8_t *buf) {
  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
  for (int x = 0; x < W; x++) {
    for (int y = 0; y < H; y++) {
      bool on = (buf[x] >> y) & 1;
      int row = FLIP_VERTICAL ? (H - 1 - y) : y;
      int col = MIRROR ? x : (W - 1 - x);    // у MD_MAX72XX столбец 0 справа
      mx.setPoint(row, col, on);
    }
  }
  mx.update();
  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
}

// ------------------- Состояние цифр ("барабаны") -------------------
const int MAX_DIGITS = 5;        // до 99999 звёзд

// Позиция p=0 — единицы (крайняя правая)
int8_t  digCur[MAX_DIGITS];      // текущая цифра (-1 = пусто)
int8_t  digNext[MAX_DIGITS];     // цифра, которая "въезжает" снизу
int8_t  digDir[MAX_DIGITS];      // +1 крутим вверх, -1 вниз
uint8_t digOff[MAX_DIGITS];      // сдвиг прокрутки 0..7
bool    digSpin[MAX_DIGITS];

// Раскладка: число центрируется в свободной области справа от логотипа
const int NUM_AREA_X = LOGO_W + 1;           // 12
const int NUM_AREA_W = W - NUM_AREA_X;       // 28
const int DIGIT_GAP  = 1;
int layoutDigits = 1;
int layoutX0 = NUM_AREA_X;

int countDigits(uint32_t n) {
  int c = 1;
  while (n >= 10) { n /= 10; c++; }
  return c;
}

void layoutFor(uint32_t maxValue) {
  layoutDigits = countDigits(maxValue);
  font = (layoutDigits <= 4) ? &BIG_FONT : &SMALL_FONT;
  int total = layoutDigits * font->w + (layoutDigits - 1) * DIGIT_GAP;
  layoutX0 = NUM_AREA_X + (NUM_AREA_W - total + 1) / 2;
}

int digitX(int p) {
  return layoutX0 + (layoutDigits - 1 - p) * (font->w + DIGIT_GAP);
}

void numberToDigits(uint32_t n, int8_t *out) {
  if (n > 99999) n = 99999;
  for (int p = 0; p < MAX_DIGITS; p++) {
    if (n == 0 && p > 0) out[p] = -1;
    else { out[p] = n % 10; n /= 10; }
  }
}

void setDigitsStatic(uint32_t n) {
  layoutFor(n);
  numberToDigits(n, digCur);
  for (int p = 0; p < MAX_DIGITS; p++) { digOff[p] = 0; digSpin[p] = false; digDir[p] = 1; }
}

void renderProject(uint8_t *buf, const Project &pr) {
  clearBuf(buf);
  drawBitmap(buf, 0, 0, pr.logo, 8, LOGO_W);
  for (int p = 0; p < layoutDigits; p++) {
    int x = digitX(p);
    int s = digDir[p] * digOff[p];                  // вверх: текущая уезжает вверх
    drawGlyph(buf, x, font->y - s, digCur[p]);
    if (digSpin[p] && digOff[p] > 0)
      drawGlyph(buf, x, font->y - s + digDir[p] * 8, digNext[p]);
  }
}

// ------------------- Анимация: одометр (как HubSpot Odometer) -------------------
// Каждый разряд прокручивается через все значения, которые он прошёл бы
// у настоящего счётчика: единицы крутятся много раз, десятки меньше и т.д.
// Все разряды стартуют и финишируют одновременно, с плавным замедлением.
// Рост числа — барабаны едут вверх, уменьшение — вниз.
const uint16_t ODO_DURATION_MS = 2000;
const uint16_t ODO_FRAME_MS    = 16;
const int32_t  ODO_MAX_STEPS   = 30;   // потолок оборотов, чтобы не мельтешило

const uint32_t POW10[MAX_DIGITS] = {1, 10, 100, 1000, 10000};

int8_t  odoStart[MAX_DIGITS];
int32_t odoSteps[MAX_DIGITS];
bool    odoOldBlank[MAX_DIGITS];
bool    odoNewBlank[MAX_DIGITS];

float easeOutCubic(float t) { t = 1.0f - t; return 1.0f - t * t * t; }

int8_t odoDigitAt(int p, int32_t k) {
  if (k == 0 && odoOldBlank[p]) return -1;
  if (k == odoSteps[p] && odoNewBlank[p]) return -1;
  int v = (odoStart[p] + digDir[p] * (int)(k % 10)) % 10;
  return v < 0 ? v + 10 : v;
}

void animateTo(Project &pr, uint32_t newStars) {
  if (newStars > 99999) newStars = 99999;
  uint32_t oldStars = pr.stars;
  layoutFor(max(oldStars, newStars));   // места хватит и старому, и новому числу

  for (int p = 0; p < MAX_DIGITS; p++) {
    int32_t oldCol = oldStars / POW10[p];
    int32_t newCol = newStars / POW10[p];
    int32_t diff = newCol - oldCol;

    digDir[p]      = diff >= 0 ? 1 : -1;
    odoStart[p]    = oldCol % 10;
    odoOldBlank[p] = digCur[p] < 0;
    odoNewBlank[p] = (p > 0 && newCol == 0);

    int32_t steps = diff >= 0 ? diff : -diff;
    if (steps > ODO_MAX_STEPS)                       // сохраняем остаток по модулю 10
      steps -= 10 * ((steps - ODO_MAX_STEPS + 9) / 10);
    if (steps == 0 && odoOldBlank[p] != odoNewBlank[p])
      steps = 10;                                    // пустое -> цифра: один оборот
    odoSteps[p] = steps;
  }

  uint32_t start = millis();
  while (true) {
    float t = (millis() - start) / (float)ODO_DURATION_MS;
    if (t > 1.0f) t = 1.0f;
    float e = easeOutCubic(t);

    for (int p = 0; p < MAX_DIGITS; p++) {
      int32_t px = (int32_t)(e * odoSteps[p] * 8 + 0.5f);
      int32_t k = px / 8;
      digCur[p]  = odoDigitAt(p, k);
      digOff[p]  = px % 8;
      digSpin[p] = digOff[p] > 0;
      if (digSpin[p]) digNext[p] = odoDigitAt(p, k + 1);
    }

    renderProject(frame, pr);
    pushToMatrix(frame);
    if (t >= 1.0f) break;
    delay(ODO_FRAME_MS);
  }

  for (int p = 0; p < MAX_DIGITS; p++) {
    digCur[p] = odoDigitAt(p, odoSteps[p]);
    digOff[p] = 0;
    digSpin[p] = false;
  }
  pr.stars = newStars;
  layoutFor(newStars);                  // если число стало короче — перецентрируем
  renderProject(frame, pr);
  pushToMatrix(frame);
}

// ------------------- Анимация: смена проекта (morphing) -------------------
// Shared element transition: каждый светящийся пиксель старой картинки
// "перетекает" в пиксель новой. Совпадающие пиксели стоят на месте,
// лишние сливаются с ближайшими (фигура сжимается), недостающие
// вырастают из ближайших (фигура раскрывается).
// Логотип меняется по этапам, цифры — одновременно с ним.
// Движение — физическая пружина (как у анимации клавиатуры в iOS).

const float SPRING_MASS      = 3.0f;
const float SPRING_DAMPING   = 500.0f;
const float SPRING_STIFFNESS = 1000.0f;
const float SPRING_SPEED     = 1.0f;   // масштаб времени: >1 быстрее, <1 медленнее
const uint16_t MORPH_FRAME_MS = 16;

// Пружина от 0 к 1. Считаем по физике: F = -k(x-1) - c*v, шаг 1 мс.
struct Spring {
  float x = 0, v = 0;
  void reset() { x = 0; v = 0; }
  void advance(float dt) {
    const float h = 0.001f;
    while (dt > 0) {
      float st = dt < h ? dt : h;
      float acc = (-SPRING_STIFFNESS * (x - 1.0f) - SPRING_DAMPING * v) / SPRING_MASS;
      v += acc * st;
      x += v * st;
      dt -= st;
    }
  }
  // "Успокоилась": остаток пути меньше полпикселя на самом длинном перемещении
  bool settled(float maxDist) const {
    return fabsf(1.0f - x) * maxDist < 0.5f && fabsf(v) * maxDist < 2.0f;
  }
};

// Промежуточные кадры "клавиатура -> НЛО"
const uint16_t KB_EMPTY_FRAME[8] = {   // рамка клавиатуры без клавиш
  0b01111111110,
  0b10000000001,
  0b10000000001,
  0b10000000001,
  0b10000000001,
  0b10000000001,
  0b10000000001,
  0b01111111110,
};
const uint16_t UFO_HEAD[8] = {         // купол НЛО = рамка, сжатая до 5x2
  0b00001110000,
  0b00011111000,
  0, 0, 0, 0, 0, 0,
};

// Клавиатура -> НЛО (обратный переход проигрывается наоборот):
//   1) клавиши и пробел втягиваются в рамку
//   2) рамка сжимается до размеров купола; якорь — середина верхней линии:
//      верх остаётся на месте и стягивается к центру, низ поднимается
//   3) из купола вырастают корпус, огни и лучи
const LogoStage KB_TO_UFO[] = {
  {STAGE_MORPH, LOGO_KEYBOARD, KB_EMPTY_FRAME, {}, {}},
  {STAGE_SCALE, nullptr, nullptr, {0, 10, 0, 7}, {3, 7, 0, 1}},
  {STAGE_MORPH, UFO_HEAD, LOGO_UFO, {}, {}},
};

struct LogoRoute {
  const uint16_t *from, *to;
  const LogoStage *stages;
  uint8_t count;
};
const LogoRoute ROUTES[] = {
  {LOGO_KEYBOARD, LOGO_UFO, KB_TO_UFO, 3},
};

const int MAX_STAGES = 6;

// Собирает этапы from -> to. Если маршрут описан в обратную сторону —
// разворачивает его. Если не описан — один этап прямого морфинга.
int buildLogoStages(const uint16_t *from, const uint16_t *to, LogoStage *out) {
  for (const LogoRoute &rt : ROUTES) {
    if (rt.from == from && rt.to == to) {
      for (int i = 0; i < rt.count; i++) out[i] = rt.stages[i];
      return rt.count;
    }
    if (rt.from == to && rt.to == from) {
      for (int i = 0; i < rt.count; i++) {
        LogoStage st = rt.stages[rt.count - 1 - i];
        const uint16_t *tmp = st.from; st.from = st.to; st.to = tmp;
        Frame8 f = st.fa; st.fa = st.fb; st.fb = f;
        out[i] = st;
      }
      return rt.count;
    }
  }
  out[0] = {STAGE_MORPH, from, to, {}, {}};
  return 1;
}

// Рамка со скруглёнными углами. У совсем низкой рамки (2 строки)
// скругляется только верх — так она совпадает с куполом НЛО.
void drawRoundFrame(uint8_t *buf, int l, int r, int t, int b) {
  bool roundTop    = (r - l) >= 2;
  bool roundBottom = (r - l) >= 2 && (b - t) >= 2;
  for (int x = l; x <= r; x++) {
    bool corner = (x == l || x == r);
    if (!(corner && roundTop))    setPixel(buf, x, t);
    if (!(corner && roundBottom)) setPixel(buf, x, b);
  }
  for (int y = t + 1; y < b; y++) { setPixel(buf, l, y); setPixel(buf, r, y); }
}

// Сопоставление пикселей двух кадров в диапазоне столбцов [x0, x1)
int buildMorph(const uint8_t *bufA, const uint8_t *bufB, int x0, int x1,
               Particle *out, int maxOut) {
  static int8_t ax[W * H], ay[W * H], bx[W * H], by[W * H];
  static bool usedA[W * H], usedB[W * H];
  int nA = 0, nB = 0, n = 0;

  for (int x = x0; x < x1; x++)
    for (int y = 0; y < H; y++) {
      if ((bufA[x] >> y) & 1) { ax[nA] = x; ay[nA] = y; usedA[nA++] = false; }
      if ((bufB[x] >> y) & 1) { bx[nB] = x; by[nB] = y; usedB[nB++] = false; }
    }
  if (nA == 0 || nB == 0) {                // из пустоты / в пустоту — просто появление
    for (int j = 0; j < nB && n < maxOut; j++) out[n++] = {bx[j], by[j], bx[j], by[j]};
    return n;
  }

  // 1) общие пиксели остаются на месте
  for (int i = 0; i < nA; i++)
    for (int j = 0; j < nB; j++)
      if (!usedB[j] && ax[i] == bx[j] && ay[i] == by[j]) {
        usedA[i] = usedB[j] = true;
        if (n < maxOut) out[n++] = {ax[i], ay[i], bx[j], by[j]};
        break;
      }

  // 2) остальные — жадно, начиная с самых близких пар
  while (true) {
    int best = INT16_MAX, bi = -1, bj = -1;
    for (int i = 0; i < nA; i++) {
      if (usedA[i]) continue;
      for (int j = 0; j < nB; j++) {
        if (usedB[j]) continue;
        int dx = ax[i] - bx[j], dy = ay[i] - by[j];
        int d = dx * dx + dy * dy;
        if (d < best) { best = d; bi = i; bj = j; }
      }
    }
    if (bi < 0) break;
    usedA[bi] = usedB[bj] = true;
    if (n < maxOut) out[n++] = {ax[bi], ay[bi], bx[bj], by[bj]};
    yield();
  }

  // 3) лишние пиксели A сливаются с ближайшим пикселем B (сжатие)
  for (int i = 0; i < nA; i++) {
    if (usedA[i]) continue;
    int best = INT16_MAX, bj = 0;
    for (int j = 0; j < nB; j++) {
      int dx = ax[i] - bx[j], dy = ay[i] - by[j];
      int d = dx * dx + dy * dy;
      if (d < best) { best = d; bj = j; }
    }
    if (n < maxOut) out[n++] = {ax[i], ay[i], bx[bj], by[bj]};
  }

  // 4) недостающие пиксели B вырастают из ближайшего пикселя A (раскрытие)
  for (int j = 0; j < nB; j++) {
    if (usedB[j]) continue;
    int best = INT16_MAX, bi = 0;
    for (int i = 0; i < nA; i++) {
      int dx = ax[i] - bx[j], dy = ay[i] - by[j];
      int d = dx * dx + dy * dy;
      if (d < best) { best = d; bi = i; }
    }
    if (n < maxOut) out[n++] = {ax[bi], ay[bi], bx[j], by[j]};
  }
  return n;
}

void drawParticles(uint8_t *buf, const Particle *ps, int n, float e) {
  for (int i = 0; i < n; i++) {
    int x = lroundf(ps[i].sx + (ps[i].dx - ps[i].sx) * e);
    int y = lroundf(ps[i].sy + (ps[i].dy - ps[i].sy) * e);
    setPixel(buf, x, y);
  }
}

float particlesMaxDist(const Particle *ps, int n) {
  float m = 0;
  for (int i = 0; i < n; i++) {
    float d = sqrtf(sq(ps[i].dx - ps[i].sx) + sq(ps[i].dy - ps[i].sy));
    if (d > m) m = d;
  }
  return m;
}

float lerpf(float a, float b, float e) { return a + (b - a) * e; }

const int MAX_LOGO_PARTICLES  = 2 * LOGO_W * H;   // 176
const int MAX_DIGIT_PARTICLES = 2 * (W - LOGO_W) * H;
LogoStage logoStages[MAX_STAGES];
Particle  logoParticles[MAX_STAGES][MAX_LOGO_PARTICLES];
int       logoCount[MAX_STAGES];
float     logoDist[MAX_STAGES];
Particle  digitParticles[MAX_DIGIT_PARTICLES];

void drawLogoStage(uint8_t *buf, int s, float e) {
  const LogoStage &st = logoStages[s];
  if (st.type == STAGE_MORPH) {
    drawParticles(buf, logoParticles[s], logoCount[s], e);
  } else {
    drawRoundFrame(buf,
      lroundf(lerpf(st.fa.l, st.fb.l, e)), lroundf(lerpf(st.fa.r, st.fb.r, e)),
      lroundf(lerpf(st.fa.t, st.fb.t, e)), lroundf(lerpf(st.fa.b, st.fb.b, e)));
  }
}

void transitionTo(int nextIdx) {
  Project &from = projects[currentProject];
  Project &to   = projects[nextIdx];

  // Цифры: весь блок числа старого проекта -> весь блок нового
  static uint8_t a[W], b[W];
  renderProject(a, from);
  setDigitsStatic(to.stars);
  renderProject(b, to);
  int digitCount = buildMorph(a, b, LOGO_W, W, digitParticles, MAX_DIGIT_PARTICLES);
  float digitDist = particlesMaxDist(digitParticles, digitCount);

  // Логотип: этапы
  int stages = buildLogoStages(from.logo, to.logo, logoStages);
  for (int s = 0; s < stages; s++) {
    const LogoStage &st = logoStages[s];
    if (st.type == STAGE_MORPH) {
      clearBuf(a); drawBitmap(a, 0, 0, st.from, 8, LOGO_W);
      clearBuf(b); drawBitmap(b, 0, 0, st.to,   8, LOGO_W);
      logoCount[s] = buildMorph(a, b, 0, LOGO_W, logoParticles[s], MAX_LOGO_PARTICLES);
      logoDist[s]  = particlesMaxDist(logoParticles[s], logoCount[s]);
    } else {
      logoCount[s] = 0;
      logoDist[s]  = max(max(abs(st.fb.l - st.fa.l), abs(st.fb.r - st.fa.r)),
                         max(abs(st.fb.t - st.fa.t), abs(st.fb.b - st.fa.b)));
    }
  }

  // Две независимые пружины: одна ведёт этапы логотипа, другая — цифры
  Spring logoSpring, digitSpring;
  int stage = 0;
  bool digitsDone = digitCount == 0;
  uint32_t last = millis();

  while (true) {
    uint32_t now = millis();
    float dt = (now - last) / 1000.0f * SPRING_SPEED;
    last = now;

    if (stage < stages) {
      logoSpring.advance(dt);
      if (logoSpring.settled(logoDist[stage])) { stage++; logoSpring.reset(); }
    }
    if (!digitsDone) {
      digitSpring.advance(dt);
      digitsDone = digitSpring.settled(digitDist);
    }

    clearBuf(frame);
    if (stage < stages) drawLogoStage(frame, stage, logoSpring.x);
    else                drawBitmap(frame, 0, 0, to.logo, 8, LOGO_W);
    drawParticles(frame, digitParticles, digitCount, digitsDone ? 1.0f : digitSpring.x);
    pushToMatrix(frame);

    if (stage >= stages && digitsDone) break;
    delay(MORPH_FRAME_MS);
  }

  renderProject(frame, to);
  pushToMatrix(frame);
  currentProject = nextIdx;
}

// ------------------- Wi-Fi -------------------
void drawWifi(int level) {
  clearBuf(frame);
  for (int r = 0; r < 8; r++) {
    if (WIFI_LEVEL[r] > level) continue;
    uint16_t row = WIFI_ICON[r];
    drawBitmap(frame, (W - 9) / 2, r, &row, 1, 9);
  }
  pushToMatrix(frame);
}

bool connectWiFi() {
  Serial.printf("Connecting to %s", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  uint32_t start = millis();
  int level = 0;
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    drawWifi(level);
    level = (level + 1) % 4;
    Serial.print(".");
    delay(300);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nConnected, IP: %s\n", WiFi.localIP().toString().c_str());
    drawWifi(3);
    delay(700);
    return true;
  }
  Serial.println("\nWi-Fi failed, working offline");
  return false;
}

// ------------------- Данные -------------------
#if !DEMO_MODE
// Запрос https://api.github.com/repos/<owner>/<repo> и чтение stargazers_count.
// Ответ большой (~6 КБ), поэтому не грузим его целиком в память, а читаем
// поток до нужного поля. Возвращает false при любой ошибке.
bool fetchFromGitHub(const Project &pr, uint32_t &out) {
  if (WiFi.status() != WL_CONNECTED) return false;

  BearSSL::WiFiClientSecure client;
  client.setInsecure();   // без проверки сертификата: читаем публичные данные

  HTTPClient http;
  http.useHTTP10(true);   // без chunked-ответа, чтобы читать поток напрямую
  http.setTimeout(10000);

  String url = String("https://api.github.com/repos/") + pr.owner + "/" + pr.repo;
  if (!http.begin(client, url)) {
    Serial.println("GitHub: begin() failed");
    return false;
  }
  http.setUserAgent("esp-github-stars-counter");   // GitHub требует User-Agent
  http.addHeader("Accept", "application/vnd.github+json");
  if (strlen(GITHUB_TOKEN) > 0)
    http.addHeader("Authorization", String("Bearer ") + GITHUB_TOKEN);

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("GitHub %s: HTTP %d\n", pr.repo, code);   // 403 = лимит запросов
    http.end();
    return false;
  }

  Stream &stream = http.getStream();
  bool found = stream.find("\"stargazers_count\":");
  long value = found ? stream.parseInt() : -1;
  http.end();

  if (value < 0) {
    Serial.printf("GitHub %s: field not found\n", pr.repo);
    return false;
  }
  out = (uint32_t)value;
  return true;
}
#endif

// Демо: возвращает текущее значение.
// Реальный режим: спрашивает GitHub, при ошибке оставляет прежнее число.
uint32_t fetchStars(const Project &pr) {
#if DEMO_MODE
  return pr.stars;
#else
  uint32_t stars;
  if (fetchFromGitHub(pr, stars)) {
    Serial.printf("GitHub %s: %lu stars\n", pr.repo, (unsigned long)stars);
    return stars;
  }
  return pr.stars;
#endif
}

#if !DEMO_MODE
// Обновить все проекты. У показываемого — анимация, у остальных
// число просто подменится и будет видно при следующей смене проекта.
void refreshAllProjects() {
  for (int i = 0; i < PROJECT_COUNT; i++) {
    uint32_t stars = fetchStars(projects[i]);
    if (stars == projects[i].stars) continue;
    if (i == currentProject) animateTo(projects[i], stars);
    else                     projects[i].stars = stars;
  }
}
#endif

// ------------------- Arduino -------------------
uint32_t lastSwitch = 0;
uint32_t lastChange = 0;
uint32_t lastFetch  = 0;

void setup() {
  Serial.begin(115200);
  randomSeed(micros());

  mx.begin();
  mx.control(MD_MAX72XX::INTENSITY, BRIGHTNESS);
  mx.clear();

  connectWiFi();

  // При старте счётчик "накручивается" от пустоты до текущего значения
#if !DEMO_MODE
  // Сразу подтягиваем остальные проекты, чтобы при смене были свежие числа
  for (int i = 0; i < PROJECT_COUNT; i++)
    if (i != currentProject) projects[i].stars = fetchStars(projects[i]);
#endif
  Project &first = projects[currentProject];
  uint32_t initial = fetchStars(first);
  first.stars = 0;
  for (int p = 0; p < MAX_DIGITS; p++) { digCur[p] = -1; digDir[p] = 1; }
  animateTo(first, initial);

  lastSwitch = lastChange = lastFetch = millis();
}

void loop() {
  uint32_t now = millis();

  if (now - lastSwitch >= PROJECT_DURATION_MS) {
    transitionTo((currentProject + 1) % PROJECT_COUNT);
    lastSwitch = lastChange = millis();
    return;
  }

#if DEMO_MODE
  // Имитация: у текущего проекта появились новые звёзды
  if (now - lastChange >= DEMO_CHANGE_MS) {
    Project &pr = projects[currentProject];
    long r = random(0, 6);
    long delta = (r == 0) ? -random(1, 3)             // кто-то убрал звезду
               : (r == 1) ? random(8, 40)              // пачка звёзд
               :            random(1, 4);
    animateTo(pr, (uint32_t)max(0L, (long)pr.stars + delta));
    Serial.printf("%s: %lu stars\n", pr.repo, (unsigned long)pr.stars);
    lastChange = millis();
  }
#else
  // Реальный режим: периодически опрашиваем GitHub
  if (now - lastFetch >= FETCH_INTERVAL_MS) {
    refreshAllProjects();
    lastFetch = millis();
  }
#endif

  delay(10);
}
