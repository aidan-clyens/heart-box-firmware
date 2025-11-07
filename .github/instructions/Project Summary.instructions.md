---
applyTo: '**'
---

# HeartBoxFirmware Project Summary

## Project Overview
This firmware targets ESP32 devices, integrating AWS IoT services using the ESP-IDF framework. The codebase is organized into components for modularity, with a focus on secure MQTT communication, device shadow management, and OTA updates.

## Architecture & Key Components
- `main/`: Entry point, device-specific logic, and certificate management.
- `components/`: Contains reusable modules:
  - `esp-aws-iot/`: AWS IoT integration (MQTT, Thing Shadow, Fleet Provisioning, Jobs, OTA).
  - `gpio_task/`, `wifi_task/`, `state_machine_task/`, etc.: Hardware and system abstractions.
- `certs/`: Device certificates for mutual TLS authentication. **Never commit real credentials.**
- `build/`: CMake build artifacts.

## Developer Workflows
- **Build:** Use ESP-IDF (`idf.py build`) or provided Docker tasks (`docker-build.bat`).
- **Flash:** Use ESP-IDF (`idf.py flash`) after connecting the device.
- **Configure:** Run `idf.py menuconfig` to set AWS endpoint, client ID, and other parameters under `Example Configuration`.
- **Provisioning:** Place device certificates in `main/certs/` as `client.key` and `client.crt`. Root CA is in `main/certs/root_cert_auth.pem`.
- **Debug:** Increase ESP log level and enable mbedTLS debug for connection issues. Serial output provides detailed logs for MQTT and shadow operations.

## Patterns & Conventions
- **Certificates:** Embedded in firmware for examples; production should use secure provisioning.
- **Configuration:** All AWS IoT settings are set via `menuconfig` and compiled in.
- **Logging:** Use ESP-IDF logging macros. Debug output is essential for troubleshooting AWS IoT connectivity.
- **Component Structure:** Each hardware/system feature is a separate component under `components/`.
- **AWS IoT Integration:** Follows AWS IoT C SDK patterns, adapted for ESP-IDF. See example directories for usage.

## Integration Points
- **AWS IoT:** MQTT, Thing Shadow, OTA, Jobs, Fleet Provisioning (see `esp-aws-iot/examples/`).
- **External Libraries:** AWS IoT Embedded C SDK, mbedTLS (for TLS).
- **Docker:** Build and run scripts in `docker/` for containerized development.

## Troubleshooting
- **TLS Handshake Errors:** Check certificate activation, endpoint correctness, and policy permissions in AWS Console.
- **MQTT Issues:** Ensure unique client IDs and correct endpoint configuration.
- **Thing Shadow:** Client identifier must match AWS Thing name exactly.

## Example: Adding a New AWS IoT Feature
1. Create a new component under `components/`.
2. Follow patterns in `esp-aws-iot/examples/` for configuration and certificate handling.
3. Register the component in `main/CMakeLists.txt`.

## Coding Guidelines

### Security
- Never commit real certificates or credentials to the repository
- Use secure storage mechanisms (NVS encryption) for sensitive data
- Validate all TLS/mTLS connections properly
- Follow AWS IAM least-privilege principles

### ESP-IDF Best Practices
- Use ESP-IDF logging macros (ESP_LOGI, ESP_LOGW, ESP_LOGE, ESP_LOGD)
- Always use ESP_ERROR_CHECK for critical operations
- Properly manage FreeRTOS resources (tasks, queues, mutexes, semaphores)
- Monitor task stack watermarks to prevent stack overflow
- Clean up resources in all code paths (success and error)

### Component Design
- Keep components modular and loosely coupled
- Define clear public APIs in component headers
- Use CMakeLists.txt to properly declare component dependencies
- Follow the existing component structure pattern

### Code Quality
- Write clear, self-documenting code with meaningful variable names
- Add comments for complex logic or hardware-specific requirements
- Use configuration options via menuconfig for user-configurable parameters
- Avoid magic numbers - use named constants or enums

## References
- See `components/esp-aws-iot/examples/README.md` for AWS IoT setup and troubleshooting.
- See individual example READMEs for feature-specific instructions.

---

When generating code, answering questions, or reviewing changes, always consider the embedded systems context, resource constraints, and AWS IoT integration patterns specific to this project. Keep answers shorter.