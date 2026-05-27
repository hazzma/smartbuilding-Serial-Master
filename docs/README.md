# Smart Building Master Documentation Index

Use these documents as the current source of truth:

- `FSD_Smart_Building_Master_UPDATED.md` - system requirements and current implementation alignment.
- `UIUX.md` - dashboard, empty state, widget, Slave Manager, and touch behavior.
- `Smart_Building_RS485_Modbus_Architecture.md` - active RS485 Modbus architecture.
- `Smart_Building_Connectivity_Dashboard_Mapping_Design_UPDATED.md` - Slave Manager, feature assignment, and dashboard mapping behavior.
- `Flutter_App_MQTT_Requirements.md` - Flutter app MQTT state/command JSON requirements.
- `From_SLave/RS485_Modbus_Slave_Firmware_Contract_V_1_4_0.md` - current agreed slave wire contract.

Important current rules:

- Slave Detail checkbox state is owned by the master and persisted in master NVS.
- Slave Detail rows are Available by default. Temperature N is Unavailable on other slaves only while Temperature N is checked on one slave.
- Slave capability/config follows the agreed slave contract: capability counts live at `0x0011..0x0016`, address assignment uses `0x00F0`, and SAVE_CONFIG uses `0x00F1 = 0xA55A`.
- Temperature mapping is fixed: Point 1 = Temp 1 / `0x0100`, Point 2 = Temp 2 / `0x0101`, Point 3 = Temp 3 / `0x0102`, Point 4 = Temp 4 / `0x0103`.
- CO2, presence, and relay registers follow the slave contract: CO2 `0x0110`, presence `0x0120`, relay 1-2 `0x0130..0x0131`.
- Current MQTT topics are configured from the master's MQTT setup or local firmware secret file; state publishes approximately every 5 seconds.

Legacy/reference documents:

- `RS485_agent_documentation_report.md` is an implementation report; use the architecture and FSD files above for normative behavior.
