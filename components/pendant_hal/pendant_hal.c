#include "pendant_hal.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>


static const char *TAG = "HAL";

// Simple linked list for now. Since we look up mainly during config load, O(N)
// is fine. For faster runtime access, drivers should store the returned
// hal_pin_t* pointer.
struct hal_pin_t {
  const char *name;
  hal_type_t type;
  void *data_ptr;
  uint32_t flags;
  struct hal_pin_t *next;
};

static hal_pin_t *s_pin_head = NULL;

esp_err_t hal_init(void) {
  ESP_LOGI(TAG, "Initializing HAL Registry...");
  s_pin_head = NULL;
  return ESP_OK;
}

esp_err_t hal_create_pin(const char *name, hal_type_t type, void *data_ptr,
                         uint32_t dir) {
  if (!name || !data_ptr) {
    return ESP_ERR_INVALID_ARG;
  }

  // Check for duplicate
  if (hal_find_pin(name) != NULL) {
    ESP_LOGE(TAG, "Pin '%s' already exists!", name);
    return ESP_ERR_INVALID_STATE;
  }

  hal_pin_t *new_pin =
      (hal_pin_t *)heap_caps_malloc(sizeof(hal_pin_t), MALLOC_CAP_DEFAULT);
  if (!new_pin) {
    return ESP_ERR_NO_MEM;
  }

  // Duplicate name string to be safe
  new_pin->name = strdup(name);
  if (!new_pin->name) {
    heap_caps_free(new_pin);
    return ESP_ERR_NO_MEM;
  }

  new_pin->type = type;
  new_pin->data_ptr = data_ptr;
  new_pin->flags = dir;
  new_pin->next = s_pin_head;
  s_pin_head = new_pin;

  ESP_LOGI(TAG, "Created Pin: '%s' Type: %d", name, type);
  return ESP_OK;
}

hal_pin_t *hal_find_pin(const char *name) {
  hal_pin_t *curr = s_pin_head;
  while (curr) {
    if (strcmp(curr->name, name) == 0) {
      return curr;
    }
    curr = curr->next;
  }
  return NULL;
}

// --- Accessors ---

void hal_set_bit(hal_pin_t *pin, bool value) {
  if (pin && pin->type == HAL_BIT && pin->data_ptr) {
    *(bool *)(pin->data_ptr) = value;
  }
}

bool hal_get_bit(hal_pin_t *pin) {
  if (pin && pin->type == HAL_BIT && pin->data_ptr) {
    return *(bool *)(pin->data_ptr);
  }
  return false;
}

void hal_set_float(hal_pin_t *pin, float value) {
  if (pin && pin->type == HAL_FLOAT && pin->data_ptr) {
    *(float *)(pin->data_ptr) = value;
  }
}

float hal_get_float(hal_pin_t *pin) {
  if (pin && pin->type == HAL_FLOAT && pin->data_ptr) {
    return *(float *)(pin->data_ptr);
  }
  return 0.0f;
}

void hal_dump(void) {
  printf("--- HAL Pin Dump ---\n");
  hal_pin_t *curr = s_pin_head;
  int count = 0;
  while (curr) {
    printf("[%03d] %-30s | Type: %d | Val: ", count++, curr->name, curr->type);

    switch (curr->type) {
    case HAL_BIT:
      printf("%s", (*(bool *)curr->data_ptr) ? "TRUE" : "false");
      break;
    case HAL_FLOAT:
      printf("%.3f", (*(float *)curr->data_ptr));
      break;
    case HAL_S32:
      printf("%ld", (*(int32_t *)curr->data_ptr));
      break;
    default:
      printf("???");
      break;
    }
    printf("\n");
    curr = curr->next;
  }
  printf("--------------------\n");
}
