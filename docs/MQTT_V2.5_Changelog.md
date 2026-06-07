# MQTT V2.5 Changelog

## Runtime Topic Format

Firmware V2.5 uses the saved master class name as the MQTT topic prefix.
The runtime format is:

```text
<class_name>/<data_type>
```

For a master named `HD01`, the exact V2 topics are:

```text
HD01/suhu
HD01/co2
HD01/lux
HD01/human
HD01/led
HD01/ac
HD01/projector
HD01/master
```

Changing the class name in the master changes the prefix automatically. For
example, class `LA2` publishes to `LA2/suhu`, `LA2/co2`, and the other
`LA2/<data_type>` topics.

Actuator state and commands currently use the same literal topics:
`<class_name>/led`, `<class_name>/ac`, and `<class_name>/projector`. Firmware
ignores its own state payload shape to prevent a state publish from being
processed again as a command.

The legacy combined JSON compatibility topic remains `binus/ayam`.

## Default EMQX Deployment

```text
Deployment: SmartClass_serverless
Address: wd5de919.ala.asia-southeast1.emqxsl.com
MQTT TLS port: 8883
WebSocket TLS port: 8084
Username: Hansganteng
Password: 12345678
Publish interval: 5 seconds
```

The ESP32 firmware connects with MQTT over TLS on port `8883`. Port `8084` is
for WebSocket TLS clients such as web applications and is not used by the
firmware's PubSubClient connection.

## Compatibility Changes

- Replaced the previous space-separated topic labels with slash-separated exact
  runtime topics such as `HD01/suhu`.
- Added tracked EMQX defaults in `src/mqtt_defaults.h`.
- Kept `src/mqtt_secrets.h` as an optional ignored local override.
- Kept retained V2 state publishing and the legacy `binus/ayam` combined state.
