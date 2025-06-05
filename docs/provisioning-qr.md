# Provisioning QR Codes

This document describes the QR code formats understood by `hid2mqtt` for provisioning and device management.

## Wi-Fi setup

Use the standard `WIFI:` QR format as defined by Android. Example:

```
WIFI:T:WPA;S:ExampleSSID;P:secretpass;;
```

## MQTT setup

```
MQTT:U:<uri>;T:<topic>;;
```

- `U` – The MQTT broker URI (e.g. `mqtt://192.168.1.10`)
- `T` – Topic under which barcode data will be published

## Locking configuration

To prevent unwanted reconfiguration, the device can be locked using

```
LOCK:<key>
```

While locked, provisioning QR codes are ignored. The only accepted code is

```
UNLOCK:<key>
```

The key must match the one used when locking the configuration.

## OTA updates

```
OTA:<url>
```

Triggers a firmware update from the given HTTPS URL. The device will reboot
after applying the update.
