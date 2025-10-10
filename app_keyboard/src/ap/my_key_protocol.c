#include "my_key_protocol.h"
#include "hw.h"
#include <string.h>

// Protocol constants
#define START_BYTE 0xAA

// Packet types
#define PACKET_TYPE_KEY 0x01
#define PACKET_TYPE_TRACKBALL 0x02
#define PACKET_TYPE_SYSTEM 0x03
#define PACKET_TYPE_BATTERY 0x04
#define PACKET_TYPE_HEARTBEAT 0x05

#define HEARTBEAT_TIMEOUT_MS     1500
#define CONNECTION_CHECK_INTERVAL 500

// Header size (Start + DeviceID + Version + PacketType + Length)
#define HEADER_SIZE 5
// Footer size (Checksum)
#define FOOTER_SIZE 1
// Max payload size
#define MAX_PAYLOAD 32
// Total max packet size
#define MAX_PACKET_SIZE (HEADER_SIZE + MAX_PAYLOAD + FOOTER_SIZE)


#ifndef LEFT_COLS
#define LEFT_COLS 1
#endif // LEFT_COLS
#ifndef RIGHT_COLS
#define RIGHT_COLS 1
#endif // RIGHT_COLS

// Buffer for storing received data
static uint8_t rx_buffer[MAX_PACKET_SIZE];

// Forward declarations
static bool parse_packet(void);
static bool validate_checksum(uint8_t *data, uint32_t length);
static void process_key_data(uint8_t device_id, uint8_t *payload, uint8_t length);
static void process_trackball_data(uint8_t device_id, uint8_t *payload, uint8_t length);
static void process_heartbeat_data(uint8_t device_id, uint8_t *payload, uint8_t length);
static void cli_command(cli_args_t *args);

// Debugging and statistics
static uint32_t tx_errors = 0;
static uint32_t rx_errors = 0;

// TX 관련 버퍼 및 변수
static uint8_t tx_buffer[MAX_PACKET_SIZE];
static uint32_t tx_packets = 0;

// 내부 Matrix 버퍼
static uint8_t rx_matrix[MATRIX_COLS] = {0};

// Trackball movement
int32_t x_movement = 0;
int32_t y_movement = 0;
bool is_moving = false;

typedef struct
{
    uint8_t device_id;
    uint8_t status_flag;
    uint8_t battery_level;
    uint32_t last_time;
    bool connected;
} heartbeat_state_t;

static heartbeat_state_t heartbeat_states[] =
{
    {DEVICE_ID_LEFT,  0u, 0u, 0u, false},
    {DEVICE_ID_RIGHT, 0u, 0u, 0u, false},
};

static uint32_t last_connection_check_time = 0u;

static heartbeat_state_t *get_heartbeat_state(uint8_t device_id)
{
    for (size_t i = 0; i < sizeof(heartbeat_states) / sizeof(heartbeat_states[0]); ++i)
    {
        if (heartbeat_states[i].device_id == device_id)
        {
            return &heartbeat_states[i];
        }
    }
    return NULL;
}

bool key_protocol_init(void)
{
    // Initialize the RF module
    if (!rfInit())
    {
        return false;
    }

    // Register CLI command for debugging
    cliAdd("keyproto", cli_command);

    return true;
}

void key_protocol_update(void)
{
    uint32_t rx_len = rfAvailable();
    bool is_error = false;
    // Check if RF has data available
    if (rx_len > 0)
    {
        if (rx_len > MAX_PACKET_SIZE)
            is_error = true;

        if (!rfRead(rx_buffer, rx_len))
            is_error = true;

        // Basic validation
        if (rx_buffer[0] != START_BYTE)
            is_error = true;

        // Checksum validation
        if (!validate_checksum(rx_buffer, rx_len))
            is_error = true;


        if (is_error)
        {
            rx_errors++;
            rfBufferFlush();
        }
        else
        {
            // Parse the packet
            parse_packet();
        }

    }

    uint32_t current_time = millis();
    if (current_time - last_connection_check_time >= CONNECTION_CHECK_INTERVAL)
    {
        last_connection_check_time = current_time;
        for (size_t i = 0; i < sizeof(heartbeat_states) / sizeof(heartbeat_states[0]); ++i)
        {
            heartbeat_state_t *state = &heartbeat_states[i];
            if (state->connected && (current_time - state->last_time > HEARTBEAT_TIMEOUT_MS))
            {
                state->connected = false;
                // logPrintf("Device 0x%02X disconnected (no heartbeat %ums)\n",
                //           state->device_id, current_time - state->last_time);
                memset(rx_matrix, 0, sizeof(rx_matrix));
                x_movement = 0;
                y_movement = 0;
                is_moving = false;
            }
        }
    }
}

static bool parse_packet(void)
{
    // Extract packet info
    uint8_t device_id = rx_buffer[1];
    // uint8_t version = rx_buffer[2];
    uint8_t packet_type = rx_buffer[3];
    uint8_t payload_length = rx_buffer[4];
    uint8_t *payload = &rx_buffer[5];

    // Process based on packet type
    switch (packet_type)
    {
    case PACKET_TYPE_KEY:
        process_key_data(device_id, payload, payload_length);
        break;

    case PACKET_TYPE_TRACKBALL:
        process_trackball_data(device_id, payload, payload_length);
        break;

    case PACKET_TYPE_SYSTEM:
    case PACKET_TYPE_BATTERY:
        // Not implemented yet
        break;

    case PACKET_TYPE_HEARTBEAT:
        process_heartbeat_data(device_id, payload, payload_length);
        break;

    default:
        // Unknown packet type
        return false;
    }

    return true;
}

static bool validate_checksum(uint8_t *data, uint32_t length)
{
    if (length < 2)
    {
        return false;
    }

    uint8_t checksum = 0;

    // Calculate XOR checksum of all bytes except the last one (which is the checksum)
    for (uint32_t i = 0; i < length - 1; i++)
    {
        checksum ^= data[i];
    }

    // Compare with the received checksum
    return (checksum == data[length - 1]);
}

static void process_key_data(uint8_t device_id, uint8_t *payload, uint8_t length)
{
    if (device_id == DEVICE_ID_LEFT)
    {
        if (length > LEFT_COLS)
            length = LEFT_COLS;
        memcpy(&rx_matrix[0], payload, length);
    }
    else if (device_id == DEVICE_ID_RIGHT)
    {
        if (length > RIGHT_COLS)
            length = RIGHT_COLS;
        memcpy(&rx_matrix[LEFT_COLS], payload, length);
    }
    else
    {
        return;
    }
}

void RfKeysReadBuf(uint8_t *buf, uint32_t len)
{
    memcpy(buf, rx_matrix, len);
}

static void process_trackball_data(uint8_t device_id, uint8_t *payload, uint8_t length)
{
    // Make sure we have enough data for X and Y (2 bytes each)
    if (length < 4)
    {
        return;
    }

    // 부호 있는 16비트 정수로 올바르게 변환 (little-endian)
    // int16_t는 부호 확장이 필요함
    int16_t x_val = (int16_t)((payload[1] << 8) | payload[0]);
    int16_t y_val = (int16_t)((payload[3] << 8) | payload[2]);

    // 값을 int32_t로 저장하되, 부호 확장이 올바르게 처리되도록 함
    x_movement = (int32_t)x_val;
    y_movement = (int32_t)y_val;
    is_moving = true;
}

static void process_heartbeat_data(uint8_t device_id, uint8_t *payload, uint8_t length)
{
    if (length < 2u)
    {
        return;
    }

    heartbeat_state_t *state = get_heartbeat_state(device_id);
    if (state == NULL)
    {
        return;
    }

    bool was_connected = state->connected;

    state->status_flag   = payload[0];
    state->battery_level = payload[1];
    state->last_time     = millis();
    state->connected     = true;

    // if (!was_connected)
    // {
    //     logPrintf("Device 0x%02X connected (battery %u%%)\n",
    //               state->device_id, state->battery_level);
    // }
}

bool RfMotionRead(int32_t *x, int32_t *y)
{
    if (is_moving)
    {
        *x = x_movement;
        *y = y_movement;
        is_moving = false; // Reset movement flag after reading
        return true;
    }
    return false;
}

// 패킷 조립 및 전송 함수
static bool tx_packet_prepare(uint8_t device_id, uint8_t packet_type, uint8_t *payload, uint8_t length)
{
    // uint8_t packet_length = HEADER_SIZE + length + FOOTER_SIZE;
    uint8_t checksum = 0;
    uint8_t i;

    if (length > MAX_PAYLOAD)
    {
        tx_errors++;
        return false;
    }

    // 헤더 구성
    tx_buffer[0] = START_BYTE;  // Start byte
    tx_buffer[1] = device_id;   // Device ID
    tx_buffer[2] = 0x01;        // Version (현재 0x01 고정)
    tx_buffer[3] = packet_type; // Packet type
    tx_buffer[4] = length;      // Payload length

    // 페이로드 복사
    memcpy(&tx_buffer[HEADER_SIZE], payload, length);

    // 체크섬 계산 (XOR)
    for (i = 0; i < HEADER_SIZE + length; i++)
    {
        checksum ^= tx_buffer[i];
    }

    // 체크섬 추가
    tx_buffer[HEADER_SIZE + length] = checksum;

    return true;
}

// 키 데이터 전송 함수
bool key_protocol_send_key_data(uint8_t device_id, uint8_t *key_matrix, uint8_t column_count)
{
    if (column_count > MAX_PAYLOAD - 1)
    {
        tx_errors++;
        return false;
    }

    // 페이로드 준비: column count + key states
    uint8_t payload[MAX_PAYLOAD];
    payload[0] = column_count;
    memcpy(&payload[1], key_matrix, column_count);

    // 패킷 조립
    if (!tx_packet_prepare(device_id, PACKET_TYPE_KEY, payload, column_count + 1))
    {
        return false;
    }

    // 패킷 전송
    uint32_t packet_length = HEADER_SIZE + (column_count + 1) + FOOTER_SIZE;
    uint32_t sent_len = rfWrite(tx_buffer, packet_length);

    if (sent_len == packet_length)
    {
        tx_packets++;
        return true;
    }
    else
    {
        tx_errors++;
        return false;
    }
}

// 트랙볼 데이터 전송 함수
bool key_protocol_send_trackball_data(uint8_t device_id, int16_t x, int16_t y)
{
    uint8_t payload[4];

    // 디버그 출력 추가 (필요시)
    // logPrintf("Sending trackball data: x=%d, y=%d\n", x, y);

    // Little-endian으로 X, Y 좌표 저장
    // 16비트 값을 8비트 바이트 두 개로 분리
    payload[0] = (uint8_t)(x & 0xFF);        // 하위 바이트
    payload[1] = (uint8_t)((x >> 8) & 0xFF); // 상위 바이트
    payload[2] = (uint8_t)(y & 0xFF);        // 하위 바이트
    payload[3] = (uint8_t)((y >> 8) & 0xFF); // 상위 바이트

    // 패킷 조립
    if (!tx_packet_prepare(device_id, PACKET_TYPE_TRACKBALL, payload, 4))
    {
        return false;
    }

    // 패킷 전송
    uint32_t sent_len = rfWrite(tx_buffer, HEADER_SIZE + 4 + FOOTER_SIZE);

    if (sent_len == (HEADER_SIZE + 4 + FOOTER_SIZE))
    {
        tx_packets++;
        return true;
    }
    else
    {
        tx_errors++;
        return false;
    }
}

// 시스템 상태 전송 함수
bool key_protocol_send_system_data(uint8_t device_id, uint8_t *system_data, uint8_t length)
{
    // 패킷 조립
    if (!tx_packet_prepare(device_id, PACKET_TYPE_SYSTEM, system_data, length))
    {
        return false;
    }

    // 패킷 전송
    uint32_t sent_len = rfWrite(tx_buffer, HEADER_SIZE + length + FOOTER_SIZE);

    if (sent_len == (HEADER_SIZE + length + FOOTER_SIZE))
    {
        tx_packets++;
        return true;
    }
    else
    {
        tx_errors++;
        return false;
    }
}

// 배터리 정보 전송 함수
bool key_protocol_send_battery_data(uint8_t device_id, uint8_t battery_level)
{
    uint8_t payload = battery_level;

    // 패킷 조립
    if (!tx_packet_prepare(device_id, PACKET_TYPE_BATTERY, &payload, 1))
    {
        return false;
    }

    // 패킷 전송
    uint32_t sent_len = rfWrite(tx_buffer, HEADER_SIZE + 1 + FOOTER_SIZE);

    if (sent_len == (HEADER_SIZE + 1 + FOOTER_SIZE))
    {
        tx_packets++;
        return true;
    }
    else
    {
        tx_errors++;
        return false;
    }
}

// 하트비트 데이터 전송 함수
bool key_protocol_send_heartbeat(uint8_t device_id, uint8_t status_flag, uint8_t battery_level)
{
    uint8_t payload[2];

    payload[0] = status_flag;
    payload[1] = battery_level;

    if (!tx_packet_prepare(device_id, PACKET_TYPE_HEARTBEAT, payload, sizeof(payload)))
    {
        return false;
    }

    uint32_t sent_len = rfWrite(tx_buffer, HEADER_SIZE + sizeof(payload) + FOOTER_SIZE);
    if (sent_len == (HEADER_SIZE + sizeof(payload) + FOOTER_SIZE))
    {
        tx_packets++;
        return true;
    }

    tx_errors++;
    return false;
}

bool key_protocol_is_connected(uint8_t device_id)
{
    heartbeat_state_t *state = get_heartbeat_state(device_id);
    return (state != NULL) ? state->connected : false;
}

uint8_t key_protocol_get_battery_level(uint8_t device_id)
{
    heartbeat_state_t *state = get_heartbeat_state(device_id);
    return (state != NULL) ? state->battery_level : 0u;
}

uint8_t key_protocol_get_status_flag(uint8_t device_id)
{
    heartbeat_state_t *state = get_heartbeat_state(device_id);
    return (state != NULL) ? state->status_flag : 0u;
}

uint32_t key_protocol_get_last_heartbeat_elapsed(uint8_t device_id)
{
    heartbeat_state_t *state = get_heartbeat_state(device_id);
    if (state == NULL)
    {
        return HEARTBEAT_TIMEOUT_MS;
    }
    uint32_t now = millis();
    return (now >= state->last_time) ? (now - state->last_time) : 0u;
}

// cli_command 함수 수정 - TX 통계 추가
static void cli_command(cli_args_t *args)
{
    if (args->argc == 1 && args->isStr(0, "info"))
    {
        cliPrintf("Key Protocol Status\n");
        cliPrintf("-------------------\n");
        cliPrintf("Total TX packets: %u\n", tx_packets);
        cliPrintf("TX error packets: %u\n", tx_errors);
        cliPrintf("Total RX errors: %u\n", rx_errors);
        for (size_t i = 0; i < sizeof(heartbeat_states) / sizeof(heartbeat_states[0]); ++i)
        {
            heartbeat_state_t *state = &heartbeat_states[i];
            cliPrintf("Device 0x%02X - connected:%s status:0x%02X battery:%u%% last:%ums\n",
                      state->device_id,
                      state->connected ? "YES" : "NO",
                      state->status_flag,
                      state->battery_level,
                      key_protocol_get_last_heartbeat_elapsed(state->device_id));
        }
        cliPrintf("Heartbeat timeout: %ums\n", HEARTBEAT_TIMEOUT_MS);
        return;
    }

    // 새로운 테스트 명령어 추가
    if (args->argc == 2 && args->isStr(0, "test_tx"))
    {
        uint8_t test_type = (uint8_t)args->getData(1);
        bool result = false;

        switch (test_type)
        {
        case 1: // 키 데이터 테스트
        {
            uint8_t key_data[3] = {0x01, 0x02, 0x03};
            result = key_protocol_send_key_data(DEVICE_ID_LEFT, key_data, 3);
            break;
        }
        case 2: // 트랙볼 데이터 테스트
            result = key_protocol_send_trackball_data(DEVICE_ID_LEFT, 10, -5);
            break;
        case 3:                                                          // 배터리 정보 테스트
            result = key_protocol_send_battery_data(DEVICE_ID_LEFT, 85); // 85% 배터리
            break;
        default:
            cliPrintf("Invalid test type\n");
            break;
        }

        cliPrintf("TX Test %s\n", result ? "SUCCESS" : "FAILED");
        return;
    }

    // 트랙볼 테스트 명령어 확장
    if (args->argc == 4 && args->isStr(0, "test_trackball"))
    {
        int16_t x = (int16_t)args->getData(1);
        int16_t y = (int16_t)args->getData(2);
        uint8_t device = (uint8_t)args->getData(3);

        bool result = key_protocol_send_trackball_data(device, x, y);
        cliPrintf("Trackball TX Test (x=%d, y=%d, dev=%d): %s\n",
                  x, y, device, result ? "SUCCESS" : "FAILED");
        return;
    }

    // Show usage
    cliPrintf("keyproto info\n");
    cliPrintf("keyproto test_tx [1:key, 2:trackball, 3:battery]\n");
    cliPrintf("keyproto test_trackball [x] [y] [device_id]\n");
}