#include "my_key_protocol.h"
#include "hw.h"
#include "pointing_device.h"
#include <string.h>

#ifdef RF_DONGLE_MODE_ENABLE

// Protocol constants
#define START_BYTE      0xAA
#define DEVICE_ID_LEFT  0x01
#define DEVICE_ID_RIGHT 0x02

// Packet types
#define PACKET_TYPE_KEY       0x01
#define PACKET_TYPE_TRACKBALL 0x02
#define PACKET_TYPE_SYSTEM    0x03
#define PACKET_TYPE_BATTERY   0x04

// Header size (Start + DeviceID + Version + PacketType + Length)
#define HEADER_SIZE     5
// Footer size (Checksum)
#define FOOTER_SIZE     1
// Max payload size
#define MAX_PAYLOAD     32
// Total max packet size
#define MAX_PACKET_SIZE (HEADER_SIZE + MAX_PAYLOAD + FOOTER_SIZE)

// Buffer for storing received data
static uint8_t rx_buffer[MAX_PACKET_SIZE];
static uint32_t rx_index = 0;
static bool packet_received = false;
static bool is_parsing = false;

// Statistics
static uint32_t packet_total = 0;
static uint32_t packet_errors = 0;
static uint32_t key_packets = 0;
static uint32_t trackball_packets = 0;

// Last received packet info for debugging
static uint8_t last_device_id = 0;
static uint8_t last_packet_type = 0;
static uint8_t last_payload_length = 0;

// Forward declarations
static bool parse_packet(void);
static bool validate_checksum(uint8_t *data, uint32_t length);
static void process_key_data(uint8_t device_id, uint8_t *payload, uint8_t length);
static void process_trackball_data(uint8_t device_id, uint8_t *payload, uint8_t length);
static void cli_command(cli_args_t *args);

// 내부 Matrix 버퍼
static uint8_t rx_matrix[MATRIX_COLS] = {0};

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
    uint8_t data;
    uint32_t rx_len;
    
    // Check if RF has data available
    if (rfAvailable() > 0)
    {
        // Read one byte at a time to properly parse the protocol
        rx_len = rfRead(&data, 1);
        
        if (rx_len == 1)
        {
            // Check if we're waiting for a start byte
            if (!is_parsing)
            {
                // If we received a start byte, begin parsing
                if (data == START_BYTE)
                {
                    rx_buffer[0] = START_BYTE;
                    rx_index = 1;
                    is_parsing = true;
                }
            }
            else
            {
                // If we're already parsing, add the byte to the buffer
                if (rx_index < MAX_PACKET_SIZE)
                {
                    rx_buffer[rx_index++] = data;
                    
                    // Check if we've received the header
                    if (rx_index == HEADER_SIZE)
                    {
                        // Get the payload length
                        uint8_t payload_length = rx_buffer[4];
                        
                        // Validate payload length
                        if (payload_length > MAX_PAYLOAD)
                        {
                            // Invalid payload length, reset and wait for next packet
                            is_parsing = false;
                            rx_index = 0;
                            packet_errors++;
                        }
                    }
                    // Check if we've received the complete packet
                    else if (rx_index >= HEADER_SIZE && 
                            (rx_index == (HEADER_SIZE + rx_buffer[4] + FOOTER_SIZE)))
                    {
                        // We have a complete packet, parse it
                        packet_received = true;
                        is_parsing = false;
                        
                        // Process the packet
                        if (parse_packet())
                        {
                            packet_total++;
                        }
                        else
                        {
                            packet_errors++;
                        }
                        
                        // Reset for next packet
                        rx_index = 0;
                    }
                }
                else
                {
                    // Buffer overflow, reset and wait for next packet
                    is_parsing = false;
                    rx_index = 0;
                    packet_errors++;
                }
            }
        }
    }
}

static bool parse_packet(void)
{
    // Check minimum packet size
    if (rx_index < (HEADER_SIZE + FOOTER_SIZE))
    {
        return false;
    }
    
    // Extract packet info
    uint8_t device_id = rx_buffer[1];
    uint8_t version = rx_buffer[2];
    uint8_t packet_type = rx_buffer[3];
    uint8_t payload_length = rx_buffer[4];
    uint8_t *payload = &rx_buffer[5];
    
    // Store for debugging
    last_device_id = device_id;
    last_packet_type = packet_type;
    last_payload_length = payload_length;
    
    // Validate checksum
    if (!validate_checksum(rx_buffer, rx_index))
    {
        return false;
    }
    
    // Process based on packet type
    switch (packet_type)
    {
        case PACKET_TYPE_KEY:
            process_key_data(device_id, payload, payload_length);
            key_packets++;
            break;
            
        case PACKET_TYPE_TRACKBALL:
            process_trackball_data(device_id, payload, payload_length);
            trackball_packets++;
            break;
            
        case PACKET_TYPE_SYSTEM:
        case PACKET_TYPE_BATTERY:
            // Not implemented yet
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
    if(device_id == DEVICE_ID_LEFT)
    {
        if(length > LEFT_COLS) length = LEFT_COLS;
        memcpy(&rx_matrix[0], payload, length);
    }
    else if(device_id == DEVICE_ID_RIGHT)
    {
        if(length > RIGHT_COLS) length = RIGHT_COLS;
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
    
    // Extract X and Y movement (little-endian int16_t)
    int16_t x_movement = (int16_t)((payload[1] << 8) | payload[0]);
    int16_t y_movement = (int16_t)((payload[3] << 8) | payload[2]);
    
    // Forward the trackball data to the pointing device
    // pmw3610_motion_update(x_movement, y_movement);
}

static void cli_command(cli_args_t *args)
{
    if (args->argc == 1 && args->isStr(0, "info"))
    {
        cliPrintf("Key Protocol Status\n");
        cliPrintf("-------------------\n");
        cliPrintf("Total packets: %u\n", packet_total);
        cliPrintf("Error packets: %u\n", packet_errors);
        cliPrintf("Key packets: %u\n", key_packets);
        cliPrintf("Trackball packets: %u\n", trackball_packets);
        cliPrintf("Last Device ID: 0x%02X\n", last_device_id);
        cliPrintf("Last Packet Type: 0x%02X\n", last_packet_type);
        cliPrintf("Last Payload Length: %u\n", last_payload_length);
        return;
    }
    
    // Show usage
    cliPrintf("keyproto info\n");
}

#endif