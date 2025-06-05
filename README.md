# hid2mqtt

**hid2mqtt** is an open-source firmware and hardware integration example that
bridges USB HID input devices (like barcode scanners) to MQTT over Wi-Fi. It is
designed for the Wemos S2-mini development board based on ESP32-S2.

This project includes:
- 🔌 Firmware to read USB HID input via OTG and publish it to an MQTT broker
- 📡 QR code–based provisioning of Wi-Fi and MQTT settings
- 🛠️ A documented example of modifying a Tera HW0007 wireless barcode scanner cradle

---

## How It Works

1. The ESP32-S2 reads barcode input from a USB HID device using its OTG port.
2. Configuration and management is done via QR codes scanned by the device:
   - Wi-Fi: standard `WIFI:` QR format
   - MQTT: custom `MQTT:` format
   - Firmware updates: `OTA:<url>`
   - Lock/Unlock: `LOCK:<key>` / `UNLOCK:<key>`
   See [`docs/provisioning-qr.md`](docs/provisioning-qr.md) for details.
3. The scanned barcodes are published to the configured MQTT topic unless they
   contain a provisioning command.

---

## Firmware Development

The firmware is written using [PlatformIO](https://platformio.org/) with the
**ESP-IDF backend**. You'll need a S2-mini board and a USB connection for
flashing and power.

To build and flash the firmware:

```bash
cd firmware
pio run -t upload
````

After this first flashing step you can use the OTA mechanism to deploy new
firmware images. Build the firmware, serve the resulting `firmware.bin` over
HTTP and show the device a matching `OTA:` QR code. The commands below use
widely available tools:

```bash
cd firmware
pio run
qrencode -t ANSIUTF8 "OTA:http://<your-ip>:8080/firmware.bin"
python3 -m http.server 8080 --directory .pio/build/lolin_s2_mini
```

Scan the printed QR code and the device will fetch the new firmware from your
HTTP server and restart once the update completes.

The Wemos S2-mini board doesn't expose any standard UART pins by default, and
the USB port is occupied by the USB-OTG connection to the HID device. For
debugging, UART0 is therefore rerouted to the accessible GPIO pins 16 (TX) and
18 (RX) next to GND. To open the serial monitor, connect a TTL UART interface
to these pins and run:

```bash
pio device monitor
```

---

## Example: Tera HW0007 Cradle Mod

One use case for `hid2mqtt` is integrating a commercial wireless barcode
scanner (like the **Tera HW0007**) into a smart home or automation setup. The
scanner normally transmits barcodes to a computer via a 2.4GHz USB receiver in
its charging cradle. By modifying the cradle to include an ESP32-S2 board, the
scanned barcodes can instead be published via MQTT.

See [`docs/tera-hw0007`](docs/tera-hw0007.md) for photos, wiring
instructions, and integration notes.

---

## Contributions

Contributions are welcome! If you use `hid2mqtt` with another scanner or HID
device, feel free to submit your example to the `examples/` directory.

---

## Contact

Maintained by [Stefan Siegel](https://github.com/ssiegel).
Feel free to open issues or pull requests.
