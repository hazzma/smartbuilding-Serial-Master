#ifndef RS485_MANAGER_H
#define RS485_MANAGER_H

#include <Arduino.h>

#define RS485_UART_NUM 0
#define RS485_TX_PIN  TX
#define RS485_RX_PIN  RX
#define RS485_DIR_PIN 21

#define RS485_BAUDRATE 19200
#define RS485_MASTER_ADDR 0x01
#define RS485_BROADCAST_ADDR 0x00
#define RS485_MODBUS_PAIRING_ADDR 247

#define RS485_MAX_PAYLOAD 32

#define RS485_MODBUS_REG_NODE_ADDRESS      0x0000
#define RS485_MODBUS_REG_FW_VERSION        0x0001
#define RS485_MODBUS_REG_MAC_0_1           0x0002
#define RS485_MODBUS_REG_MAC_2_3           0x0003
#define RS485_MODBUS_REG_MAC_4_5           0x0004
#define RS485_MODBUS_IDENTITY_REGS         5
#define RS485_MODBUS_REG_TEMP_ASSIGNMENT   0x0010
#define RS485_MODBUS_REG_LUX_ASSIGNMENT    0x0011
#define RS485_MODBUS_REG_CO2_COUNT         0x0012
#define RS485_MODBUS_REG_PRESENCE_ASSIGNMENT 0x0013
#define RS485_MODBUS_REG_RELAY_ASSIGNMENT  0x0014
#define RS485_MODBUS_REG_IR_PROJECTOR_ENABLE 0x0015
#define RS485_MODBUS_REG_IR_AC_1_ENABLE    0x0016
#define RS485_MODBUS_REG_IR_AC_2_ENABLE    0x0017
#define RS485_MODBUS_CAPABILITY_ASSIGN_REGS 8
#define RS485_MODBUS_REG_CONFIG_VERSION    0x00F0
#define RS485_MODBUS_REG_LAST_ERROR        0x00F1
#define RS485_MODBUS_REG_UPTIME_LOW        0x00F2
#define RS485_MODBUS_REG_UPTIME_HIGH       0x00F3
#define RS485_MODBUS_REG_RECOVERY_MAC_0_1  0x00F4
#define RS485_MODBUS_REG_RECOVERY_MAC_2_3  0x00F5
#define RS485_MODBUS_REG_RECOVERY_MAC_4_5  0x00F6
#define RS485_MODBUS_REG_RECOVERY_ADDRESS  0x00F7
#define RS485_MODBUS_REG_TEMP_1_X10        0x0100
#define RS485_MODBUS_REG_TEMP_2_X10        0x0101
#define RS485_MODBUS_REG_TEMP_3_X10        0x0102
#define RS485_MODBUS_REG_TEMP_4_X10        0x0103
#define RS485_MODBUS_REG_LUX_1_LX          0x0104
#define RS485_MODBUS_REG_LUX_2_LX          0x0105
#define RS485_MODBUS_REG_LUX_3_LX          0x0106
#define RS485_MODBUS_REG_LUX_4_LX          0x0107
#define RS485_MODBUS_REG_CO2_PPM           0x0108
#define RS485_MODBUS_REG_PRESENCE_1_STATE  0x0109
#define RS485_MODBUS_REG_PRESENCE_2_STATE  0x010A
#define RS485_MODBUS_REG_PRESENCE_3_STATE  0x010B
#define RS485_MODBUS_REG_PRESENCE_4_STATE  0x010C
#define RS485_MODBUS_REG_RELAY_1           0x010D
#define RS485_MODBUS_REG_RELAY_2           0x010E
#define RS485_MODBUS_SENSOR_BLOCK_REGS     15
#define RS485_MODBUS_REG_AC_1_POWER        0x0200
#define RS485_MODBUS_REG_AC_1_SET_TEMP     0x0201
#define RS485_MODBUS_REG_AC_1_MODE         0x0202
#define RS485_MODBUS_REG_AC_2_POWER        0x0203
#define RS485_MODBUS_REG_AC_2_SET_TEMP     0x0204
#define RS485_MODBUS_REG_AC_2_MODE         0x0205
#define RS485_MODBUS_REG_AC_1_COMMAND_STATUS 0x0206
#define RS485_MODBUS_REG_AC_2_COMMAND_STATUS 0x0207
#define RS485_MODBUS_REG_AC_1_FAN_SPEED    0x0208
#define RS485_MODBUS_REG_AC_1_SWING_VERTICAL 0x0209
#define RS485_MODBUS_REG_AC_1_SWING_HORIZONTAL 0x020A
#define RS485_MODBUS_REG_AC_2_FAN_SPEED    0x020B
#define RS485_MODBUS_REG_AC_2_SWING_VERTICAL 0x020C
#define RS485_MODBUS_REG_AC_2_SWING_HORIZONTAL 0x020D
#define RS485_MODBUS_REG_PROJECTOR_POWER   0x0210
#define RS485_MODBUS_REG_PROJECTOR_INPUT   0x0211
#define RS485_MODBUS_REG_PROJECTOR_COMMAND_STATUS 0x0212
#define RS485_MODBUS_REG_AC_POWER          RS485_MODBUS_REG_AC_1_POWER
#define RS485_MODBUS_REG_AC_SET_TEMP       RS485_MODBUS_REG_AC_1_SET_TEMP
#define RS485_MODBUS_REG_AC_MODE           RS485_MODBUS_REG_AC_1_MODE
#define RS485_MODBUS_COMMAND_STATUS_IDLE   0
#define RS485_MODBUS_COMMAND_STATUS_SUCCESS 1
#define RS485_MODBUS_COMMAND_STATUS_BUSY   2
#define RS485_MODBUS_COMMAND_STATUS_FAILED 3
#define RS485_MODBUS_TEMP_ERROR_SENTINEL   0x8000
#define RS485_MODBUS_TEMP_UNASSIGNED       0x8001
#define RS485_MODBUS_U16_ERROR_SENTINEL    0xFFFF
#define RS485_MODBUS_U16_UNASSIGNED        0xFFFE

#define RS485_CAP_TEMP          (1 << 0)
#define RS485_CAP_CO2           (1 << 1)
#define RS485_CAP_PRESENCE      (1 << 2)
#define RS485_CAP_AC_IR         (1 << 3)
#define RS485_CAP_PROJECTOR_IR  (1 << 4)
#define RS485_CAP_LIGHT_RELAY   (1 << 5)
#define RS485_CAP_LUX           (1 << 6)
#define RS485_CAP_LCD_CTRL      (1 << 7)

enum RS485MessageType : uint8_t {
    RS485_TYPE_REQUEST  = 0x01,
    RS485_TYPE_RESPONSE = 0x02,
    RS485_TYPE_ACK      = 0x03,
    RS485_TYPE_NACK     = 0x04,
    RS485_TYPE_ERROR    = 0x05,
    RS485_TYPE_EVENT    = 0x06
};

enum RS485Command : uint8_t {
    RS485_CMD_PING           = 0x01,
    RS485_CMD_GET_INFO       = 0x02,
    RS485_CMD_READ_SENSOR    = 0x03,
    RS485_CMD_SET_OUTPUT     = 0x04,
    RS485_CMD_GET_CONFIG     = 0x05,
    RS485_CMD_SET_CONFIG     = 0x06,
    RS485_CMD_REBOOT         = 0x08,
    RS485_CMD_PAIRING_START  = 0x20,
    RS485_CMD_PAIRING_HELLO  = 0x21,
    RS485_CMD_ASSIGN_ADDRESS = 0x22,
    RS485_CMD_SET_ROLE       = 0x23,
    RS485_CMD_PAIRING_DONE   = 0x24
};

enum RS485RxResult : uint8_t {
    RS485_RX_OK = 0,
    RS485_RX_TIMEOUT,
    RS485_RX_CRC_ERROR,
    RS485_RX_SEQ_MISMATCH,
    RS485_RX_CMD_MISMATCH,
    RS485_RX_ADDR_MISMATCH,
    RS485_RX_NACK,
    RS485_RX_ERROR
};

struct RS485Frame {
    uint8_t dst;
    uint8_t src;
    uint8_t type;
    uint8_t cmd;
    uint8_t seq;
    uint8_t len;
    uint8_t payload[RS485_MAX_PAYLOAD];
};

struct RS485TransactionResult {
    bool ok;
    RS485RxResult rx_result;
    uint8_t dst;
    uint8_t cmd;
    uint8_t seq;
    uint8_t attempts;
    uint8_t response_type;
    uint8_t error_code;
};

void rs485_manager_init();
void rs485_task_init();
void rs485_manager_loop();
bool rs485_send_request(uint8_t dst, uint8_t cmd, const uint8_t* payload, uint8_t len, RS485Frame* response);
RS485TransactionResult rs485_send_transaction(uint8_t dst, uint8_t cmd, const uint8_t* payload, uint8_t len, RS485Frame* response);
void rs485_request_pairing();
void rs485_cancel_pairing();
void rs485_request_assign_pairing_candidate(uint8_t address);
void rs485_set_poll_enabled(bool enabled);
void rs485_request_test(uint8_t address, uint8_t cmd, bool write_command);
bool rs485_apply_slave_assignments(uint8_t slave_index);
void rs485_request_light_command(bool on);
void rs485_request_ac_command(bool power, float target_c, uint8_t mode = 0, uint8_t fan_speed = 0, uint8_t swing_mode = 0);
void rs485_request_projector_command(bool power, uint8_t input = 0);

#endif
