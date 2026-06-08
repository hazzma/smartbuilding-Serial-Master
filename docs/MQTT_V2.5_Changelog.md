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

Firmware V2.5 runtime publishes only the V2 per-topic model. The old combined
JSON compatibility topic `binus/ayam` is cleared as a retained message on MQTT
connect and is not published periodically.

## Payload Format

Runtime payloads are intentionally small:

| Topic example | Payload | Meaning |
|---|---|---|
| `HD01/suhu` | Integer | Average temperature in Celsius, rounded from valid temperature slots. `-1` means no valid temperature. |
| `HD01/co2` | Integer | CO2 ppm. |
| `HD01/lux` | Integer | Lux value. |
| `HD01/human` | Integer | `1` means presence detected, `0` means no presence. |
| `HD01/led` | Integer | `1` means any mapped LED/relay is ON, `0` means all mapped LED/relay outputs are OFF. |
| `HD01/projector` | Integer | `1` means projector ON command/state, `0` means OFF command/state. |
| `HD01/ac` | 8-digit integer string | AC command/state encoded as `PPTTFFSS`. Target temperature is clamped to `16..30` degrees Celsius. |

The firmware also accepts scalar command payloads on `HD01/led` and
`HD01/projector`: `1`, `0`, `on`, `off`, `true`, or `false`. JSON command
payloads are still accepted for compatibility.

### AC 8-Digit Payload Draft

The next AC MQTT contract uses one compact 8-digit decimal payload:

```text
PPTTFFSS
```

Field meaning:

| Field | Digits | Values |
|---|---:|---|
| `PP` | 2 | AC power: `00` off, `01` on. |
| `TT` | 2 | Target temperature in Celsius: `16..30`. |
| `FF` | 2 | Fan speed enum. |
| `SS` | 2 | Swing/vertical vane enum. |

Examples:

```text
01240201
```

Means AC on, target `24` C, fan speed `02`, swing mode `01`.

```text
00240000
```

Means AC off, remembered/desired target `24` C, fan auto/default, swing off/fixed.

Fan speed enum:

| Value | Meaning |
|---|---|
| `00` | Auto |
| `01` | Low |
| `02` | Medium |
| `03` | High |
| `04` | Quiet/Silent |
| `05` | Turbo/Powerful |
| `06..98` | Reserved |
| `99` | No change / unsupported |

Swing enum:

| Value | Meaning |
|---|---|
| `00` | Off / fixed |
| `01` | Auto swing |
| `02` | Up |
| `03` | Mid-up |
| `04` | Middle |
| `05` | Mid-down |
| `06` | Down |
| `07` | Step next |
| `08` | Step previous |
| `09` | Auto comfort |
| `10` | Auto powerful |
| `11..98` | Reserved |
| `99` | No change / unsupported |

Implementation note: master firmware now publishes and accepts `HD01/ac` in
this `PPTTFFSS` format. Legacy AC JSON parsing remains as a transition fallback,
but new clients should use the 8-digit payload.

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
- Kept retained V2 state publishing.
- Stopped periodic legacy `binus/ayam` combined-state publishing.
