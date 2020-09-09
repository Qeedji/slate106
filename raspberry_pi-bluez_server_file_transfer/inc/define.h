#ifndef H_DEFINE
#define H_DEFINE

#include <stdint.h>

/* CONSTANT FOR eir_parse_name() and readflags() */
#define FLAGS_AD_TYPE 0x01
#define FLAGS_LIMITED_MODE_BIT 0x01
#define FLAGS_GENERAL_MODE_BIT 0x02
#define EIR_NAME_SHORT 0x08    /* shortened local name */
#define EIR_NAME_COMPLETE 0x09 /* complete local name */

/*GATT and GAP service*/
#define UUID_GAP 0x1800
#define UUID_GATT 0x1801

#define ERR_SUCCESS 0
#define ERR_ERR -1
#define EXIT_FAILURE_TIMEOUT 2

/*BLE SPS*/
#define SLATE_SERVICE_UUID "06000001-0000-0000-0000-0000004e4553"
#define CENTRAL_IDENTIFICATION_CHARACTERISTIC_UUID "00000000-0000-0000-4000-0000024e4553"
#define PERIPHERAL_AUTHENTICATION_CHARACTERISTIC_UUID "00000000-0000-0000-4000-0000034e4553"
#define MISC_CHARACTERISTIC_UUID "00000000-0000-0000-4000-0000054e4553"
#define BLE_SPS_IDENT_CHAR_VALUE_LENGTH 8
#define BLE_SPS_AUTHEN_CHAR_VALUE_LENGTH 4
#define BLE_SPS_MISC_CHAR_VALUE_LENGTH 1
static const uint8_t BLE_SPS_IDENT_CHAR_VALUE[BLE_SPS_IDENT_CHAR_VALUE_LENGTH] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06};
static const uint8_t BLE_SPS_AUTHEN_CHAR_VALUE[BLE_SPS_AUTHEN_CHAR_VALUE_LENGTH] = {0x00, 0x00, 0x00, 0x00};
static const uint8_t BLE_SPS_MISC_CHAR_VALUE[BLE_SPS_MISC_CHAR_VALUE_LENGTH] = {0x01};

/*MLDP SERVICE*/
#define BLE_MLDP_MAX_DATA_LEN 20
#define MLDP_DATA_CHARAC_UUID "00035b03-58e6-07dd-021a-08123a000301"
#define MLDP_SERVICE_UUID "00035b03-58e6-07dd-021a-08123a000300"
#define MLDP_CTRL_CHARAC_UUID "00035b03-58e6-07dd-021a-08123a0003ff"

#define ATT_CID 4
#define BDADDR_LE_PUBLIC 0x01
#define BT_SECURITY_LOW 1

/* PRINT TOOL*/
#define PRLOG(...) \
  printf(__VA_ARGS__);

#define COLOR_OFF "\x1B[0m"
#define COLOR_RED "\x1B[0;91m"
#define COLOR_GREEN "\x1B[0;92m"
#define COLOR_YELLOW "\x1B[0;93m"
#define COLOR_BLUE "\x1B[0;94m"
#define COLOR_MAGENTA "\x1B[0;95m"
#define COLOR_BOLDGRAY "\x1B[1;30m"
#define COLOR_BOLDWHITE "\x1B[1;37m"

/* DEBUG*/
#define PRLOG_DEBUG(...)                   \
  printf(COLOR_GREEN "[DEBUG]" COLOR_OFF); \
  printf(__VA_ARGS__);

#define PRLOG_ERROR(...)                 \
  printf(COLOR_RED "[ERROR]" COLOR_OFF); \
  printf(__VA_ARGS__);

#define HCI_OE_USER_ENDED_CONNECTION 0x13

static volatile int signal_received = 0; //used to handle SIGINT

/*MLDP service uuid*/
static uint128_t mldp_service_uuid = {0x00, 0x03, 0x5b, 0x03, 0x58, 0xe6, 0x07, 0xdd, 0x02, 0x1a, 0x08, 0x12, 0x3a, 0x00, 0x03, 0x00};
static uint128_t mldp_data_char_uuid = {0x00, 0x03, 0x5b, 0x03, 0x58, 0xe6, 0x07, 0xdd, 0x02, 0x1a, 0x08, 0x12, 0x3a, 0x00, 0x03, 0x01};
static uint128_t mldp_ctrl_char_uuid = {0x00, 0x03, 0x5b, 0x03, 0x58, 0xe6, 0x07, 0xdd, 0x02, 0x1a, 0x08, 0x12, 0x3a, 0x00, 0x03, 0xff};

#endif
