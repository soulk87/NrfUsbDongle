#include "spi.h"

#ifdef _USE_HW_SPI
#include <nrfx_spim.h>
#include "cli.h"

typedef struct
{
  nrfx_spim_t spim;
  nrfx_spim_config_t config;
  void (*p_tx_done_func)(void);
  uint32_t freq;
  bool is_init;
  bool is_dma_mode;
  bool is_tx_done;
} spi_tbl_t;

#define SCK_PIN 24  // P0.24
#define MOSI_PIN 32 // P1.00
#define MISO_PIN 32 // P1.00
#define SS_PIN 38   // P1.06

static spi_tbl_t spi_tbl[HW_SPI_MAX_CH] = {
   {
        .spim = NRFX_SPIM_INSTANCE(3),
        .config = NRFX_SPIM_DEFAULT_CONFIG(SCK_PIN, MOSI_PIN, MISO_PIN, SS_PIN),
        .freq = 32000000,
        .p_tx_done_func = NULL,
        .is_init = false,
        .is_dma_mode = true
    } 
};

#ifdef _USE_CLI_SPI
void cliSpi(cli_args_t *args);
#endif

/**
 * @brief Function for handling SPIM driver events.
 *
 * @param[in] p_event   Pointer to the SPIM driver event.
 * @param[in] p_context Pointer to the context passed from the driver.
 */
static void spim_handler(nrfx_spim_evt_t const * p_event, void * p_context)
{
    if (p_event->type == NRFX_SPIM_EVENT_DONE)
    {
        spi_tbl_t *p_spi = (spi_tbl_t *)p_context;
        p_spi->is_tx_done = true;
        if (p_spi->p_tx_done_func != NULL)
        {
          p_spi->p_tx_done_func();
        }
    }
}

bool spiInit(void)
{
  uint32_t err_count = 0;
  

  IRQ_CONNECT(NRFX_IRQ_NUMBER_GET(NRF_SPIM_INST_GET(3)), IRQ_PRIO_LOWEST,
        NRFX_SPIM_INST_HANDLER_GET(3), 0, 0);

  for(int i=0; i<SPI_MAX_CH; i++)
  {
    nrfx_err_t status;

    spi_tbl[i].config.frequency = spi_tbl[i].freq;
    
    if(spi_tbl[i].is_dma_mode == true)
    {


      status = nrfx_spim_init(&spi_tbl[i].spim, &spi_tbl[i].config, spim_handler, (void *)&spi_tbl[i]);
    }
    else
    {
      status = nrfx_spim_init(&spi_tbl[i].spim, &spi_tbl[i].config, NULL, NULL);
    }

    if (status != NRFX_SUCCESS)
    {
      err_count++;
    }
    spi_tbl[i].is_init = true;
  }

#ifdef _USE_CLI_SPI
  cliAdd("spi", cliSpi);
#endif

  if (err_count > 0)
  {
    return false;
  }
  return true;
}

void spiAttachTxInterrupt(uint8_t ch, void (*func)())
{
  if (ch >= HW_SPI_MAX_CH)
  {
    return;
  }

  if(spi_tbl[ch].is_dma_mode == false)
  {
    return;
  }

  spi_tbl[ch].p_tx_done_func = func;
}

bool spiTransfer(uint8_t ch, uint8_t *tx_buf, uint32_t tx_length, uint8_t *rx_buf, uint32_t rx_length, uint32_t timeout)
{
  nrfx_err_t status;

  uint32_t timeout_cnt = timeout;

  if ((ch >= HW_SPI_MAX_CH) || (spi_tbl[ch].is_init == false))
  {
    return false;
  }

  if(spi_tbl[ch].is_dma_mode == true)
  {
    spi_tbl[ch].is_tx_done = false;
  }

  nrfx_spim_t spim_inst = spi_tbl[ch].spim;

  nrfx_spim_xfer_desc_t xfer_desc = NRFX_SPIM_XFER_TRX(tx_buf, tx_length, rx_buf, rx_length);

  status = nrfx_spim_xfer(&spim_inst, &xfer_desc, 0);
  if (status != NRFX_SUCCESS)
  {
    return false;
  }

  // dma 모드 spi를 polling 방식으로 사용 시 완료 대기
  if(spi_tbl[ch].is_dma_mode == true)
  {
    while(spi_tbl[ch].is_tx_done == false)
    {
      timeout_cnt--;
      if (timeout_cnt == 0)
      {
        return false;
      }
      // delay(1);
    }
  }



  return true;
}

bool spiTransferDma(uint8_t ch, uint8_t *tx_buf, uint32_t tx_length, uint8_t *rx_buf, uint32_t rx_length)
{
  nrfx_err_t status;

  if ((ch >= HW_SPI_MAX_CH) || (spi_tbl[ch].is_init == false) || (spi_tbl[ch].is_dma_mode == false))
  {
    return false;
  }

  nrfx_spim_t spim_inst = spi_tbl[ch].spim;

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