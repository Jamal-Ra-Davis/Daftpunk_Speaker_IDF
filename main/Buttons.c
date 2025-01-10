// #include <Arduino.h>
// #include <BluetoothA2DPSink.h>
#include "Buttons.h"
#include "global_defines.h"
#include "Events.h"

#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "driver/timer.h"
#include "esp_log.h"
#include "esp_err.h"

// TODO: Replace logging calls in ISRs with ISR safe logging method
// May need to keep logging framework to log from ISR
// #include "Logging.h"

#define VOL_TIMER_PERIOD 500
#define SHORT_PRESS_PERIOD 500
#define DEBOUNCE_PERIOD 50

#define VOLUME_PLUS_GPIO_SEL GPIO_SEL_32
#define VOLUME_MINUS_GPIO_SEL GPIO_SEL_34
#define PAIR_GPIO_SEL GPIO_SEL_35

#define VOLUME_PLUS_GPIO_NUM GPIO_NUM_32
#define VOLUME_MINUS_GPIO_NUM GPIO_NUM_34
#define PAIR_GPIO_NUM GPIO_NUM_35

static void volume_p_button_handler();
static void volume_m_button_handler();
static void pair_button_handler();

typedef enum
{
  VOLUME_PLUS,
  VOLUME_MINUS,
  NUM_VOLUME_KEYS
} volume_key_t;

struct volume_button_data
{
  uint32_t press_time;
  TimerHandle_t timer;
  uint32_t pin;
};

struct volume_button_data vol_data[NUM_VOLUME_KEYS];
static const char *timer_names[NUM_VOLUME_KEYS] = {"Timer_vol_p", "Timer_vol_m"};

static void volume_timer_func(TimerHandle_t xTimer);

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
  uint32_t gpio_num = (uint32_t)arg;
  switch (gpio_num)
  {
  case VOLUME_PLUS_GPIO_NUM:
    volume_p_button_handler();
    break;
  case VOLUME_MINUS_GPIO_NUM:
    volume_m_button_handler();
    break;
  case PAIR_GPIO_NUM:
    pair_button_handler();
    break;
  default:
    return;
  }
}

int init_buttons()
{
  esp_err_t ret;

  vol_data[VOLUME_PLUS].timer = xTimerCreate(timer_names[VOLUME_PLUS], MS_TO_TICKS(VOL_TIMER_PERIOD), pdFALSE, (void *)VOLUME_PLUS, volume_timer_func);
  vol_data[VOLUME_PLUS].pin = VOLUME_PLUS_GPIO_NUM;
  if (vol_data[VOLUME_PLUS].timer == NULL)
  {
    ESP_LOGE(BUTTON_TAG, "Failed to create timer for volume plus");
    return -1;
  }

  vol_data[VOLUME_MINUS].timer = xTimerCreate(timer_names[VOLUME_MINUS], MS_TO_TICKS(VOL_TIMER_PERIOD), pdFALSE, (void *)VOLUME_MINUS, volume_timer_func);
  vol_data[VOLUME_MINUS].pin = VOLUME_MINUS_GPIO_NUM;
  if (vol_data[VOLUME_MINUS].timer == NULL)
  {
    ESP_LOGE(BUTTON_TAG, "Failed to create timer for volume minus");
    return -1;
  }

  ret = setup_button_gpio_config();
  ESP_ERROR_CHECK(ret);

  ret = gpio_install_isr_service(0);
  ESP_ERROR_CHECK(ret);

  ret = gpio_isr_handler_add(VOLUME_PLUS_GPIO_NUM, gpio_isr_handler, (void *)VOLUME_PLUS_GPIO_NUM);
  ESP_ERROR_CHECK(ret);
  ret = gpio_isr_handler_add(VOLUME_MINUS_GPIO_NUM, gpio_isr_handler, (void *)VOLUME_MINUS_GPIO_NUM);
  ESP_ERROR_CHECK(ret);
  ret = gpio_isr_handler_add(PAIR_GPIO_NUM, gpio_isr_handler, (void *)PAIR_GPIO_NUM);
  ESP_ERROR_CHECK(ret);
  return 0;
}

int setup_button_gpio_config()
{
  esp_err_t ret;
  gpio_config_t gp_cfg = {
      .pin_bit_mask = VOLUME_PLUS_GPIO_SEL | VOLUME_MINUS_GPIO_SEL | PAIR_GPIO_SEL,
      .mode = GPIO_MODE_INPUT,
      .intr_type = GPIO_INTR_ANYEDGE,
  };
  ret = gpio_config(&gp_cfg);
  ESP_ERROR_CHECK(ret);
  return ret;
}

static void volume_button_handler(volume_key_t key)
{
  if (gpio_get_level(vol_data[key].pin) == 0)
  {
    // Button pressed

    // Save press time to compare against release time
    vol_data[key].press_time = esp_log_timestamp();

    // Start timer
    if (xTimerStart(vol_data[key].timer, 0) != pdPASS)
    {
      // Failed to start timer
    }
  }
  else
  {
    // Button released

    // Stop timer
    if (xTimerStop(vol_data[key].timer, 0) != pdPASS)
    {
      // Failed to stop timer
    }

    // Compare release time to press time to check for short press
    uint32_t delta = esp_log_timestamp() - vol_data[key].press_time;
    if (delta < DEBOUNCE_PERIOD)
    {
      // Do nothing
    }
    else if (delta <= SHORT_PRESS_PERIOD)
    {
      // Short press, Push short press events to event queue

      if (key == VOLUME_PLUS)
      {
        push_event(VOL_P_SHORT_PRESS, true);
      }
      else if (key == VOLUME_MINUS)
      {
        push_event(VOL_M_SHORT_PRESS, true);
      }
    }
  }
}
static void volume_p_button_handler()
{
  volume_button_handler(VOLUME_PLUS);
}
static void volume_m_button_handler()
{
  volume_button_handler(VOLUME_MINUS);
}

// Pair button can have normal short press long press behavior - LP: pair action, SP: select action
static void pair_button_handler()
{
  static uint32_t press_time = 0;
  if (gpio_get_level(PAIR_GPIO_NUM) == 0)
  {
    // Button pressed

    // Save press time to compare against release time
    press_time = esp_log_timestamp();
  }
  else
  {
    // Button released

    // Compare release time to press time to check for short press
    uint32_t delta = esp_log_timestamp() - press_time;
    if (delta < DEBOUNCE_PERIOD)
    {
      // Do nothing
    }
    else if (delta <= SHORT_PRESS_PERIOD)
    {
      push_event(PAIR_SHORT_PRESS, true);
    }
    else
    {
      push_event(PAIR_LONG_PRESS, true);
    }
  }
}

static void volume_timer_func(TimerHandle_t xTimer)
{
  uint32_t id;
  id = (uint32_t)pvTimerGetTimerID(xTimer);
  if (id >= NUM_VOLUME_KEYS)
  {
    // ESP_LOGE(BUTTON_TAG, "Invalid timer ID (%d)", id);
    return;
  }

  // Check if button is still pressed
  if (gpio_get_level(vol_data[id].pin) == 0)
  {
    if (id == VOLUME_PLUS)
    {
      push_event(VOL_P_SHORT_PRESS, true);
    }
    else if (id == VOLUME_MINUS)
    {
      push_event(VOL_M_SHORT_PRESS, true);
    }

    if (xTimerStart(xTimer, 0) != pdPASS)
    {
      // Failed to start timer
      ESP_LOGE(BUTTON_TAG, "Failed to start timer");
    }
  }
}