---
applyTo: "components/aws_iot_task/**,main/**,certs/**"
---

# AWS IoT Instructions

Covers cloud resource provisioning, certificate management, AWS CLI usage, and firmware-side conventions for AWS IoT Core integration.

> General task architecture rules still apply — see [task-architecture-specification.instructions.md](task-architecture-specification.instructions.md).

---

## AWS IoT Resource Checklist

Every physical device requires these resources in AWS IoT Core **before** building firmware:

1. **Thing** — name must exactly match `HEARTBOX_DEVICE_NAME` (e.g., `Heart_Box_1`)
2. **X.509 Certificate** — generated in AWS Console, must be set to **Active**
3. **Policy** — attached to the certificate (see minimum policy below)
4. **Certificate attached to Thing** — via Console or CLI

Verify with:
```bash
aws iot list-things
aws iot list-certificates
aws iot list-thing-principals --thing-name Heart_Box_1
```

---

## Minimum IAM Policy

Use least-privilege. Only grant what the device requires:

```json
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Effect": "Allow",
      "Action": ["iot:Connect"],
      "Resource": "arn:aws:iot:REGION:ACCOUNT_ID:client/Heart_Box_*"
    },
    {
      "Effect": "Allow",
      "Action": ["iot:Publish"],
      "Resource": "arn:aws:iot:REGION:ACCOUNT_ID:topic/heartbox/*"
    },
    {
      "Effect": "Allow",
      "Action": ["iot:Subscribe"],
      "Resource": "arn:aws:iot:REGION:ACCOUNT_ID:topicfilter/heartbox/*"
    },
    {
      "Effect": "Allow",
      "Action": ["iot:Receive"],
      "Resource": "arn:aws:iot:REGION:ACCOUNT_ID:topic/heartbox/*"
    }
  ]
}
```

Never use `"Action": "iot:*"` or `"Resource": "*"` in production policies.

---

## Certificate File Layout

Certificates are embedded at build time. They **must** exist at this path before running `idf.py build`:

```
certs/{DEVICE_NAME}/
├── AmazonRootCA1.pem           ← Amazon Root CA (download from AWS)
├── device_certificate.pem.crt  ← Device cert (rename from AWS download)
└── private_key.pem.key         ← Device private key (rename from AWS download)
```

**Naming is exact** — CMakeLists.txt validates these filenames. AWS downloads use different names; always rename them.

**Never commit real certificates.** The `certs/` directory is `.gitignore`d. Treat `.pem.key` files as secrets.

---

## AWS CLI: Provisioning a New Device

```bash
# 1. Create a Thing
aws iot create-thing --thing-name Heart_Box_1

# 2. Create certificate + keys (saves to files)
aws iot create-keys-and-certificate \
  --set-as-active \
  --certificate-pem-outfile certs/Heart_Box_1/device_certificate.pem.crt \
  --private-key-outfile certs/Heart_Box_1/private_key.pem.key \
  --public-key-outfile /dev/null

# 3. Note the certificateArn from the output above, then attach policy
aws iot attach-policy \
  --policy-name HeartBoxPolicy \
  --target <certificateArn>

# 4. Attach certificate to Thing
aws iot attach-thing-principal \
  --thing-name Heart_Box_1 \
  --principal <certificateArn>

# 5. Download Root CA (only needed once)
curl -o certs/Heart_Box_1/AmazonRootCA1.pem \
  https://www.amazontrust.com/repository/AmazonRootCA1.pem
```

---

## AWS CLI: Debugging & Inspection

```bash
# Get your endpoint URL (paste into menuconfig)
aws iot describe-endpoint --endpoint-type iot:Data-ATS

# Check certificate status (must be ACTIVE)
aws iot describe-certificate --certificate-id <id>

# List policies attached to a certificate
aws iot list-attached-policies --target <certificateArn>

# List Things a certificate is attached to
aws iot list-principal-things --principal <certificateArn>

# Monitor MQTT messages from CLI (requires mosquitto or AWS IoT MQTT test client)
aws iot-data publish \
  --topic heartbox/1 \
  --payload '{"client":"CLI","button":"pressed","duration_ms":500}'

# Check recent connection logs (requires IoT Core logging enabled)
aws logs filter-log-events \
  --log-group-name AWSIotLogsV2 \
  --filter-pattern "Heart_Box_1"
```

---

## Configuration Keys (menuconfig)

Set via `idf.py menuconfig` → **Example Configuration**:

| Key | Purpose | Example |
|-----|---------|---------|
| `HEARTBOX_DEVICE_NAME` | Cert path + MQTT client ID | `Heart_Box_1` |
| `HEARTBOX_TOPIC_NAME` | MQTT pub/sub topic | `heartbox/1` |
| `HEARTBOX_AWS_IOT_ENDPOINT` | Broker URL (from `describe-endpoint`) | `abc123-ats.iot.us-east-1.amazonaws.com` |

The **client identifier** passed to `aws_iot_connect()` must match the Thing name exactly — it's used to filter out self-published messages on the subscriber side.

---

## MQTT Message Format

All messages are JSON, published/subscribed at QoS 1:

```json
{
  "client": "Heart_Box_1",
  "button": "pressed",
  "duration_ms": 1250
}
```

- `client` — sender's client ID (used by receiver to ignore own messages)
- `button` — always `"pressed"` currently
- `duration_ms` — how long button was held; drives LED pulse duration on receiving device

When adding new message fields, update `aws_iot_handle_publish_packet()` in `aws_iot_task.c` and add corresponding cJSON parsing.

---

## Firmware Conventions

### Accessing Embedded Certificates
Certificates are accessed via linker symbols — never read from flash at a file path:

```c
extern const char root_cert_auth_start[] asm("_binary_AmazonRootCA1_pem_start");
extern const char root_cert_auth_end[]   asm("_binary_AmazonRootCA1_pem_end");
extern const char client_cert_start[]    asm("_binary_device_certificate_pem_crt_start");
extern const char client_key_start[]     asm("_binary_private_key_pem_key_start");
```

### Connection Sequence
The state machine drives the MQTT lifecycle — do not call these directly from `aws_iot_task_init()`:

```
wifi connected → aws_iot_connect(endpoint, client_id)
             → aws_iot_subscribe_to_topic(topic)
             → aws_iot_start_listening()
             → [operational: publish/receive]
```

### Adding New AWS IoT Commands
1. Add a new `eAppMsgType_t` value in `message_types.h`
2. Add a payload struct to `AwsIotMsg_t` union
3. Add a public API function that calls `generic_task_post_msg()`
4. Handle the new type in `aws_iot_on_message()`

---

## Troubleshooting

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| TLS handshake fails (`-0x7780`) | Certificate not Active, or wrong endpoint | Run `describe-certificate`, `describe-endpoint` |
| MQTT connect rejected (`-28`) | Policy not attached, or cert not attached to Thing | Run `list-attached-policies`, `list-principal-things` |
| Messages not received | Topic mismatch or subscription not confirmed | Check `HEARTBOX_TOPIC_NAME` on both devices |
| Build fails: "cert file not found" | Files missing or misnamed in `certs/{DEVICE_NAME}/` | Check exact filenames against required layout above |
| Device receives its own messages | `client` field not being compared correctly | Check `aws_iot_handle_publish_packet()` client ID filter |
