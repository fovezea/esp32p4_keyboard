/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "driver/gpio.h"
#include "errno.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "usb/usb_host.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>


#include "usb/hid_host.h"
#include "usb/hid_usage_keyboard.h"
#include "usb/hid_usage_mouse.h"

#include "pendant_hal.h" // Import HAL

/* GPIO Pin number for quit from example logic */
#define APP_QUIT_PIN GPIO_NUM_0

static const char *TAG = "example";

// --- HAL PINS ---
// We will map keys '1', '2', '3', '4' to these softkeys
static bool s_hal_softkey_1 = false;
static bool s_hal_softkey_2 = false;
static bool s_hal_softkey_3 = false;
static bool s_hal_softkey_4 = false;

// We also store the last raw keycode for debugging
static int32_t s_hal_last_keycode = 0;

QueueHandle_t app_event_queue = NULL;

typedef enum { APP_EVENT = 0, APP_EVENT_HID_HOST } app_event_group_t;

typedef struct {
  app_event_group_t event_group;
  struct {
    hid_host_device_handle_t handle;
    hid_host_driver_event_t event;
    void *arg;
  } hid_host_device;
} app_event_queue_t;

static const char *hid_proto_name_str[] = {"NONE", "KEYBOARD", "MOUSE"};

typedef struct {
  enum key_state { KEY_STATE_PRESSED = 0x00, KEY_STATE_RELEASED = 0x01 } state;
  uint8_t modifier;
  uint8_t key_code;
} key_event_t;

// (Skipping large char tables and helper functions for brevity, using
// simplified logic) In a real implementation, we would keep the full tables or
// move them to a helper file. For now, we focus on the HAL integration.

/**
 * @brief Key Event Callback - Integrate HAL Here
 */
static void key_event_callback(key_event_t *key_event) {
  // 1. Update Raw Keycode in HAL
  s_hal_last_keycode = key_event->key_code;

  // 2. Simple Mapping (The "Input Translator")
  // Map '1' (Keycode 0x1E), '2' (0x1F), etc. to Softkeys
  if (key_event->state == KEY_STATE_PRESSED) {
    ESP_LOGI(TAG, "Key Pressed: 0x%02X", key_event->key_code);

    switch (key_event->key_code) {
    case HID_KEY_1: // '1' -> Softkey 1
      s_hal_softkey_1 = true;
      ESP_LOGI(TAG, "HAL: Softkey 1 ON");
      break;
    case HID_KEY_2:
      s_hal_softkey_2 = true;
      ESP_LOGI(TAG, "HAL: Softkey 2 ON");
      break;
    case HID_KEY_3:
      s_hal_softkey_3 = true;
      ESP_LOGI(TAG, "HAL: Softkey 3 ON");
      break;
    case HID_KEY_4:
      s_hal_softkey_4 = true;
      ESP_LOGI(TAG, "HAL: Softkey 4 ON");
      break;
    default:
      break;
    }
  } else {
    // Key Released
    switch (key_event->key_code) {
    case HID_KEY_1:
      s_hal_softkey_1 = false;
      ESP_LOGI(TAG, "HAL: Softkey 1 OFF");
      break;
    case HID_KEY_2:
      s_hal_softkey_2 = false;
      ESP_LOGI(TAG, "HAL: Softkey 2 OFF");
      break;
    case HID_KEY_3:
      s_hal_softkey_3 = false;
      ESP_LOGI(TAG, "HAL: Softkey 3 OFF");
      break;
    case HID_KEY_4:
      s_hal_softkey_4 = false;
      ESP_LOGI(TAG, "HAL: Softkey 4 OFF");
      break;
    }
  }

  // Dump HAL state occasionally/on change could happen here
}

static inline bool key_found(const uint8_t *const src, uint8_t key,
                             unsigned int length) {
  for (unsigned int i = 0; i < length; i++) {
    if (src[i] == key) {
      return true;
    }
  }
  return false;
}

static void hid_host_keyboard_report_callback(const uint8_t *const data,
                                              const int length) {
  hid_keyboard_input_report_boot_t *kb_report =
      (hid_keyboard_input_report_boot_t *)data;

  if (length < sizeof(hid_keyboard_input_report_boot_t)) {
    return;
  }

  static uint8_t prev_keys[HID_KEYBOARD_KEY_MAX] = {0};
  key_event_t key_event;

  for (int i = 0; i < HID_KEYBOARD_KEY_MAX; i++) {
    // key released
    if (prev_keys[i] > HID_KEY_ERROR_UNDEFINED &&
        !key_found(kb_report->key, prev_keys[i], HID_KEYBOARD_KEY_MAX)) {
      key_event.key_code = prev_keys[i];
      key_event.modifier = 0;
      key_event.state = KEY_STATE_RELEASED;
      key_event_callback(&key_event);
    }

    // key pressed
    if (kb_report->key[i] > HID_KEY_ERROR_UNDEFINED &&
        !key_found(prev_keys, kb_report->key[i], HID_KEYBOARD_KEY_MAX)) {
      key_event.key_code = kb_report->key[i];
      key_event.modifier = kb_report->modifier.val;
      key_event.state = KEY_STATE_PRESSED;
      key_event_callback(&key_event);
    }
  }

  memcpy(prev_keys, &kb_report->key, HID_KEYBOARD_KEY_MAX);
}

static void hid_host_mouse_report_callback(const uint8_t *const data,
                                           const int length) {
  // Mouse logic can be added here
}

static void hid_host_generic_report_callback(const uint8_t *const data,
                                             const int length) {
  // Generic logic
}

void hid_host_interface_callback(hid_host_device_handle_t hid_device_handle,
                                 const hid_host_interface_event_t event,
                                 void *arg) {
  uint8_t data[64] = {0};
  size_t data_length = 0;
  hid_host_dev_params_t dev_params;
  ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));

  switch (event) {
  case HID_HOST_INTERFACE_EVENT_INPUT_REPORT:
    ESP_ERROR_CHECK(hid_host_device_get_raw_input_report_data(
        hid_device_handle, data, 64, &data_length));

    if (HID_SUBCLASS_BOOT_INTERFACE == dev_params.sub_class) {
      if (HID_PROTOCOL_KEYBOARD == dev_params.proto) {
        hid_host_keyboard_report_callback(data, data_length);
      } else if (HID_PROTOCOL_MOUSE == dev_params.proto) {
        hid_host_mouse_report_callback(data, data_length);
      }
    } else {
      hid_host_generic_report_callback(data, data_length);
    }
    break;
  case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
    ESP_LOGI(TAG, "HID Device DISCONNECTED");
    ESP_ERROR_CHECK(hid_host_device_close(hid_device_handle));
    break;
  case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
    ESP_LOGI(TAG, "HID Device TRANSFER_ERROR");
    break;
  default:
    break;
  }
}

void hid_host_device_event(hid_host_device_handle_t hid_device_handle,
                           const hid_host_driver_event_t event, void *arg) {
  hid_host_dev_params_t dev_params;
  ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));

  switch (event) {
  case HID_HOST_DRIVER_EVENT_CONNECTED:
    ESP_LOGI(TAG, "HID Device CONNECTED");
    const hid_host_device_config_t dev_config = {
        .callback = hid_host_interface_callback, .callback_arg = NULL};

    ESP_ERROR_CHECK(hid_host_device_open(hid_device_handle, &dev_config));
    if (HID_SUBCLASS_BOOT_INTERFACE == dev_params.sub_class) {
      ESP_ERROR_CHECK(hid_class_request_set_protocol(hid_device_handle,
                                                     HID_REPORT_PROTOCOL_BOOT));
      if (HID_PROTOCOL_KEYBOARD == dev_params.proto) {
        ESP_ERROR_CHECK(hid_class_request_set_idle(hid_device_handle, 0, 0));
      }
    }
    ESP_ERROR_CHECK(hid_host_device_start(hid_device_handle));
    break;
  default:
    break;
  }
}

static void usb_lib_task(void *arg) {
  const usb_host_config_t host_config = {
      .skip_phy_setup = false,
      .intr_flags = ESP_INTR_FLAG_LEVEL1,
  };

  ESP_ERROR_CHECK(usb_host_install(&host_config));
  xTaskNotifyGive(arg);

  while (true) {
    uint32_t event_flags;
    usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
    if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
      ESP_ERROR_CHECK(usb_host_device_free_all());
      break;
    }
  }
  ESP_ERROR_CHECK(usb_host_uninstall());
  vTaskDelete(NULL);
}

static void gpio_isr_cb(void *arg) {
  BaseType_t xTaskWoken = pdFALSE;
  const app_event_queue_t evt_queue = {
      .event_group = APP_EVENT,
  };
  if (app_event_queue) {
    xQueueSendFromISR(app_event_queue, &evt_queue, &xTaskWoken);
  }
  if (xTaskWoken == pdTRUE) {
    portYIELD_FROM_ISR();
  }
}

void hid_host_device_callback(hid_host_device_handle_t hid_device_handle,
                              const hid_host_driver_event_t event, void *arg) {
  const app_event_queue_t evt_queue = {.event_group = APP_EVENT_HID_HOST,
                                       .hid_host_device.handle =
                                           hid_device_handle,
                                       .hid_host_device.event = event,
                                       .hid_host_device.arg = arg};
  if (app_event_queue) {
    xQueueSend(app_event_queue, &evt_queue, 0);
  }
}

void app_main(void) {
  BaseType_t task_created;
  app_event_queue_t evt_queue;
  ESP_LOGI(TAG, "HID Host + HAL Example");

  // 1. Initialize HAL
  ESP_ERROR_CHECK(hal_init());

  // 2. Create HAL Pins
  // These "Softkeys" represent the semantic functions we want in LinuxCNC
  ESP_ERROR_CHECK(
      hal_create_pin("pendant.softkey.1", HAL_BIT, &s_hal_softkey_1, HAL_IO));
  ESP_ERROR_CHECK(
      hal_create_pin("pendant.softkey.2", HAL_BIT, &s_hal_softkey_2, HAL_IO));
  ESP_ERROR_CHECK(
      hal_create_pin("pendant.softkey.3", HAL_BIT, &s_hal_softkey_3, HAL_IO));
  ESP_ERROR_CHECK(
      hal_create_pin("pendant.softkey.4", HAL_BIT, &s_hal_softkey_4, HAL_IO));
  ESP_ERROR_CHECK(hal_create_pin("hw.kb.last_keycode", HAL_S32,
                                 &s_hal_last_keycode, HAL_IN));

  ESP_LOGI(TAG, "HAL Initialized. Connect USB Keyboard...");

  // 3. Init Boot Button (Quit)
  const gpio_config_t input_pin = {
      .pin_bit_mask = BIT64(APP_QUIT_PIN),
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_ENABLE,
      .intr_type = GPIO_INTR_NEGEDGE,
  };
  ESP_ERROR_CHECK(gpio_config(&input_pin));
  ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1));
  ESP_ERROR_CHECK(gpio_isr_handler_add(APP_QUIT_PIN, gpio_isr_cb, NULL));

  // 4. Start USB Tasks
  task_created =
      xTaskCreatePinnedToCore(usb_lib_task, "usb_events", 4096,
                              xTaskGetCurrentTaskHandle(), 2, NULL, 0);
  assert(task_created == pdTRUE);
  ulTaskNotifyTake(false, 1000);

  const hid_host_driver_config_t hid_host_driver_config = {
      .create_background_task = true,
      .task_priority = 5,
      .stack_size = 4096,
      .core_id = 0,
      .callback = hid_host_device_callback,
      .callback_arg = NULL};
  ESP_ERROR_CHECK(hid_host_install(&hid_host_driver_config));

  app_event_queue = xQueueCreate(10, sizeof(app_event_queue_t));

  // 5. Event Loop
  while (1) {
    if (xQueueReceive(app_event_queue, &evt_queue, portMAX_DELAY)) {
      if (APP_EVENT == evt_queue.event_group) {
        // Quit logic...
        break;
      }

      if (APP_EVENT_HID_HOST == evt_queue.event_group) {
        hid_host_device_event(evt_queue.hid_host_device.handle,
                              evt_queue.hid_host_device.event,
                              evt_queue.hid_host_device.arg);
      }
    }
  }

  // Cleanup
  ESP_LOGI(TAG, "HID Driver uninstall");
  ESP_ERROR_CHECK(hid_host_uninstall());
  gpio_isr_handler_remove(APP_QUIT_PIN);
  vQueueDelete(app_event_queue);
}
