/**
 * Copyright (c) 2016, Innes SA,
 * All Rights Reserved
 *
 * The copyright notice above does not evidence any
 * actual or intended publication of such source code.
 */

/**
 * @file   	main.c
 * @brief  	main
 * @author 	K. AUDIERNE
 * @date 	2020-09-10
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/signalfd.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/time.h>
#include <regex.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include "l2cap.h"
#include "uuid.h"
#include "mainloop.h"
#include "util.h"
#include "att.h"
#include "queue.h"
#include "gatt-db.h"
#include "gatt-client.h"
#include "gatt-server.h"
#include "libe-kermit.h"
#include "fifo.h"
#include "file_transfer_task.h"
#include "define.h"

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

typedef enum
{
  BLE_SCANNING,
  BLE_SLATE_FOUND,
  BLE_SOCKET_OPEN,
  BLE_CONNECTED,
  BLE_SPS_DISCOVERING,
  BLE_ALL_SERVICE_DISCOVERY_COMPLETE,
  BLE_WAIT_MLDP_DATA,
  BLE_FILE_TRANSFER
} ble_connection_step;

ble_connection_step ble_con_step = -1;

struct client
{
  // pointer to a bt_gatt_client structure
  struct bt_gatt_client *gatt;

  // session id
  unsigned int reliable_session_id;

  // MLDP service handle
  uint16_t mldp_data_char_handle;
  uint16_t mldp_data_desc_handle;
  uint16_t mldp_ctrl_char_handle;
  struct gatt_db_attribute *mldp_data_char_attr;

  // SLATE SPS
  uint16_t central_identification_char_handle;
  uint16_t peripheral_authentication_char_handle;
  uint16_t misc_char_handle;
};

struct server
{
  struct bt_gatt_server *gatt;

  uint8_t *device_name;
  size_t name_len;
  uint16_t gatt_svc_chngd_handle;
  bool svc_chngd_enabled;

  // MLDP service handle
  struct gatt_db_attribute *mldp_data_char_attr;
  struct gatt_db_attribute *mldp_ctrl_char_attr;
  uint16_t mldp_service_handle;
  uint16_t mldp_data_char_handle;
  uint16_t mldp_data_desc_handle;
  uint16_t mldp_ctrl_char_handle;
};

struct gatt_central
{
  // socket file descriptor
  int fd;

  // pointer to a bt_att structure
  struct bt_att *att;

  // pointer to a gatt_db structure
  struct gatt_db *db_c; //client
  struct gatt_db *db_s; //server

  struct client cli;
  struct server srv;

  //identifier to the ft thread
  ft_t ft_s;

  fifo mldp_fifo_rx;
  fifo mldp_fifo_tx;
};

static struct gatt_central *m_gatt_central = NULL;
static void retry_scan(struct gatt_central *central);
static void sig_handler(int signum);
static int end_of_state_machine = 0;

/*-----------------------------------------------------------------------------
 * scan & connect functions
 *-----------------------------------------------------------------------------*/

static void eir_parse_name(uint8_t *eir, size_t eir_len, char *buf, size_t buf_len)
{
  size_t offset;

  offset = 0;
  while (offset < eir_len)
  {
    uint8_t field_len = eir[0];
    size_t name_len;

    /* Check for the end of EIR */
    if (field_len == 0)
      break;

    if (offset + field_len > eir_len)
      goto failed;

    switch (eir[1])
    {
    case EIR_NAME_SHORT:
    case EIR_NAME_COMPLETE:
      name_len = field_len - 1;
      if (name_len > buf_len)
        goto failed;

      memcpy(buf, &eir[2], name_len);
      return;
    }

    offset += field_len + 1;
    eir += field_len + 1;
  }

failed:
  snprintf(buf, buf_len, "(unknown)");
}

static int read_flags(uint8_t *flags, const uint8_t *data, size_t size)
{
  size_t offset;

  if (!flags || !data)
    return -EINVAL;

  offset = 0;
  while (offset < size)
  {
    uint8_t len = data[offset];
    uint8_t type;

    /* Check if it is the end of the significant part */
    if (len == 0)
      break;

    if (len + offset > size)
      break;

    type = data[offset + 1];

    if (type == FLAGS_AD_TYPE)
    {
      *flags = data[offset + 2];
      return 0;
    }

    offset += 1 + len;
  }

  return -ENOENT;
}

static int check_report_filter(uint8_t procedure, le_advertising_info *info)
{
  uint8_t flags;

  /* If no discovery procedure is set, all reports are treat as valid */
  if (procedure == 0)
    return 1;

  /* Read flags AD type value from the advertising report if it exists */
  if (read_flags(&flags, info->data, info->length))
    return 0;

  switch (procedure)
  {
  case 'l': /* Limited Discovery Procedure */
    if (flags & FLAGS_LIMITED_MODE_BIT)
      return 1;
    break;
  case 'g': /* General Discovery Procedure */
    if (flags & (FLAGS_LIMITED_MODE_BIT | FLAGS_GENERAL_MODE_BIT))
      return 1;
    break;
  default:
    PRLOG("Unknown discovery procedure\n");
  }

  return 0;
}

/** slate_research() --  parse the scan result
 * Input : addr -- address of the device to test
 * output : out_research_success-- true if SLATE is discovered, otherwise false
 **/
static int slate_research(char *addr, bool *out_research_success, char *slate_addr)
{
  *out_research_success = false;
  if (addr == NULL)
  {
    return ERR_ERR;
  }
  if (strcmp((char *)addr, slate_addr) == 0)
  {
    *out_research_success = true;
    return ERR_SUCCESS;
  }
}

/** scan_result() --  collect the name and address of the scanned devices
 * Input : dd -- identifier to the local adapter (hci0)
 *         filter_type -- defined if the scan collect only devices in the withelist
 *         slate_addr -- pointer to the SLATE MAC address on wich we want to connect
 * output : out_success -- true if SLATE is discovered, otherwise false. This is the result of slate_research.
 **/
static int scan_result(int dd, uint8_t filter_type, bool *out_success, char *slate_addr)
{
  unsigned char buf[HCI_MAX_EVENT_SIZE], *ptr;
  struct hci_filter nf, of;
  struct sigaction sa;
  socklen_t olen;
  int len;

  olen = sizeof(of);
  if (getsockopt(dd, SOL_HCI, HCI_FILTER, &of, &olen) < 0)
  {
    PRLOG_ERROR("Could not get socket options");
    return EXIT_FAILURE;
  }

  hci_filter_clear(&nf);
  hci_filter_set_ptype(HCI_EVENT_PKT, &nf);
  hci_filter_set_event(EVT_LE_META_EVENT, &nf);

  if (setsockopt(dd, SOL_HCI, HCI_FILTER, &nf, sizeof(nf)) < 0)
  {
    PRLOG_ERROR("Could not set socket options");
    return EXIT_FAILURE;
  }

  memset(&sa, 0, sizeof(sa));
  sa.sa_flags = SA_NOCLDSTOP;
  sa.sa_handler = sig_handler;
  sigaction(SIGINT, &sa, NULL); //listen if crt-c is pressed.

  while (1)
  {
    evt_le_meta_event *meta;
    le_advertising_info *info;
    char addr[18];

    while ((len = read(dd, buf, sizeof(buf))) < 0)
    {
      if (errno == EINTR && signal_received == SIGINT)
      {
        len = 0;
        goto done;
      }

      if (errno == EAGAIN || errno == EINTR)
        continue;
      goto done;
    }

    ptr = buf + (1 + HCI_EVENT_HDR_SIZE);
    len -= (1 + HCI_EVENT_HDR_SIZE);

    meta = (void *)ptr;

    if (meta->subevent != 0x02)
      goto done;

    /* Ignoring multiple reports */
    info = (le_advertising_info *)(meta->data + 1);
    if (check_report_filter(filter_type, info))
    {
      char name[30];

      memset(name, 0, sizeof(name));

      ba2str(&info->bdaddr, addr);
      eir_parse_name(info->data, info->length,
                     name, sizeof(name) - 1);
      bool research_slate_success = false;
      slate_research(addr, &research_slate_success, slate_addr);
      if (research_slate_success)
      {
        *out_success = true;
        return EXIT_SUCCESS;
      }
    }
  }

done:
  setsockopt(dd, SOL_HCI, HCI_FILTER, &of, sizeof(of));

  if (len < 0)
    return EXIT_FAILURE;

  return EXIT_SUCCESS;
}

/** cmd_lescan() --  start a BLE scan
 * Input : dev_id -- identifier to the local adapter (hci0)
 *         slate_addr -- pointer to the SLATE MAC address on wich we want to connect
 * Return : True if SLATE was found, else false
 **/
static void cmd_lescan(int dev_id, char *slate_addr)
{
  bool out_success = false;
  int err, dd;
  uint8_t own_type = LE_PUBLIC_ADDRESS;
  uint8_t scan_type = 0x01;
  uint8_t filter_type = 0;
  uint8_t filter_policy = 0x00;
  uint16_t interval = htobs(0x0010);
  uint16_t window = htobs(0x0010);
  uint8_t filter_dup = 0x01;

  if (dev_id < 0)
    dev_id = hci_get_route(NULL);

  dd = hci_open_dev(dev_id);
  if (dd < 0)
  {
    perror("Could not open device");
    exit(1);
  }

  err = hci_le_set_scan_parameters(dd, scan_type, interval, window,
                                   own_type, filter_policy, 10000);
  if (err < 0)
  {
    perror("Set scan parameters failed");
    exit(1);
  }

  err = hci_le_set_scan_enable(dd, 0x01, filter_dup, 10000);
  if (err < 0)
  {
    perror("Enable scan failed");
    exit(1);
  }

  PRLOG("LE Scan ...\n");

  err = scan_result(dd, filter_type, &out_success, slate_addr);

  if (err < 0)
  {
    perror("Could not receive advertising events");
    exit(1);
  }
  err = hci_le_set_scan_enable(dd, 0x00, filter_dup, 10000);
  if (err < 0)
  {
    perror("Disable scan failed");
    exit(1);
  }
  if (out_success)
  {
    hci_close_dev(dd);
    ble_con_step = BLE_SLATE_FOUND;
  }
}

/** le_connection() --  start a BLE connection
 * Input : dev_id -- identifier to the local adapter (hci0)
 *         slate_addr -- pointer to the SLATE MAC address on wich we want to connect
 **/
static void le_connection(int dev_id, char *slate_addr)
{
  char *argv;
  argv = slate_addr;
  int err, dd;
  bdaddr_t bdaddr;
  uint16_t interval, latency, max_ce_length, max_interval, min_ce_length;
  uint16_t min_interval, supervision_timeout, window, handle;
  uint8_t initiator_filter, own_bdaddr_type, peer_bdaddr_type;

  own_bdaddr_type = LE_PUBLIC_ADDRESS;
  peer_bdaddr_type = LE_PUBLIC_ADDRESS;
  initiator_filter = 0; /* Use peer address */

  if (dev_id < 0)
    dev_id = hci_get_route(NULL);

  dd = hci_open_dev(dev_id);
  if (dd < 0)
  {
    perror("Could not open device");
    exit(1);
  }

  memset(&bdaddr, 0, sizeof(bdaddr_t));
  if (argv)
    str2ba(argv, &bdaddr);

  interval = htobs(0x0006);
  window = htobs(0x0006);
  min_interval = htobs(0x0006);
  max_interval = htobs(0x0006);
  latency = htobs(0x0000);
  supervision_timeout = htobs(0x0C80);
  min_ce_length = htobs(0x0001);
  max_ce_length = htobs(0xffff);

  err = hci_le_create_conn(dd, interval, window, initiator_filter,
                           peer_bdaddr_type, bdaddr, own_bdaddr_type, min_interval,
                           max_interval, latency, supervision_timeout,
                           min_ce_length, max_ce_length, &handle, 25000);
  if (err < 0)
  {
    perror("Could not create connection");
    exit(1);
  }
  ble_con_step = BLE_CONNECTED;

  hci_close_dev(dd);
}

/** le_deconnection() --  Stop a BLE connection
 * Input : dev_id -- identifier to the local adapter (hci0)
 **/
static void le_deconnection(int dev_id)
{
  int err = 0, dd = 0, argc = 0;
  uint16_t handle;
  uint8_t reason;
  char **argv = NULL;

  if (dev_id < 0)
    dev_id = hci_get_route(NULL);

  dd = hci_open_dev(dev_id);
  if (dd < 0)
  {
    perror("Could not open device");
    exit(1);
  }

  handle = atoi(argv[0]);

  reason = (argc > 1) ? atoi(argv[1]) : HCI_OE_USER_ENDED_CONNECTION;

  err = hci_disconnect(dd, handle, reason, 10000);
  if (err < 0)
  {
    perror("Could not disconnect");
    exit(1);
  }

  hci_close_dev(dd);
}

/** sig_handler -- signal handler
 * Input: signum -- number of the signal received
 **/
static void sig_handler(int signum)
{
  int dev_id = hci_get_route(NULL);
  if (signum == SIGINT)
  {
    if (ble_con_step == BLE_SCANNING)
    {
      signal_received = signum;
      end_of_state_machine = 1;
    }
    else
    {
      le_deconnection(dev_id);
      end_of_state_machine = 1;
    }
  }
}

/*-----------------------------------------------------------------------------
 * Display client and server services, read characteristics data
 *-----------------------------------------------------------------------------*/

/** att_disconnect_cb() --  Callback function of bt_att_register_disconnect()
 * Explanation : Function called when the Bluetooth connection is stopped. Then it stop the mainloop.
 **/
static void att_disconnect_cb(int err, void *user_data)
{
  pthread_join(m_gatt_central->ft_s.ft_task_id, NULL); //wait the end of file transfer
  PRLOG("Device disconnected: %s\n", strerror(err));
  ble_con_step = BLE_SCANNING;
  mainloop_quit();
}

/** log_service_event() --  Log service information when an event occur on him (modification or supression)
 * Input : attr -- pointer the database attribute
 *         str -- pointer to a string of characters to print
 **/
static void log_service_event(struct gatt_db_attribute *attr, const char *str)
{
  char uuid_str[MAX_LEN_UUID_STR];
  bt_uuid_t uuid;
  uint16_t start, end;

  gatt_db_attribute_get_service_uuid(attr, &uuid);
  bt_uuid_to_string(&uuid, uuid_str, sizeof(uuid_str));

  gatt_db_attribute_get_service_handles(attr, &start, &end);
}

/** service_added_cb() --  Callback function of gatt_db_register()
 * Input : attr -- pointer the database attribute
 *         str -- pointer to a string of characters to print
 * Explanation: Called when  service is added to the Client database
 **/
static void service_added_cb(struct gatt_db_attribute *attr, void *user_data)
{
  log_service_event(attr, "Service Added");
}

/** service_added_cb() --  Callback function of gatt_db_register()
 * Input : attr -- pointer the database attribute
 *         str -- pointer to a string of characters to print
 * Explanation : called when a service is removed from the client database
 **/
static void service_removed_cb(struct gatt_db_attribute *attr, void *user_data)
{
  log_service_event(attr, "Service Removed");
}

/** att_debug_cb() --  debug ATT layer
 * Input: str -- string to print for debugging
 *        user_data -- prefix
 **/
static void att_debug_cb(const char *str, void *user_data)
{
  const char *prefix = user_data;

  PRLOG(COLOR_BOLDGRAY "%s" COLOR_BOLDWHITE "%s\n" COLOR_OFF, prefix, str);
}

/** gatt_debug_cb() --  debug GATT layer
 * Input: str -- string to print for debugging
 *        user_data -- prefix
 **/
static void gatt_debug_cb(const char *str, void *user_data)
{
  const char *prefix = user_data;

  PRLOG(COLOR_GREEN "%s%s\n" COLOR_OFF, prefix, str);
}

/** print_uuid() --  convert the uuid in string and print it
 * Input: uuid -- type : bt_uuid_t
 * Return:  /
 **/
static void print_uuid(const bt_uuid_t *uuid)
{
  char uuid_str[MAX_LEN_UUID_STR];
  bt_uuid_t uuid128;

  bt_uuid_to_uuid128(uuid, &uuid128);
  bt_uuid_to_string(&uuid128, uuid_str, sizeof(uuid_str));

  PRLOG("%s\n", uuid_str);
}

/** print_desc() --  Print descriptors of the client
 * Input: attr -- attribute of the data base
 *        user_data -- pointer to the central structure
 * Return:  /
 **/
static void print_desc(struct gatt_db_attribute *attr, void *user_data)
{
  PRLOG("\t\t  " COLOR_MAGENTA "descr" COLOR_OFF
        " - handle: 0x%04x, uuid: ",
        gatt_db_attribute_get_handle(attr));
  print_uuid(gatt_db_attribute_get_type(attr));
}

/** print_chrc() --  Print characteristics of the client
 * Input: attr -- attribute of the data base
 *        user_data -- pointer to the central structure
 * Return:  /
 **/
static void print_chrc(struct gatt_db_attribute *attr, void *user_data)
{
  uint16_t handle, value_handle;
  uint8_t properties;
  uint16_t ext_prop;
  bt_uuid_t uuid;

  if (!gatt_db_attribute_get_char_data(attr, &handle, &value_handle, &properties, &ext_prop, &uuid))
    return;

  PRLOG("\t  " COLOR_YELLOW "charac" COLOR_OFF
        " - start: 0x%04x, \b value: 0x%04x, "
        "props: 0x%02x, ext_props: 0x%04x, uuid: ",
        handle, value_handle, properties, ext_prop);
  print_uuid(&uuid);
  gatt_db_service_foreach_desc(attr, print_desc, NULL);
}

/** print_service() --  Print services of the client
 * Input: attr -- attribute of the data base
 *        user_data -- pointer to the central structure
 * Return:  /
 **/
static void print_service(struct gatt_db_attribute *attr, void *user_data)
{
  struct gatt_central *central = user_data;
  uint16_t start, end;
  bool primary;
  bt_uuid_t uuid;

  if (!gatt_db_attribute_get_service_data(attr, &start, &end, &primary,
                                          &uuid))
    return;

  PRLOG(COLOR_RED "service" COLOR_OFF " - start: 0x%04x, "
                  "end: 0x%04x, type: %s, uuid: ",
        start, end, primary ? "primary" : "secondary");
  print_uuid(&uuid);

  gatt_db_service_foreach_char(attr, print_chrc, central);
}

/** client_store_handle() --  callback function to store handle of characteristics
 * Input: attr -- attribute of the data base
 *        user_data -- pointer to the central structure
 * Return:  /
 * Explanation : Handles are stored in the client structure of the central.
 **/
static void client_store_handle(struct gatt_db_attribute *attr, void *user_data)
{
  uint16_t handle, value_handle, ext_prop;
  uint8_t properties;
  bt_uuid_t uuid, uuid1, uuid2, uuid3, uuid4, uuid5;
  struct gatt_central *central = user_data;
  ble_con_step = BLE_SPS_DISCOVERING;
  if (!gatt_db_attribute_get_char_data(attr, &handle, &value_handle, &properties, &ext_prop, &uuid))
    return;

  bt_string_to_uuid(&uuid1, CENTRAL_IDENTIFICATION_CHARACTERISTIC_UUID);
  bt_string_to_uuid(&uuid2, PERIPHERAL_AUTHENTICATION_CHARACTERISTIC_UUID);
  bt_string_to_uuid(&uuid3, MISC_CHARACTERISTIC_UUID);
  bt_string_to_uuid(&uuid4, MLDP_DATA_CHARAC_UUID);
  bt_string_to_uuid(&uuid5, MLDP_CTRL_CHARAC_UUID);

  if (bt_uuid_cmp(&uuid, &uuid1) == 0)
  {
    central->cli.central_identification_char_handle = value_handle;
  }

  if (bt_uuid_cmp(&uuid, &uuid2) == 0)
  {
    central->cli.peripheral_authentication_char_handle = value_handle;
  }
  if (bt_uuid_cmp(&uuid, &uuid3) == 0)
  {
    central->cli.misc_char_handle = value_handle;
  }
  if (bt_uuid_cmp(&uuid, &uuid4) == 0)
  {
    central->cli.mldp_data_char_handle = value_handle;
    central->cli.mldp_data_desc_handle = value_handle + 1;
  }
  if (bt_uuid_cmp(&uuid, &uuid5) == 0)
  {
    central->cli.mldp_ctrl_char_handle = value_handle;
  }
}

/** client_parse_chrc() -- acces to handles of characteristics
 * Input: attr -- attribute of the data base
 *        user_data -- pointer to the central structure
 * Output:  /
 * Return:  /
 * Explanation : For each characteristics, this function store handles by using client_store_handle().
 **/
static void client_parse_chrc(struct gatt_db_attribute *attr, void *user_data)
{
  struct gatt_central *central = user_data;
  uint16_t start, end;
  bool primary;
  bt_uuid_t uuid;

  if (!gatt_db_attribute_get_service_data(attr, &start, &end, &primary,
                                          &uuid))
    return;

  gatt_db_service_foreach_char(attr, client_store_handle, central);
}

/** get_handle_from_uuid() -- store the handle of each charateristics of the client.
 * Input: central -- pointer to the central structure
 * Output:  /
 * Return:  /
 * Explanation : This function analyzes each service in the database. For each of them, it launches the function client_parse_chrc()
 **/
static void get_handle_from_uuid(struct gatt_central *central)
{
  PRLOG("\n");
  gatt_db_foreach_service(central->db_c, NULL, client_parse_chrc, central);
}

/*-----------------------------------------------------------------------------
 * Write value in client BLE SPS
 *-----------------------------------------------------------------------------*/

/** client_write_cb_misc_char() -- Callback function of write_misc_char()
 * Input:   success -- true on success to write in misc char and false on fail
 *          att_ecode -- unused here
 *          user_data -- pointer to the central structure
 * Output:  /
 * Return:  /
 * Explanation : In case of error, process is stoped and scan restart
 **/
static void client_write_cb_misc_char(bool success, uint8_t att_ecode, void *user_data)
{
  struct gatt_central *central = user_data;
  if (!success)
  {
    retry_scan(central);
    return;
  }
  ble_con_step = BLE_WAIT_MLDP_DATA;
}

/** client_write_cb_misc_char() -- write in misc characteristic
 * Input:   central -- pointer to the central structure
 *          handle -- handle value of the misc characteristic
 *          value -- pointer to value to write
 *          length -- length of value (in bytes)
 * Return:  EXIT_SUCCESS on  success, EXIT_FAILURE on error
 **/
static int write_misc_char(struct gatt_central *central, uint16_t handle, const uint8_t *value, uint16_t length)
{
  if (bt_gatt_client_write_value(central->cli.gatt, central->cli.misc_char_handle, value, length, client_write_cb_misc_char, central, NULL) != 0)
    return EXIT_FAILURE;
  return EXIT_SUCCESS;
}

/** client_write_cb_authentication_char() -- Callback function of write_authentication_char()
 * Input:   success -- true on success to write in authentification char and false on fail
 *          att_ecode -- unused here
 *          user_data -- pointer to the central structure
 * Output:  /
 * Return:  /
 * Explanation : In case of error, process is stoped and scan restart
 **/
static void client_write_cb_authentication_char(bool success, uint8_t att_ecode, void *user_data)
{
  struct gatt_central *central = user_data;
  if (!success)
  {
    retry_scan(central);
    return;
  }
  write_misc_char(central, central->cli.misc_char_handle, BLE_SPS_MISC_CHAR_VALUE, BLE_SPS_MISC_CHAR_VALUE_LENGTH);
}

/** write_authentication_char() -- write in authentication characteristic
 * Input:   central -- pointer to the central structure
 *          handle -- handle value of the authentication characteristic
 *          value -- pointer to value to write
 *          length -- length of value (in bytes)
 * Return:  EXIT_SUCCESS on  success, EXIT_FAILURE on error
 **/
static int write_authentication_char(struct gatt_central *central, uint16_t handle, const uint8_t *value, uint16_t length)
{
  if (bt_gatt_client_write_value(central->cli.gatt, central->cli.peripheral_authentication_char_handle, value, length, client_write_cb_authentication_char, central, NULL) != 0)
    return EXIT_FAILURE;
  return EXIT_SUCCESS;
}

/** client_write_cb_identification_char() -- Callback function of write_identification_char()
 * Input:   success -- true on success to write in identification char and false on fail
 *          att_ecode -- unused here
 *          user_data -- pointer to the central structure
 * Output:  /
 * Return:  /
 * Explanation : In case of error, process is stoped and scan restart
 **/
static void client_write_cb_identification_char(bool success, uint8_t att_ecode, void *user_data)
{
  struct gatt_central *central = user_data;
  if (!success)
  {
    retry_scan(central);
    return;
  }
  write_authentication_char(central, central->cli.peripheral_authentication_char_handle, BLE_SPS_AUTHEN_CHAR_VALUE, BLE_SPS_AUTHEN_CHAR_VALUE_LENGTH);
}

/** write_identification_char() -- write in identification characteristic
 * Input:   central -- pointer to the central structure
 *          handle -- handle value of the identification characteristic
 *          value -- pointer to value to write
 *          length -- length of value (in bytes)
 * Return:  EXIT_SUCCESS on  success, EXIT_FAILURE on error
 **/
static int write_identification_char(struct gatt_central *central, uint16_t handle, const uint8_t *value, uint16_t length)
{
  if (bt_gatt_client_write_value(central->cli.gatt, central->cli.central_identification_char_handle, value, length, client_write_cb_identification_char, central, NULL) != 0)
    return EXIT_FAILURE;
  return EXIT_SUCCESS;
}

/** write_ble_sps() -- start the connexion process with the SLATE
 * Input:   central -- pointer to the central structure
 **/
static void write_ble_sps(struct gatt_central *central)
{
  write_identification_char(central, central->cli.central_identification_char_handle, BLE_SPS_IDENT_CHAR_VALUE, BLE_SPS_IDENT_CHAR_VALUE_LENGTH);
}

/*-----------------------------------------------------------------------------
 * File transfer functions
 *-----------------------------------------------------------------------------*/

/** mldp_fifos_init() -- initialization of fifos
 * Input:   central  - pointer to the central strucutre
 * Output:  /
 * Return:  EXIT_SUCCESS on success, EXIT_FAILURE on error
 **/
static int mldp_fifos_init(struct gatt_central *central)
{
  int err_rx = 0, err_tx = 0;
  static uint8_t mldp_rx_buff[MLDP_RX_BUFF_SIZE]; // Buffer for MLDP RX FIFO instance
  static uint8_t mldp_tx_buff[MLDP_TX_BUFF_SIZE]; // Buffer for MLDP TX FIFO instance

  err_rx = fifo_init(&central->mldp_fifo_rx, mldp_rx_buff, sizeof(mldp_rx_buff));
  err_tx = fifo_init(&central->mldp_fifo_tx, mldp_tx_buff, sizeof(mldp_tx_buff));

  if (err_rx != 0 || err_tx != 0)
    return EXIT_FAILURE;
  return EXIT_SUCCESS;
}

/** tx_fifo_send_bytes -- Write data in MLDP characteristics
 * Input:   /
 * Output:  /
 * Return:  EXIT_SUCCESS on success, EXIT_FAILURE on error
 **/
static int tx_fifo_send_bytes()
{
  uint32_t nbReadableBytes = 0;
  uint32_t lengthToSend = 0;
  uint32_t length = 0;
  static uint8_t data_array[BLE_MLDP_MAX_DATA_LEN];
  bool signed_write = false;

  nbReadableBytes = fifo_length(&m_gatt_central->mldp_fifo_tx);

  if (nbReadableBytes == 0)
  {
    return EXIT_SUCCESS;
  }

  unsigned int repeatOnErrorCount = 0;
  while (nbReadableBytes)
  {
    length = MIN(nbReadableBytes, BLE_MLDP_MAX_DATA_LEN);
    lengthToSend = length;

    if (repeatOnErrorCount == 0)
    {
      if (fifo_read(&m_gatt_central->mldp_fifo_tx, data_array, &length) != 0)
        return EXIT_FAILURE;

      if (length != lengthToSend)
        return EXIT_FAILURE;
    }

    if (!bt_gatt_client_write_without_response(m_gatt_central->cli.gatt, m_gatt_central->cli.mldp_data_char_handle, false, data_array, length))
    {
      PRLOG_ERROR("PACKET NOT SENT\n");
      if (repeatOnErrorCount >= 3)
        break;

      repeatOnErrorCount++;
      continue;
    }
    repeatOnErrorCount = 0;
    nbReadableBytes -= length;
    usleep(1500);
  }
  return EXIT_SUCCESS;
}

/** ble_mldp_get_byte -- get one byte from rx fifo
 * Input:   /
 * Output:  p_byte -- pointer to the byte extract from the fifo
 * Return:  EXIT_SUCCESS on success, EXIT_FAILURE on error
 **/
static int ble_mldp_get_byte(uint8_t *p_byte)
{
  int err = fifo_get(&m_gatt_central->mldp_fifo_rx, p_byte);
  if (err != 0)
    return EXIT_FAILURE;

  return EXIT_SUCCESS;
}

/** ble_mldp_send_bytes -- Send the data to write in MLDP DATA characteristic
 * Input:   p_string -- pointer to the data to send
 *          length -- length in byte
 * Output:  /
 * Return: EXIT_SUCCESS on success, EXIT_FAILURE on error
 **/
static int ble_mldp_send_bytes(const uint8_t *p_string, uint32_t length)
{
  PRLOG_DEBUG("Begining Send Bytes : %u bytes\n", length)
  int err = 0;
  if (ble_con_step != BLE_FILE_TRANSFER)
    return EXIT_FAILURE;

  err = fifo_write(&m_gatt_central->mldp_fifo_tx, p_string, &length);
  if (err != 0)
    return err;

  if (tx_fifo_send_bytes() != 0)
    return EXIT_FAILURE;

  return EXIT_SUCCESS;
}

/** FT_WAIT_MS -- Standby function
 * Input:   timeMS -- time to wait before returning to the file transfer thread
 * Output:  /
 * Return: /
 * Explanation :Returns to the main thread and remains on standby for timeMS.
 * Listens if data from the MLDP DATA characteristic are received
 **/
static void ft_wait_ms(uint32_t timeMS)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  ts.tv_nsec += (timeMS * 1000000);

  pthread_cond_timedwait(&m_gatt_central->ft_s.condition, &m_gatt_central->ft_s.mutex, &ts);
  pthread_mutex_unlock(&m_gatt_central->ft_s.mutex);
}

/** fifos_flush -- Reset fifos
 * Input:   central -- pointer to the central structure
 **/
static void fifos_flush(struct gatt_central *central)
{
  fifo_flush(&central->mldp_fifo_rx);
  fifo_flush(&central->mldp_fifo_tx);
}

/** start_file_transfer -- Start file transfer
 * Input:   central -- pointer to the central structure
 * Explanation : Called when the first MLDP data is received.
 *               This function initializes the function that link the bluetooth part to the Kermit program.
 **/
static void *start_file_transfer(struct gatt_central *central)
{
  int err = 0;
  fifos_flush(central);

  memset(&central->ft_s.kermit_handler_s, 0, sizeof(central->ft_s.kermit_handler_s));
  central->ft_s.kermit_handler_s.ble_mldp_get_byte = ble_mldp_get_byte;
  central->ft_s.kermit_handler_s.ble_mldp_send_bytes = ble_mldp_send_bytes;
  central->ft_s.kermit_handler_s.ft_wait_ms = ft_wait_ms;

  err = file_transfer_start_server(&central->ft_s);
  if (err != 0)
  {
    exit(1); //failed to create file transfer thread
  }
}

/*-----------------------------------------------------------------------------
 * Populate central server database
 *-----------------------------------------------------------------------------*/

/** server_mldp_data_char_write_cb -- Callback function of the server MLDP characteristics.
 * Input:   attrib -- attribut of the server database
 *          id -- identifier of the GATT request.
 *          offset -- unused here.
 *          value -- pointer to th value just written in the server MLDP char.
 *          len -- length of value
 *          opcode -- operation code, can be used to speficie an operation to do. Not used here.
 *          att --
 *          user_data -- pointer to the central structure
 * Output:  /
 * Return:  /
 * Explanation : When the remote device writes in the Rpi MLDP data char, it will be notified by this function.
 * Each data received are put in the rx fifo.
 * When we receive the first data, we lauch the file transfer.
 **/
static void server_mldp_data_char_write_cb(struct gatt_db_attribute *attrib, unsigned int id, uint16_t offset, const uint8_t *value, size_t len, uint8_t opcode, struct bt_att *att, void *user_data)
{

  struct gatt_central *central = user_data;
  uint8_t ecode = 0;

  if (attrib == NULL)
    return;

  if (offset)
  {
    ecode = BT_ATT_ERROR_INVALID_OFFSET;
    goto done;
  }

  if (ble_con_step == BLE_WAIT_MLDP_DATA)
  {
    PRLOG_DEBUG("Start file transfer\n");
    ble_con_step = BLE_FILE_TRANSFER;
    start_file_transfer(central);
  }
  fifo_write(&central->mldp_fifo_rx, value, &len);

done:
  gatt_db_attribute_write_result(attrib, id, ecode);
}

/** populate_mldp_service -- Cretae MLDP service
 * Input: central -- pointer to the central structure
 **/
static void populate_mldp_service(struct gatt_central *central)
{
  bt_uuid_t uuid;
  struct gatt_db_attribute *service, *mldp_data_chr_attr, *mldp_ctrl_char_attr;

  /* Add MDLP Service */
  bt_uuid128_create(&uuid, mldp_service_uuid);
  service = gatt_db_add_service(central->db_s, &uuid, true, 8);
  central->srv.mldp_service_handle = gatt_db_attribute_get_handle(service);

  /* Add MLDP_DATA Characteristic */
  bt_uuid128_create(&uuid, mldp_data_char_uuid);
  mldp_data_chr_attr = gatt_db_service_add_characteristic(service, &uuid,
                                                          BT_ATT_PERM_READ | BT_ATT_PERM_WRITE,
                                                          BT_GATT_CHRC_PROP_READ | BT_GATT_CHRC_PROP_WRITE | BT_GATT_CHRC_PROP_WRITE_WITHOUT_RESP | BT_GATT_CHRC_PROP_INDICATE | BT_GATT_CHRC_PROP_NOTIFY,
                                                          NULL, server_mldp_data_char_write_cb, central);
  central->srv.mldp_data_char_handle = gatt_db_attribute_get_handle(mldp_data_chr_attr);
  central->srv.mldp_data_char_attr = mldp_data_chr_attr;

  bt_uuid16_create(&uuid, GATT_CLIENT_CHARAC_CFG_UUID);
  gatt_db_service_add_descriptor(service, &uuid,
                                 BT_ATT_PERM_READ | BT_ATT_PERM_WRITE,
                                 NULL,
                                 NULL, central);

  /* Add MLDP_CTRL Characteristic */
  bt_uuid128_create(&uuid, mldp_ctrl_char_uuid);
  mldp_ctrl_char_attr = gatt_db_service_add_characteristic(service, &uuid,
                                                           BT_ATT_PERM_READ | BT_ATT_PERM_WRITE,
                                                           BT_GATT_CHRC_PROP_READ | BT_GATT_CHRC_PROP_WRITE | BT_GATT_CHRC_PROP_WRITE_WITHOUT_RESP,
                                                           NULL, NULL, NULL);
  central->srv.mldp_ctrl_char_handle = gatt_db_attribute_get_handle(mldp_ctrl_char_attr);
  central->srv.mldp_ctrl_char_attr = mldp_ctrl_char_attr;

  gatt_db_service_set_active(service, true);
}

/** populate_db -- add services in the server database
 * Input: central -- pointer to the central structure
 **/
static void populate_db(struct gatt_central *central)
{
  populate_mldp_service(central);
}

/** service_changed_cb -- Callback function used to modify a server service
 * Input:   start_handle -- first handle of the service to modify
 *          end_handle -- last handle of the service to modify
 *          user_data -- pointer to central structure
 **/
static void service_changed_cb(uint16_t start_handle, uint16_t end_handle, void *user_data)
{
  struct gatt_central *central = user_data;
  gatt_db_foreach_service_in_range(central->db_c, NULL, print_service, central,
                                   start_handle, end_handle);
}

/*-----------------------------------------------------------------------------
 *
 *-----------------------------------------------------------------------------*/

/** ready_cb -- Callback function used in gatt_central_create()
 * Input:   success -- true on success to create client structure and false on fail
 *          att_ecode -- unused here
 *          user_data -- pointer to the central structure
 * Output:  /
 * Return:  /
 * Explanation :This function is called after central was created and mainloop was init.
 * It allows to store client services handles and to launch the identification and authentication protocol of the SLATE
 **/
static void ready_cb(bool success, uint8_t att_ecode, void *user_data)
{
  struct gatt_central *central = user_data;
  if (!success)
    return;

  get_handle_from_uuid(central);
  ble_con_step = BLE_ALL_SERVICE_DISCOVERY_COMPLETE;
  write_ble_sps(central);
}

/** att_debug_func -- Function to debug ATT protocol
 * Input: str -- pointer to the string to print
 *        user_data -- unused here
 * Explanation : activate the ATT debugging tool in gatt_central_create()
 **/
void att_debug_func(const char *str, void *user_data)
{
  fprintf(stderr, "att: %s\n", str);
}

/** gatt_central_create -- Create Central structure with client and server.
 * Input: fd -- file descriptor of the socket
 *        mtu -- length of ATT packet (default 23 bytes)
 * Explanation : activate the ATT debugging tool in gatt_central_create()
 **/
static struct gatt_central *gatt_central_create(int fd, uint16_t mtu)
{
  struct gatt_central *central;

  central = new0(struct gatt_central, 1);
  if (!central)
    return NULL;

  central->att = bt_att_new(fd, false);
  if (!central->att)
  {
    bt_att_unref(central->att);
    free(central);
    return NULL;
  }

  /****** ATT PROTOCOLE DEBUGGING  ******/

  /*if (!bt_att_set_debug(central->att, att_debug_func, NULL, NULL))
  {
    fprintf(stderr, "Failed bt_att_set_debug\n");
  }*/

  if (!bt_att_set_close_on_unref(central->att, true))
  {
    bt_att_unref(central->att);
    free(central);
    return NULL;
  }

  if (!bt_att_register_disconnect(central->att, att_disconnect_cb, NULL,
                                  NULL))
  {
    bt_att_unref(central->att);
    free(central);
    return NULL;
  }

  central->fd = fd;
  central->db_c = gatt_db_new();
  central->db_s = gatt_db_new();

  if (!central->db_c)
  {
    bt_att_unref(central->att);
    free(central);
    return NULL;
  }
  if (!central->db_s)
  {
    bt_att_unref(central->att);
    free(central);
    return NULL;
  }

  central->srv.gatt = bt_gatt_server_new(central->db_s, central->att, mtu, 0);
  if (!central->srv.gatt)
  {
    gatt_db_unref(central->db_s);
    bt_att_unref(central->att);
    free(central);
    return NULL;
  }

  central->cli.gatt = bt_gatt_client_new(central->db_c, central->att, mtu, 0);
  if (!central->cli.gatt)
  {
    gatt_db_unref(central->db_c);
    bt_att_unref(central->att);
    free(central);
    return NULL;
  }

  gatt_db_register(central->db_c, service_added_cb, service_removed_cb,
                   NULL, NULL);

  bt_gatt_client_ready_register(central->cli.gatt, ready_cb, central, NULL);
  bt_gatt_client_set_service_changed(central->cli.gatt, service_changed_cb, central, NULL);

  /* bt_gatt_client already holds a reference */
  gatt_db_unref(central->db_c);
  gatt_db_unref(central->db_s);

  populate_db(central);

  return central;
}

/** l2cap_le_att_connect -- Open a Bluetooth socket base on the L2CAP layer.
 * Input:   src -- pointer to the source address (the address of the Rpi).
 *          dst -- pointer to the destination address (address of the remote device).
 *          dst_type -- type of the destination address : public or private.
 *          sec -- specifies the level of security of the socket.
 * Output:  /
 * Return:  /
 * Explanation : This function allows to create a bluetooth socket. The function connect() is used because it's the central that initiate the connection.
 * Otherwise, you must use the listen() function and wait for a remote device to connect to the socket
 **/
static int l2cap_le_att_connect(bdaddr_t *src, bdaddr_t *dst, uint8_t dst_type, int sec)
{
  int sock;
  struct sockaddr_l2 srcaddr, dstaddr;
  struct bt_security btsec;

  sock = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
  if (sock < 0)
    return EXIT_FAILURE;

  /* Set up source address */
  memset(&srcaddr, 0, sizeof(srcaddr));
  srcaddr.l2_family = AF_BLUETOOTH;
  srcaddr.l2_cid = htobs(ATT_CID);
  srcaddr.l2_bdaddr_type = 0;
  bacpy(&srcaddr.l2_bdaddr, src);

  if (bind(sock, (struct sockaddr *)&srcaddr, sizeof(srcaddr)) < 0)
  {
    close(sock);
    return EXIT_FAILURE;
  }

  /* Set the security level */
  memset(&btsec, 0, sizeof(btsec));
  btsec.level = sec;
  if (setsockopt(sock, SOL_BLUETOOTH, BT_SECURITY, &btsec,
                 sizeof(btsec)) != 0)
  {
    close(sock);
    return EXIT_FAILURE;
  }

  /* Set up destination address */
  memset(&dstaddr, 0, sizeof(dstaddr));
  dstaddr.l2_family = AF_BLUETOOTH;
  dstaddr.l2_cid = htobs(ATT_CID);
  dstaddr.l2_bdaddr_type = dst_type;
  bacpy(&dstaddr.l2_bdaddr, dst);

  PRLOG("Connecting to device...");
  fflush(stdout);

  if (connect(sock, (struct sockaddr *)&dstaddr, sizeof(dstaddr)) < 0)
  {
    perror(" Failed to connect");
    close(sock);
    return -1;
  }
  PRLOG(" Done\n");
  return sock;
}

/** retry_scan -- Function to restart scan on error
 * Input: central -- pointer to the central structure
 **/
static void retry_scan(struct gatt_central *central)
{
  mainloop_quit();
  ble_con_step = BLE_SCANNING;
}

/** address_usage -- Print the format of address to respect
 **/
static void address_usage()
{
  PRLOG("INVALID ADDRESS\n");
  PRLOG("Address must be in this form : xx:xx:xx:xx:xx:xx (where 'x' are hexadecimals symbols)\n");
}

/** parse_given_address -- Watch if address is in the MAC address format
 * Input: address -- address to parse
 * Return : 0 on succes, else -1
 **/
static int parse_given_address(char *address)
{
  regex_t reg;
  const char *str_regex = "^[a-fA-F0-9:]{17}|[a-fA-F0-9]{12}$";
  int err;

  // Creation of regEx
  err = regcomp(&reg, str_regex, REG_NOSUB | REG_EXTENDED);
  if (err == 0) //regex compiled successfully
  {
    int match;
    match = regexec(&reg, address, 0, NULL, 0);
    regfree(&reg);
    if (match == 0)
    {
      ble_con_step = BLE_SCANNING;
      return 0;
    }
    else
    {
      address_usage();
      return -1;
    }
  }
  else
  {
    PRLOG("Error in regular expression compilation\n");
    return -1;
  }
}

int main(int argc, char *argv[])
{
  int dev_id = hci_get_route(NULL);
  bdaddr_t src_addr, dst_addr;
  int fd = 0, err_mutex = 0, err_cond = 0;
  uint16_t mtu = 0;
  struct gatt_central *central;

  if (parse_given_address(argv[1]) != 0)
  { /* INVALID address */
    exit(1);
  }

  while (end_of_state_machine == 0)
  {
    signal(SIGINT, &sig_handler); //listen if ctrl-c is pressed
    if (ble_con_step == BLE_SCANNING)
    {
      cmd_lescan(dev_id, argv[1]);
    }
    else if (ble_con_step == BLE_SLATE_FOUND)
    {
      le_connection(dev_id, argv[1]);
    }
    else if (ble_con_step == BLE_CONNECTED)
    {
      if (str2ba(argv[1], &dst_addr))
      {
        break;
      }
      if (dev_id == -1)
        bacpy(&src_addr, BDADDR_ANY);
      else if (hci_devba(dev_id, &src_addr) < 0)
      {
        perror("Adapter not available");
        break;
      }
      fd = l2cap_le_att_connect(&src_addr, &dst_addr, BDADDR_LE_PUBLIC, BT_SECURITY_LOW);
      if (fd < 0)
      {
        ble_con_step = BLE_SCANNING;
        break;
      }
      ble_con_step = BLE_SOCKET_OPEN;
    }
    else if (ble_con_step == BLE_SOCKET_OPEN)
    {
      mainloop_init();
      central = gatt_central_create(fd, mtu);
      if (!central)
      {
        ble_con_step = BLE_SCANNING;
        close(fd);
        break;
      }
      m_gatt_central = central; //access to the central instance everywhere
      pthread_condattr_init(&central->ft_s.attr);
      pthread_condattr_setclock(&central->ft_s.attr, CLOCK_MONOTONIC);
      err_cond = pthread_cond_init(&central->ft_s.condition, &central->ft_s.attr);
      err_mutex = pthread_mutex_init(&central->ft_s.mutex, NULL);
      if (err_cond != 0 || err_mutex != 0)
      {
        break;
      }
      mldp_fifos_init(central);
      mainloop_run();
    }
  }
  pthread_exit(NULL);
  return 0;
}
