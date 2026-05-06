# esp32_wifi2iic

Expose an I²C bus over HTTP using an ESP32 and Protocol Buffers.

## Purpose

**esp32_wifi2iic** gives any I²C device a WiFi interface without requiring you to write embedded firmware.  
Flash the firmware to an ESP32, connect your I²C sensor or peripheral to the SDA/SCL pins, and interact with it from any machine on the same network using plain HTTP requests carrying serialized Protocol Buffer messages.

Typical use-cases:
- Rapid prototyping with I²C sensors from a PC/laptop.
- Remote monitoring or data-logging without a dedicated embedded application.
- Integration testing of I²C peripherals from a host test suite.

---

## Building

The project relies on the **ESP-IDF** toolchain.  
A Docker image bundling the exact toolchain is provided so that no manual environment setup is required.

### Prerequisites

- [Docker](https://docs.docker.com/get-docker/) installed and running.
- The repository cloned: `git clone <repo-url>`

> The ESP-IDF toolchain is bundled inside the Docker image (based on `espressif/idf:latest`).  
> No submodules or local IDF installation are needed.

### Option A — VS Code Dev Container (recommended)

1. Open the repository folder in VS Code.
2. When prompted, click **"Reopen in Container"** (or run **Dev Containers: Reopen in Container** from the command palette).  
   VS Code will build the image defined in `docker/dev.dockerfile` and open a fully configured shell.
3. Inside the container, configure and build:

```bash
# from the repository root inside the container
source /opt/esp/idf/export.sh

cmake -DCMAKE_BUILD_TYPE=Release \
      -DNETWORK_SSID="YourSSID" \
      -DNETWORK_PASSWORD="YourPassword" \
      -S /workspace -B /build

cmake --build /build -j$(nproc)
```

### Option B — Plain Docker commands

```bash
# 1. Build the dev image (run once from the repo root)
docker build \
  --build-arg UID=$(id -u) \
  --build-arg GID=$(id -g) \
  --build-arg USERNAME=$(id -un) \
  --target builder \
  -t esp-builder:latest \
  -f docker/dev.dockerfile .

# 2. CMake configure
docker run -i \
  -v $(pwd):/workspace \
  -v $(pwd)/bin:/build \
  --entrypoint bash \
  esp-builder:latest \
  -c "source /opt/esp/idf/export.sh && \
      cmake -DCMAKE_BUILD_TYPE=Release \
            -DNETWORK_SSID=\"YourSSID\" \
            -DNETWORK_PASSWORD=\"YourPassword\" \
            -S /workspace -B /build"

# 3. Build
docker run -i \
  -v $(pwd):/workspace \
  -v $(pwd)/bin:/build \
  --entrypoint bash \
  esp-builder:latest \
  -c "source /opt/esp/idf/export.sh && cmake --build /build -j$(nproc)"
```

### CMake configuration arguments

| Argument | Default | Description |
|---|---|---|
| `NETWORK_SSID` | *(required)* | WiFi network name to join |
| `NETWORK_PASSWORD` | *(required)* | WiFi network password |
| `SERVICE_PORT` | `5656` | TCP port the HTTP service listens on |
| `I2C_SDA_GPIO` | `GPIO_NUM_33` | GPIO pin number for I²C SDA |
| `I2C_SCL_GPIO` | `GPIO_NUM_35` | GPIO pin number for I²C SCL |
| `I2C_SCL_SPEED_HZ` | `400000` | I²C clock frequency in Hz |

### Flashing

After a successful build, flash with the standard IDF tools (inside the container or after sourcing `export.sh`):

```bash
idf.py -p /dev/ttyUSB0 flash
```

---

## API

Once the ESP32 is running and connected to WiFi it serves the API on:

```
http://<device-ip>:<SERVICE_PORT>/iic
```

All request and response bodies are **binary-serialized Protocol Buffer** messages  
(content-type: `application/x-protobuf`).  
The `.proto` definition is in [`src/service.proto`](src/service.proto).

### Write — `POST /iic`

Writes bytes to an I²C device.

**Request** — `WifiI2CRelayWriteRequest`

| Field | Type | Description |
|---|---|---|
| `address` | `uint32` | 7-bit I²C device address |
| `timeout_ms` | `uint32` | Transaction timeout in milliseconds |
| `write_data` | `bytes` | Bytes to transmit to the device |

**Response** — `WifiI2CRelayResponse`

| Field | Type | Description |
|---|---|---|
| `result` | `Result` | `SUCCESS`, `BAD_REQUEST`, `TIMEOUT`, or `FAILURE` |
| `data` | `bytes` | Always empty for write transactions |

**Example** (using the test client):

```bash
cd test
python3 test_client.py http://192.168.1.42:5656/iic write 0x50 100 deadbeef
```

---

### Read — `GET /iic`

Reads bytes from an I²C device.  
The behaviour depends on whether `write_data` is populated:

| `write_data` | I²C operation performed |
|---|---|
| empty | plain read (`i2c_master_receive`) |
| non-empty | atomic write-then-read (`i2c_master_transmit_receive`) — write the bytes first, then read back |

The write-then-read mode is useful for register-addressed devices: set `write_data` to the register address and `read_size` to the number of bytes to read back.

**Request** — `WifiI2CRelayReadRequest`

| Field | Type | Description |
|---|---|---|
| `address` | `uint32` | 7-bit I²C device address |
| `timeout_ms` | `uint32` | Transaction timeout in milliseconds |
| `write_data` | `bytes` | Optional bytes to transmit before reading (leave empty for a plain read). Useful when the I²C device requires a register address to be written before the actual read — for example, MPU6050 sensors expect the target register to be sent first. When set, the write and the subsequent read are performed as a single atomic transaction **without a stop condition in between** (`i2c_master_transmit_receive`). |
| `read_size` | `uint32` | Number of bytes to read back (max 128) |

**Response** — `WifiI2CRelayResponse`

| Field | Type | Description |
|---|---|---|
| `result` | `Result` | `SUCCESS`, `BAD_REQUEST`, `TIMEOUT`, or `FAILURE` |
| `data` | `bytes` | Bytes received from the device (valid only on `SUCCESS`) |

**Examples** (using the test client):

```bash
cd test

# Plain read — receive 4 bytes from device 0x50
python3 test_client.py http://192.168.1.42:5656/iic read 0x50 100 4

# Write-then-read — send register address 0x00, read 2 bytes back
python3 test_client.py http://192.168.1.42:5656/iic read 0x50 100 2 --write-data 00
```

---

### Result codes

| Value | Meaning |
|---|---|
| `SUCCESS` | Transaction completed successfully |
| `BAD_REQUEST` | Malformed request or `read_size` exceeds 128 bytes |
| `TIMEOUT` | Device did not respond within `timeout_ms` |
| `FAILURE` | Transaction failed for another reason |

---

### Customising buffer sizes

The maximum byte-field sizes used by the nanopb-generated serialiser are defined in [`src/service.options`](src/service.options):

```
service_api.WifiI2CRelayWriteRequest.write_data max_size:128
service_api.WifiI2CRelayReadRequest.write_data  max_size:128
service_api.WifiI2CRelayResponse.data           max_size:128
```

Edit the `max_size` values to match your needs, then rebuild the firmware — the new limits will be picked up automatically during the cmake build step.

---

## Test client

A ready-made Python test client is provided in [`test/test_client.py`](test/test_client.py).

```bash
pip install protobuf requests
cd test
python3 test_client.py --help
```

To regenerate the Python protobuf bindings after changing `src/service.proto`:

```bash
pip install grpcio-tools
python3 -m grpc_tools.protoc -I src --python_out=test src/service.proto
```
