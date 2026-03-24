# HeartBoxFirmware — Copilot Instructions

Heart Boxes is an ESP32/FreeRTOS firmware that connects paired IoT devices via AWS IoT Core MQTT. One partner presses a button; the other box lights up.

> **Detailed references (always follow these):**
> - [Project Summary](.github/instructions/Project%20Summary.instructions.md) — architecture, components, provisioning, troubleshooting
> - [Task Architecture Specification](.github/instructions/task-architecture-specification.instructions.md) — **mandatory** GenericTask pattern, lifecycle, coding rules

---

## Build & Flash

```bash
# Main firmware
idf.py build
idf.py -p COM4 flash monitor

# Test app (build main first)
cd test_app
idf.py build
idf.py -p COM4 flash monitor
```

VS Code tasks (Ctrl+Shift+B) wrap these commands. Only use `idf.py` — never raw `cmake` or `make`.

---

## Key Directory Map

| Path | Purpose |
|------|---------|
| `main/` | App entry point (`main.c`), task init order (`application.c`) |
| `components/system_wrappers/` | **GenericTask framework** — the core FreeRTOS abstraction |
| `components/gpio_task/` | Button input (GPIO26) + LED outputs (GPIO12, 14, 27) |
| `components/wifi_task/` | STA/AP modes, HTTP provisioning server |
| `components/aws_iot_task/` | MQTT connect/publish/subscribe over TLS |
| `components/state_machine_task/` | Application orchestrator; posts events to all task queues |
| `components/file_system/` | NVS-based persistent credential storage |
| `test_app/main/` | Unity test files — one file per component |
| `certs/{DEVICE_NAME}/` | Per-device X.509 certificates (never commit real ones) |

---

## Task Initialization Order (application.c)

```
esp_netif_init → file_system_init → wifi_task_init → gpio_task_init
  → aws_iot_task_init → state_machine_task_init
```

Each `*_task_init()` returns `esp_err_t`. Call `*_task_deinit()` to tear down.

---

## Adding a New Component

1. Create `components/<name>/` with `<name>.c`, `include/<name>.h`, `CMakeLists.txt`
2. Implement `<name>_task_init()` / `<name>_task_deinit()` using `generic_task_create()` + `generic_task_start()` — see [task-architecture-specification.instructions.md](.github/instructions/task-architecture-specification.instructions.md)
3. Add hardware init to `on_init()` callback (not in `_task_init()`)
4. Add test file `test_app/main/test_<name>.c` using Unity macros
5. Register in `main/CMakeLists.txt` and `test_app/main/test_app_main.c`

---

## Testing

Uses the **Unity** framework. Tests run on real hardware (not mocked).

```c
TEST(group_name, test_case_name) {
    TEST_ASSERT_EQUAL(ESP_OK, component_task_init());
    TEST_ASSERT_NOT_NULL(some_result);
    TEST_ASSERT_EQUAL(ESP_OK, component_task_deinit());
}
```

Run `test_app/scripts/add_test_group.bat <name>` to scaffold a new test file.

---

## Configuration

Set via `idf.py menuconfig` → **Example Configuration**:

| Key | Description |
|-----|-------------|
| `HEARTBOX_DEVICE_NAME` | `Heart_Box_1`, `Heart_Box_2`, or `Test` — drives certificate path |
| `HEARTBOX_TOPIC_NAME` | MQTT pub/sub topic (e.g., `heartbox/1`) |
| `HEARTBOX_AWS_IOT_ENDPOINT` | AWS IoT Core endpoint URL |
| `HEARTBOX_TEST_WIFI_SSID/PASSWORD` | Used only in test builds |

Certificates must exist at `certs/{DEVICE_NAME}/` as `client.key`, `client.crt`, and `root_cert_auth.pem` before building.

---

## Key Conventions

- **All task public APIs return `esp_err_t`** — never `void` or `GenericTask*`
- **Hardware init belongs in `on_init()` callback**, not in `*_task_init()`
- **Goto-cleanup pattern** in `on_init()` for resource rollback
- **`on_stop()` always continues cleanup** even if individual steps fail
- Logging: `ESP_LOGI/W/E/D(TAG, ...)` — define `static const char *TAG = "MODULE";` per file
- Naming: `snake_case` functions, `UPPER_CASE` macros, `TypeName_t` structs, `ePrefix_t` enums
- Message types defined in `components/system_wrappers/include/message_types.h`
