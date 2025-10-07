#include "myrf.h"
#include "qbuffer.h"
#include "cli.h"

#ifdef _USE_HW_RF
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/clock_control/nrf_clock_control.h>
#include <zephyr/irq.h>
#include <zephyr/logging/log.h>
#include <nrf.h>
#include <esb.h>
#include <zephyr/kernel.h>
#include <zephyr/types.h>

LOG_MODULE_REGISTER(esb_driver, LOG_LEVEL_NONE);

#define RF_RX_BUF_LENGTH 256

static struct esb_payload rx_payload;
static struct esb_payload tx_payload = ESB_CREATE_PAYLOAD(0,
                              0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17);

static qbuffer_t rf_rx_q;
static uint8_t rf_rx_buf[RF_RX_BUF_LENGTH];
static struct k_mutex rf_rx_mutex;

#ifdef _USE_CLI_HW_RF
static void cliCmd(cli_args_t *args);
#endif
static int clocks_start(void);
static void event_handler(struct esb_evt const *event);
static int esb_initialize(void);

bool rfInit(void)
{
  int err;

  qbufferCreate(&rf_rx_q, rf_rx_buf, RF_RX_BUF_LENGTH);
  k_mutex_init(&rf_rx_mutex);

  err = clocks_start();
  if (err)
  {
  return false;
  }

  err = esb_initialize();
  if (err)
  {
    LOG_ERR("ESB initialization failed, err %d", err);
  return false;
  }
  LOG_INF("Initialization complete");

#if HW_RF_MODE == _DEF_RF_MODE_TX
  tx_payload.noack = false;
#else
  err = esb_write_payload(&tx_payload);
  if (err)
  {
    LOG_ERR("Write payload, err %d", err);
  return false;
  }

  LOG_INF("Setting up for packet receiption");

  err = esb_start_rx();
  if (err)
  {
    LOG_ERR("RX setup failed, err %d", err);
  return false;
  }
#endif

#ifdef _USE_CLI_HW_RF
  cliAdd("rf", cliCmd);
#endif
  return true;
}

uint32_t rfAvailable(void)
{
  uint32_t ret;
  
  k_mutex_lock(&rf_rx_mutex, K_FOREVER);
  ret = qbufferAvailable(&rf_rx_q);
  k_mutex_unlock(&rf_rx_mutex);
  
  return ret;
}

bool rfBufferFlush(void)
{
  k_mutex_lock(&rf_rx_mutex, K_FOREVER);
  qbufferFlush(&rf_rx_q);
  k_mutex_unlock(&rf_rx_mutex);
  return true;
}


uint32_t rfWrite(uint8_t *p_data, uint32_t length)
{
#if HW_RF_MODE == _DEF_RF_MODE_TX
  if (length > 30)
  length = 30;

  memcpy(tx_payload.data, p_data, length);
  tx_payload.length = length;

  if (esb_write_payload(&tx_payload) == 0)
  return length;
  else
  return 0;
#else
  return 0;
#endif
}

uint32_t rfRead(uint8_t *p_data, uint32_t length)
{
  uint32_t ret;
  
  k_mutex_lock(&rf_rx_mutex, K_FOREVER);
  ret = qbufferRead(&rf_rx_q, p_data, length);
  k_mutex_unlock(&rf_rx_mutex);
  
  return ret;
}

static void event_handler(struct esb_evt const *event)
{
  switch (event->evt_id)
  {
  case ESB_EVENT_TX_SUCCESS:
    LOG_DBG("TX SUCCESS EVENT");
  break;
  case ESB_EVENT_TX_FAILED:
    LOG_DBG("TX FAILED EVENT");
  break;
  case ESB_EVENT_RX_RECEIVED:
  if (esb_read_rx_payload(&rx_payload) == 0)
  {
    // 수신된 데이터를 큐에 저장
    k_mutex_lock(&rf_rx_mutex, K_FOREVER);
    qbufferWrite(&rf_rx_q, rx_payload.data, rx_payload.length);
    k_mutex_unlock(&rf_rx_mutex);
  }
  break;
  }
}

static int clocks_start(void)
{
  int err;
  int res;
  struct onoff_manager *clk_mgr;
  struct onoff_client clk_cli;

  clk_mgr = z_nrf_clock_control_get_onoff(CLOCK_CONTROL_NRF_SUBSYS_HF);
  if (!clk_mgr)
  {
    LOG_ERR("Unable to get the Clock manager");
  return -ENXIO;
  }

  sys_notify_init_spinwait(&clk_cli.notify);

  err = onoff_request(clk_mgr, &clk_cli);
  if (err < 0)
  {
    LOG_ERR("Clock request failed: %d", err);
  return err;
  }

  do
  {
  err = sys_notify_fetch_result(&clk_cli.notify, &res);
  if (!err && res)
    {
      LOG_ERR("Clock could not be started: %d", res);
    return res;
    }
  } while (err);

  LOG_DBG("HF clock started");
  return 0;
}

#if HW_RF_MODE == _DEF_RF_MODE_TX
static int esb_initialize(void)
{
  int err;
  /* These are arbitrary default addresses. In end user products
   * different addresses should be used for each set of devices.
   */
  uint8_t base_addr_0[4] = {0xE7, 0xE7, 0xE7, 0xE7};
  uint8_t base_addr_1[4] = {0xC2, 0xC2, 0xC2, 0xC2};
  uint8_t addr_prefix[8] = {0xE7, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8};

  struct esb_config config = ESB_DEFAULT_CONFIG;

  config.protocol = ESB_PROTOCOL_ESB_DPL;
  config.retransmit_delay = 600;
  config.bitrate = ESB_BITRATE_2MBPS;
  config.event_handler = event_handler;
  config.mode = ESB_MODE_PTX;
  config.selective_auto_ack = true;
  if (IS_ENABLED(CONFIG_ESB_FAST_SWITCHING))
  {
  config.use_fast_ramp_up = true;
  }

  err = esb_init(&config);

  if (err)
  {
    return err;
  }

  err = esb_set_base_address_0(base_addr_0);
  if (err)
  {
    return err;
  }

  err = esb_set_base_address_1(base_addr_1);
  if (err)
  {
    return err;
  }

  err = esb_set_prefixes(addr_prefix, ARRAY_SIZE(addr_prefix));
  if (err)
  {
    return err;
  }

  return 0;
}

#else

static int esb_initialize(void)
{
  int err;
  /* These are arbitrary default addresses. In end user products
   * different addresses should be used for each set of devices.
   */
  uint8_t base_addr_0[4] = {0xE7, 0xE7, 0xE7, 0xE7};
  uint8_t base_addr_1[4] = {0xC2, 0xC2, 0xC2, 0xC2};
  uint8_t addr_prefix[8] = {0xE7, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8};

  struct esb_config config = ESB_DEFAULT_CONFIG;

  config.protocol = ESB_PROTOCOL_ESB_DPL;
  config.bitrate = ESB_BITRATE_2MBPS;
  config.mode = ESB_MODE_PRX;
  config.event_handler = event_handler;
  config.selective_auto_ack = true;
  if (IS_ENABLED(CONFIG_ESB_FAST_SWITCHING))
  {
  config.use_fast_ramp_up = true;
  }

  err = esb_init(&config);
  if (err)
  {
    return err;
  }

  err = esb_set_base_address_0(base_addr_0);
  if (err)
  {
    return err;
  }

  err = esb_set_base_address_1(base_addr_1);
  if (err)
  {
    return err;
  }
  
  err = esb_set_prefixes(addr_prefix, ARRAY_SIZE(addr_prefix));
  if (err)
  {
    return err;
  }

  return 0;
}

#endif

#ifdef _USE_CLI_HW_RF
void cliCmd(cli_args_t *args)
{
  if (args->argc == 1 && args->isStr(0, "tx"))
  {
  esb_flush_tx();
  esb_write_payload(&tx_payload);
  tx_payload.data[1]++;
  cliPrintf("rf tx\n");
  }
  else if (args->argc == 1 && args->isStr(0, "rx"))
  {
  cliPrintf("rf available: %d\n", rfAvailable());
  }
  else
  {
  cliPrintf("rf tx\n");
  cliPrintf("rf rx\n");
  }
}
#endif
#endif
