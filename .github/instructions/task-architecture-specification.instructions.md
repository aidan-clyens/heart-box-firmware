---
applyTo: '**'
---

# Task Architecture Specification

## GenericTask Framework

All tasks in the HeartBoxFirmware project MUST follow the GenericTask architecture pattern for consistency, maintainability, and proper resource management.

### Core Principles

1. **Separation of Concerns**: External API functions vs. task-context callbacks
2. **Atomic Operations**: Start/stop operations either fully succeed or fully fail with rollback
3. **Resource Safety**: All resources created in `on_init` must be cleaned up in `on_stop`
4. **Error Propagation**: Callbacks return `esp_err_t` to enable proper error handling

### GenericTask Lifecycle

```
create → start → [running] → stop → delete
   ↓       ↓                    ↓       ↓
 malloc  xTask              vTask   vPortFree
 queue   on_init            on_stop  cleanup
 mutex                      
```

## API Functions (External Context)

These functions are called from **outside** the task context:

### `<module>_task_init()`
- **Purpose**: Create and start the GenericTask
- **Return**: `esp_err_t` (NOT `GenericTask*`)
- **Responsibilities**:
  - Call `generic_task_create()` with callbacks
  - Call `generic_task_start()` with stack size and priority
  - Clean up on failure (call `generic_task_delete()` if start fails)
- **Pattern**:
```c
esp_err_t gpio_task_init(void)
{
  gpio_task = generic_task_create(
      "GPIO",
      sizeof(GpioMsg_t),
      gpio_on_init,
      gpio_on_message,
      gpio_on_stop);

  if (gpio_task == NULL) {
    return ESP_ERR_NO_MEM;
  }

  esp_err_t ret = generic_task_start(gpio_task, 2048, 10);
  if (ret != ESP_OK) {
    generic_task_delete(gpio_task);
    gpio_task = NULL;
    return ret;
  }

  return ESP_OK;
}
```

### `<module>_task_deinit()`
- **Purpose**: Stop and clean up the GenericTask
- **Return**: `esp_err_t`
- **Responsibilities**:
  - Call `generic_task_stop()` to signal graceful shutdown
  - Call `generic_task_delete()` to free resources
  - Set task pointer to NULL
- **Pattern**:
```c
esp_err_t gpio_task_deinit(void)
{
  if (gpio_task == NULL) {
    return ESP_ERR_INVALID_STATE;
  }

  esp_err_t ret = generic_task_stop(gpio_task);
  if (ret != ESP_OK) {
    return ret;
  }

  ret = generic_task_delete(gpio_task);
  if (ret != ESP_OK) {
    return ret;
  }

  gpio_task = NULL;
  return ESP_OK;
}
```

### DO NOT Create These Functions
- ❌ `<module>_task_stop()` - Use `<module>_task_deinit()` instead
- ❌ `<module>_task_is_running()` - State management is internal
- ❌ Functions that return `GenericTask*` - Don't expose internal structure

## Callback Functions (Task Context)

These functions run **within** the task context and MUST NOT be called directly:

### `on_init(GenericTask *self)`
- **Purpose**: Initialize hardware and resources within task context
- **Return**: `esp_err_t` (`ESP_OK` on success, error code on failure)
- **Runs**: After FreeRTOS task is created, before entering message loop
- **Responsibilities**:
  - Initialize hardware (GPIO, I2C, SPI, etc.)
  - Create FreeRTOS resources (semaphores, timers, other tasks)
  - Install interrupt handlers
  - Allocate memory
- **Error Handling**: MUST use goto cleanup pattern for rollback
- **Pattern**:
```c
static esp_err_t gpio_on_init(GenericTask *self)
{
  esp_err_t ret;

  // Step 1: Initialize GPIO
  ret = gpio_config(&config);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "GPIO config failed");
    return ret;
  }

  // Step 2: Create semaphore
  semaphore = xSemaphoreCreateBinary();
  if (semaphore == NULL) {
    ESP_LOGE(TAG, "Semaphore creation failed");
    ret = ESP_ERR_NO_MEM;
    goto cleanup_gpio;
  }

  // Step 3: Install ISR
  ret = gpio_isr_handler_add(PIN, isr_handler, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "ISR install failed");
    goto cleanup_semaphore;
  }

  return ESP_OK;

cleanup_semaphore:
  vSemaphoreDelete(semaphore);
  semaphore = NULL;
cleanup_gpio:
  gpio_reset_pin(PIN);
  return ret;
}
```

### `on_message(GenericTask *self, void *msg_buf, size_t msg_len)`
- **Purpose**: Handle messages from the task queue
- **Return**: `void` (no error propagation needed)
- **Runs**: In message loop when queue receives data
- **Responsibilities**:
  - Validate message size
  - Parse message type
  - Execute commands
  - Update state
- **Pattern**:
```c
static void gpio_on_message(GenericTask *self, void *msg_buf, size_t msg_len)
{
  if (msg_len != sizeof(GpioMsg_t)) {
    return;  // Size mismatch, ignore
  }

  GpioMsg_t *msg = (GpioMsg_t *)msg_buf;

  switch (msg->type) {
    case GPIO_CMD_SET_STATE:
      // Handle command
      break;
    default:
      ESP_LOGW(TAG, "Unknown message type: %d", msg->type);
      break;
  }
}
```

### `on_stop(GenericTask *self)`
- **Purpose**: Clean up resources within task context before shutdown
- **Return**: `esp_err_t` (`ESP_OK` on success, error code on failure)
- **Runs**: After `should_stop` flag is set, before task deletes itself
- **Responsibilities**:
  - Stop and delete timers
  - Delete child tasks
  - Remove interrupt handlers
  - Delete semaphores/mutexes
  - Reset hardware to safe state
  - Free allocated memory
- **Error Handling**: Continue cleanup even if individual steps fail (log errors)
- **Important**: Failure does NOT prevent task shutdown (only logs error)
- **Pattern**:
```c
static esp_err_t gpio_on_stop(GenericTask *self)
{
  esp_err_t ret;

  // Step 1: Stop timers
  if (timer != NULL) {
    xTimerStop(timer, portMAX_DELAY);
    xTimerDelete(timer, portMAX_DELAY);
    timer = NULL;
  }

  // Step 2: Delete child tasks
  if (button_task_handle != NULL) {
    vTaskDelete(button_task_handle);
    button_task_handle = NULL;
  }

  // Step 3: Remove ISR (continue even if fails)
  ret = gpio_isr_handler_remove(PIN);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to remove ISR: %s", esp_err_to_name(ret));
    // Continue cleanup anyway
  }

  // Step 4: Delete semaphores
  if (semaphore != NULL) {
    vSemaphoreDelete(semaphore);
    semaphore = NULL;
  }

  // Step 5: Reset hardware
  gpio_set_level(PIN, GPIO_LOW);
  gpio_reset_pin(PIN);

  return ESP_OK;
}
```

## Coding Rules

### MUST DO
1. ✅ Initialize ALL hardware in `on_init()`, not in `<module>_task_init()`
2. ✅ Clean up ALL resources in `on_stop()` that were created in `on_init()`
3. ✅ Use goto cleanup pattern in `on_init()` for error rollback
4. ✅ Continue cleanup in `on_stop()` even if individual steps fail
5. ✅ Store handles for ALL child tasks/timers/semaphores created in `on_init()`
6. ✅ Return `esp_err_t` from init/deinit functions, not `void` or `GenericTask*`
7. ✅ Set all resource pointers to NULL after deletion
8. ✅ Validate message size in `on_message()` before casting
9. ✅ Use ESP_ERROR_CHECK for critical external API calls
10. ✅ Log errors with `esp_err_to_name()` for debugging

### MUST NOT DO
1. ❌ Call `on_init()`, `on_message()`, or `on_stop()` directly - they're callbacks
2. ❌ Initialize hardware in `<module>_task_init()` - use `on_init()` instead
3. ❌ Return `GenericTask*` from public API functions
4. ❌ Expose `GenericTask*` in header files
5. ❌ Create resources in `on_stop()` (it's cleanup only)
6. ❌ Use "rollback to running" logic in `on_stop()` - task always stops
7. ❌ Create `<module>_task_stop()` wrappers - use `<module>_task_deinit()` instead
8. ❌ Forget to save handles for resources (can't clean up without them)
9. ❌ Leave tasks running when module deinitializes
10. ❌ Delete task while holding a mutex

## Error Handling Philosophy

### `on_init()` Failure
- Task MUST NOT start running
- All partial initialization MUST be rolled back
- `generic_task_start()` returns error code
- Task state transitions: STARTING → ERROR → STOPPED

### `on_stop()` Failure  
- Task ALWAYS stops regardless of return value
- Errors are logged but don't prevent shutdown
- Continue cleanup for remaining resources
- This prevents "half-stopped" states

## Module Static Variables Pattern

```c
// Module state (file scope, static)
static GenericTask *module_task = NULL;
static TaskHandle_t helper_task_handle = NULL;
static SemaphoreHandle_t resource_semaphore = NULL;
static TimerHandle_t module_timer = NULL;

// Resource tracking
static bool is_hardware_initialized = false;
```

## Example: Complete Task Implementation

See `components/gpio_task/gpio_task.c` for a reference implementation that follows this specification.

## Testing Requirements

1. Test that `on_init()` failure prevents task from starting
2. Test that resources are cleaned up on `on_init()` failure
3. Test that `on_stop()` is called before task exits
4. Test that all resources are freed after `on_stop()`
5. Test that task cannot be deleted while running

## Common Mistakes to Avoid

### ❌ Wrong: Hardware Init in External Function
```c
esp_err_t module_task_init(void)
{
  gpio_config(&config);  // ❌ Wrong context!
  task = generic_task_create(...);
  generic_task_start(task, 2048, 10);
}
```

### ✅ Right: Hardware Init in Callback
```c
esp_err_t module_task_init(void)
{
  task = generic_task_create(..., module_on_init, ...);
  return generic_task_start(task, 2048, 10);
}

static esp_err_t module_on_init(GenericTask *self)
{
  return gpio_config(&config);  // ✅ Runs in task context
}
```

### ❌ Wrong: Exposing GenericTask
```c
GenericTask *module_task_init(void)  // ❌ Exposes internal structure
{
  return gpio_task;
}
```

### ✅ Right: Return Error Code
```c
esp_err_t module_task_init(void)  // ✅ Standard ESP-IDF pattern
{
  // Create and start task
  return ESP_OK;
}
```

### ❌ Wrong: No Cleanup on Init Failure
```c
static esp_err_t module_on_init(GenericTask *self)
{
  semaphore = xSemaphoreCreateBinary();
  if (gpio_config(&config) != ESP_OK) {
    return ESP_FAIL;  // ❌ Semaphore leaked!
  }
  return ESP_OK;
}
```

### ✅ Right: Goto Cleanup Pattern
```c
static esp_err_t module_on_init(GenericTask *self)
{
  semaphore = xSemaphoreCreateBinary();
  if (gpio_config(&config) != ESP_OK) {
    goto cleanup_semaphore;  // ✅ Proper rollback
  }
  return ESP_OK;

cleanup_semaphore:
  vSemaphoreDelete(semaphore);
  semaphore = NULL;
  return ESP_FAIL;
}
```

---

When reviewing or generating task code, always verify it follows this specification. Any deviation should be flagged and corrected.
