/// Ribbon 2
/// FM DSP Radio
/// 
/// Contributor: ikubaku <hide4d51 at gmail.com>
/// Contributor: _your_name_here_
/// 
/// Licensed under The MIT License.

// use Wire Library
#include <Wire.h>
// use Adafruit GFX Library
#include <Adafruit_GFX.h>
// use Adafruit SH110X Library
#include <Adafruit_SH110X.h>

// use Radio Library (RDA5807)
#include <radio.h>
#include <RDA5807M.h>


// データ構造定義
enum AppModeEnum {
  STANDBY,
  RADIO,
};

typedef enum AppModeEnum AppMode;


// タスク関数
AppMode switch_task(AppMode current_mode);

void standby_task_init(void);
void standby_task(void);
void standby_task_shutdown(void);

void radio_task_init(void);
void radio_task(void);
void radio_task_shutdown(void);


// 定数
int const LED_PIN PROGMEM = 3;
int const MODE_PIN PROGMEM = 4;
int const NEXT_PIN PROGMEM = 5;
int const PREV_PIN PROGMEM = 6;

unsigned long const DEBOUNCE_TIME_MS PROGMEM = 100;

uint8_t const DISPLAY_ADDRESS PROGMEM = 0x3C;
int const SCREEN_WIDTH PROGMEM = 128;
int const SCREEN_HEIGHT PROGMEM = 64;

uint8_t const RADIO_VOLUME PROGMEM = 1;

RADIO_FREQ const MAX_FREQ PROGMEM = 10800;    // 108.0 MHz
RADIO_FREQ const MIN_FREQ PROGMEM = 7600;     // 76.0 MHz
int16_t const TUNE_STEP PROGMEM = 50;         // 0.5 MHzステップ


// タスクステート
struct RadioTaskState {
  RADIO_FREQ tune_freq;
};


// デバイスのハンドル
Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
RDA5807M radio;


// 最初に1度だけ実行される
void setup() {
  // LED制御ピンの設定
  pinMode(LED_PIN, OUTPUT);

  // モード選択ボタンのピンの設定
  pinMode(MODE_PIN, INPUT_PULLDOWN);
  
  // 周波数調整ボタンのピンの設定
  pinMode(NEXT_PIN, INPUT_PULLDOWN);
  pinMode(PREV_PIN, INPUT_PULLDOWN);

  // OLEDディスプレイの初期化
  delay(250);    // ディスプレイのリセット待機
  display.begin(DISPLAY_ADDRESS, true);
  display.clearDisplay();
  display.display();

  // Radioライブラリの初期化
  radio.init();
  // 設定
  radio.setBand(RADIO_BAND_FMWORLD);    // 76-108MHz帯
  radio.setVolume(RADIO_VOLUME);
  radio.setMono(false);
  radio.setMute(true);

  // 初期タスクの準備
  standby_task_init();
}

// 繰り返し実行される
void loop() {
  static AppMode mode = STANDBY;
  static unsigned long last_mode_button_edge = 0;
  static int last_mode_button_state = LOW;
  static bool mode_button_pressed = false;

  static struct RadioTaskState radio_state = { .tune_freq = 7600 };

  switch(mode) {
    case STANDBY:
      standby_task();
      break;
    case RADIO:
      radio_task(&radio_state);
      break;
  }

  // モード選択ボタンの状態取得
  int mode_button_state = digitalRead(MODE_PIN);
  if (mode_button_state != last_mode_button_state) {
    last_mode_button_edge = millis();
  }
  if ((millis() - last_mode_button_state) > DEBOUNCE_TIME_MS) {
    int button_state = mode_button_pressed ? HIGH : LOW;
    if (mode_button_state != button_state) {
      mode_button_pressed = mode_button_state == HIGH;

      // 離す -> 押すのタイミング (LOW -> HIGH) でモードを切り替える
      if (mode_button_pressed) {
        mode = switch_task(mode, &radio_state);
      }
    }
  }
}

// タスク切り替えルーチン
AppMode switch_task(AppMode current_mode, struct RadioTaskState * const p_radio_state) {
  switch(current_mode) {
    case STANDBY:
      standby_task_shutdown();
      radio_task_init(p_radio_state);
      return RADIO;
    case RADIO:
      radio_task_shutdown(p_radio_state);
      standby_task_init();
      return STANDBY;
  }

  // unreachable
}

// スタンバイ時タスク（時計）
void standby_task_init() {
  display.clearDisplay();

  // メッセージの表示
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.print("Ribbon2: STBY");
  display.display();
}

void standby_task() {
}

void standby_task_shutdown() {
  //stub
}

// ラジオタスク
void radio_task_init(struct RadioTaskState * const p_radio_state) {
  display.clearDisplay();

  radio.setFrequency(p_radio_state->tune_freq);
  radio.setMute(false);

  // メッセージの表示
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.print("Ribbon2: RADIO");
  display.setCursor(0, 8);
  display.print("Freq:");
  display.print(p_radio_state->tune_freq / 100);
  display.print(".");
  display.print(p_radio_state->tune_freq % 100);
  display.display();
}

void radio_task(struct RadioTaskState * const p_radio_state) {
  static unsigned long last_next_button_edge = 0;
  static int last_next_button_state = LOW;
  static bool next_button_pressed = false;
  static unsigned long last_prev_button_edge = 0;
  static int last_prev_button_state = LOW;
  static bool prev_button_pressed = false;

  bool should_tune;
  int16_t tune_amount = 0;

  // 周波数選択ボタンの状態取得
  int next_button_state = digitalRead(NEXT_PIN);
  if (next_button_state != last_next_button_state) {
    last_next_button_edge = millis();
  }
  if ((millis() - last_next_button_state) > DEBOUNCE_TIME_MS) {
    int button_state = next_button_pressed ? HIGH : LOW;
    if (next_button_state != button_state) {
      next_button_pressed = next_button_state == HIGH;

      // 離す -> 押すのタイミング (LOW -> HIGH) 
      if (next_button_pressed) {
        should_tune = true;
        tune_amount += TUNE_STEP;
      }
    }
  }
  int prev_button_state = digitalRead(PREV_PIN);
  if (prev_button_state != last_prev_button_state) {
    last_prev_button_edge = millis();
  }
  if ((millis() - last_prev_button_state) > DEBOUNCE_TIME_MS) {
    int button_state = prev_button_pressed ? HIGH : LOW;
    if (prev_button_state != button_state) {
      prev_button_pressed = prev_button_state == HIGH;

      // 離す -> 押すのタイミング (LOW -> HIGH) 
      if (prev_button_pressed) {
        should_tune = true;
        tune_amount -= TUNE_STEP;
      }
    }
  }

  // 周波数調整と画面表示の更新
  if (should_tune) {
    RADIO_FREQ new_freq = p_radio_state->tune_freq + tune_amount;
    if (new_freq < MIN_FREQ) new_freq = MIN_FREQ;
    else if (MAX_FREQ < new_freq) new_freq = MAX_FREQ;
    if (new_freq != p_radio_state->tune_freq) {
      radio.setFrequency(new_freq);
      display.fillRect(0, 8, 127, 13, SH110X_BLACK);
      display.setTextSize(1);
      display.setTextColor(SH110X_WHITE);
      display.setCursor(0, 8);
      display.print("Freq:");
      display.print(new_freq / 100);
      display.print(".");
      display.print(new_freq % 100);
      display.display();
      p_radio_state->tune_freq = new_freq;
    }
  }
}

void radio_task_shutdown(struct RadioTaskState * const p_radio_state) {
  radio.setMute(true);
}
