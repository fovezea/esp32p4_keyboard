#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

// --- Usage ---
// 1. hal_init()
// 2. hal_create_pin("axis.x.select", HAL_BIT, &my_bool_var, HAL_RD | HAL_WR);
// 3. Update my_bool_var in your driver or read it in your logic.

// --- Types ---

typedef enum {
  HAL_BIT,   // bool (0 or 1)
  HAL_FLOAT, // float
  HAL_S32,   // int32_t
  HAL_U32,   // uint32_t
             // Future: HAL_PORT/MSG for complex types?
} hal_type_t;

// Pin Flags
#define HAL_IN (1 << 0)  // Pin is an input TO the HAL (from hardware)
#define HAL_OUT (1 << 1) // Pin is an output FROM the HAL (to hardware/LEDs)
#define HAL_IO (HAL_IN | HAL_OUT)

typedef struct hal_pin_t hal_pin_t;

// --- API ---

/**
 * @brief Initialize the HAL registry.
 */
esp_err_t hal_init(void);

/**
 * @brief Create a new HAL pin.
 *
 * @param name Unique name for the pin (e.g. "axis.x.pos")
 * @param type Data type (HAL_BIT, HAL_FLOAT, etc.)
 * @param data_ptr Pointer to the storage variable. MUST REMAIN VALID (usually
 * static or heap).
 * @param dir Direction flags (HAL_IN, HAL_OUT) - purely informational for now.
 * @return esp_err_t ESP_OK or ESP_ERR_NO_MEM / ESP_ERR_INVALID_ARG
 */
esp_err_t hal_create_pin(const char *name, hal_type_t type, void *data_ptr,
                         uint32_t dir);

/**
 * @brief Find a pin by name.
 *
 * @param name Name to search for.
 * @return hal_pin_t* Pointer to the pin object, or NULL if not found.
 */
hal_pin_t *hal_find_pin(const char *name);

// --- Accessors (Safe/Convenient) ---

// Set a bit pin value. Safe to call from ISR (if data_ptr accessible).
void hal_set_bit(hal_pin_t *pin, bool value);

// Get a bit pin value.
bool hal_get_bit(hal_pin_t *pin);

// Set a float pin value.
void hal_set_float(hal_pin_t *pin, float value);

// Get a float pin value.
float hal_get_float(hal_pin_t *pin);

/**
 * @brief Debug: Dump all pins to stdout.
 */
void hal_dump(void);

#ifdef __cplusplus
}
#endif
