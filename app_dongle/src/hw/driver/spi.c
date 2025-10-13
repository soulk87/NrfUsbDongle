#include "spi.h"

#ifdef _USE_HW_SPI
#include <nrfx_spim.h>
#include "cli.h"

#define SCK_PIN 24  // P0.24
#define MOSI_PIN 32 // P1.00
#define MISO_PIN 32 // P1.00
#define SS_PIN 38   // P1.06

static const nrfx_spim_t spim[HW_SPI_MAX_CH] = {NRFX_SPIM_INSTANCE(1)};

#ifdef _USE_CLI_SPI
void cliSpi(cli_args_t *args);
#endif

bool spiInit(void)
{
  nrfx_err_t status;

  nrfx_spim_config_t spim_config = NRFX_SPIM_DEFAULT_CONFIG(SCK_PIN,
                                                            MOSI_PIN,
                                                            MISO_PIN,
                                                            SS_PIN);
  spim_config.frequency = NRFX_MHZ_TO_HZ(8);
  status = nrfx_spim_init(&spim[0], &spim_config, NULL, NULL);
  if (status != NRFX_SUCCESS)
  {
    return false;
  }

#ifdef _USE_CLI_SPI
  cliAdd("spi", cliSpi);
#endif

  return true;
}

void spiSetDataMode(uint8_t ch, uint8_t dataMode)
{
}

bool spiTransfer(uint8_t ch, uint8_t *tx_buf, uint32_t tx_length, uint8_t *rx_buf, uint32_t rx_length, uint32_t timeout)
{
  nrfx_err_t status;

  if (ch >= HW_SPI_MAX_CH)
  {
    return false;
  }

  nrfx_spim_t spim_inst = spim[ch];

  nrfx_spim_xfer_desc_t xfer_desc = NRFX_SPIM_XFER_TRX(tx_buf, tx_length, rx_buf, rx_length);

  status = nrfx_spim_xfer(&spim_inst, &xfer_desc, 0);
  if (status != NRFX_SUCCESS)
  {
    return false;
  }

  return true;
}

#ifdef _USE_CLI_SPI
void cliSpi(cli_args_t *args)
{
  bool ret = false;
  /** @brief Transmit buffer initialized with the specified message ( @ref MSG_TO_SEND ). */
  // static uint8_t m_tx_buffer[16] = {0};

  /** @brief Receive buffer defined with the size to store specified message ( @ref MSG_TO_SEND ). */
  // static uint8_t m_rx_buffer[16];

  if (args->argc == 1)
  {
    if (args->isStr(0, "test") == true)
    {
      // Perform test operation
      // spiTransfer(0, m_tx_buffer, m_rx_buffer, sizeof(m_tx_buffer), 1000);
      cliPrintf("SPI test executed.\n");

      ret = true;
    }
  }
  // if (args->argc == 3)
  // {
  //   if(args->isStr(0, "read") == true)
  //   {
  //     addr   = (uint32_t)args->getData(1);
  //     length = (uint32_t)args->getData(2);

  //     if (length > eepromGetLength())
  //     {
  //       cliPrintf( "length error\n");
  //     }
  //     for (i=0; i<length; i++)
  //     {
  //       if (eepromReadByte(addr+i, &data) == true)
  //       {
  //         cliPrintf( "addr : %d\t 0x%02X\n", addr+i, data);
  //       }
  //       else
  //       {
  //         cliPrintf("eepromReadByte() Error\n");
  //         break;
  //       }
  //     }
  //   }
  //   else if(args->isStr(0, "write") == true)
  //   {
  //     addr = (uint32_t)args->getData(1);
  //     data = (uint8_t )args->getData(2);

  //     pre_time = millis();
  //     eep_ret = eepromWriteByte(addr, data);

  //     cliPrintf( "addr : %d\t 0x%02X %dms\n", addr, data, millis()-pre_time);
  //     if (eep_ret)
  //     {
  //       cliPrintf("OK\n");
  //     }
  //     else
  //     {
  //       cliPrintf("FAIL\n");
  //     }
  //   }
  //   else
  //   {
  //     ret = false;
  //   }
  // }
  // else
  // {
  //   ret = false;
  // }
  ret = false;

  if (ret == false)
  {
    cliPrintf("spi test\n");
    cliPrintf("spi read  [addr] [length]\n");
    cliPrintf("spi write [addr] [data]\n");
  }
}
#endif

#endif