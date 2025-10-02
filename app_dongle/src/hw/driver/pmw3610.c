#include "pmw3610.h"

#include "spi.h"
#include "bsp.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(pmw3610, LOG_LEVEL_INF);
/* Page 0 */
#define PMW3610_PROD_ID 0x00
#define PMW3610_REV_ID 0x01
#define PMW3610_MOTION 0x02
#define PMW3610_DELTA_X_L 0x03
#define PMW3610_DELTA_Y_L 0x04
#define PMW3610_DELTA_XY_H 0x05
#define PMW3610_PERFORMANCE 0x11
#define PMW3610_BURST_READ 0x12
#define PMW3610_RUN_DOWNSHIFT 0x1b
#define PMW3610_REST1_RATE 0x1c
#define PMW3610_REST1_DOWNSHIFT 0x1d
#define PMW3610_OBSERVATION1 0x2d
#define PMW3610_SMART_MODE 0x32
#define PMW3610_POWER_UP_RESET 0x3a
#define PMW3610_SHUTDOWN 0x3b
#define PMW3610_SPI_CLK_ON_REQ 0x41
#define PMW3610_SPI_PAGE0 0x7f

/* Page 1 */
#define PMW3610_RES_STEP 0x05
#define PMW3610_SPI_PAGE1 0x7f

/* Burst register offsets */
#define BURST_MOTION 0
#define BURST_DELTA_X_L 1
#define BURST_DELTA_Y_L 2
#define BURST_DELTA_XY_H 3
#define BURST_SQUAL 4
#define BURST_SHUTTER_HI 5
#define BURST_SHUTTER_LO 6

#define BURST_DATA_LEN_NORMAL (BURST_DELTA_XY_H + 1)
#define BURST_DATA_LEN_SMART (BURST_SHUTTER_LO + 1)
#define BURST_DATA_LEN_MAX MAX(BURST_DATA_LEN_NORMAL, BURST_DATA_LEN_SMART)

/* Init sequence values */
#define OBSERVATION1_INIT_MASK 0x0f
#define PERFORMANCE_INIT 0x0d
#define RUN_DOWNSHIFT_INIT 0x04
#define REST1_RATE_INIT 0x04
#define REST1_DOWNSHIFT_INIT 0x0f

#define PRODUCT_ID_PMW3610 0x3e
#define SPI_WRITE BIT(7)
#define MOTION_STATUS_MOTION BIT(7)
#define SPI_CLOCK_ON_REQ_ON 0xba
#define SPI_CLOCK_ON_REQ_OFF 0xb5
#define RES_STEP_INV_X_BIT 6
#define RES_STEP_INV_Y_BIT 5
#define RES_STEP_RES_MASK 0x1f
#define PERFORMANCE_FMODE_MASK (0x0f << 4)
#define PERFORMANCE_FMODE_NORMAL (0x00 << 4)
#define PERFORMANCE_FMODE_FORCE_AWAKE (0x0f << 4)
#define POWER_UP_RESET 0x5a
#define POWER_UP_WAKEUP 0x96
#define SHUTDOWN_ENABLE 0xe7
#define SPI_PAGE0_1 0xff
#define SPI_PAGE1_0 0x00
#define SHUTTER_SMART_THRESHOLD 45
#define SMART_MODE_ENABLE 0x00
#define SMART_MODE_DISABLE 0x80

#define PMW3610_DATA_SIZE_BITS 12

#define RESET_DELAY_MS 10
#define INIT_OBSERVATION_DELAY_MS 10
#define CLOCK_ON_DELAY_US 300

#define RES_STEP 200
#define RES_MIN 200
#define RES_MAX 3200

struct pmw3610_config
{
    uint16_t axis_x;
    uint16_t axis_y;
    int16_t res_cpi;
    bool invert_x;
    bool invert_y;
    bool force_awake;
    bool smart_mode;
};

// struct pmw3610_data
// {
//     const struct device *dev;
//     struct k_work motion_work;
//     struct gpio_callback motion_cb;
//     bool smart_flag;
// };

static uint8_t pmw3610_tx_buffer[16] = {0};
static uint8_t pmw3610_rx_buffer[16] = {0};

struct pmw3610_config pmw3610_cfg = {
    .axis_x = 0,
    .axis_y = 1,
    .res_cpi = 1600,
    .invert_x = false,
    .invert_y = false,
    .force_awake = true,
    .smart_mode = false,
};

static int pmw3610_read(uint8_t addr, uint8_t *value, uint8_t len)
{
    bool ret;
    pmw3610_tx_buffer[0] = addr;
    pmw3610_tx_buffer[1] = 0x00;
    memset(pmw3610_rx_buffer, 0, sizeof(pmw3610_rx_buffer));

    ret = spiTransfer(HW_PMW3610_SPI_CH, pmw3610_tx_buffer, 1, pmw3610_rx_buffer, 1 + len, 1000);

    if (!ret)
    {
        return -1;
    }
    else
    {
        memcpy(value, &pmw3610_rx_buffer[1], len);
        return 0;
    }
}

static int pmw3610_read_reg(uint8_t addr, uint8_t *value)
{
    return pmw3610_read(addr, value, 1);
}

static int pmw3610_write_reg(uint8_t addr, uint8_t value)
{
    bool ret;
    pmw3610_tx_buffer[0] = addr | SPI_WRITE;
    pmw3610_tx_buffer[1] = value;

    ret = spiTransfer(HW_PMW3610_SPI_CH, pmw3610_tx_buffer, 2, pmw3610_rx_buffer, 0, 1000);

    if (!ret)
    {
        return -1;
    }
    else
    {
        return 0;
    }
}

static int pmw3610_spi_clk_on(void)
{
    int ret;

    ret = pmw3610_write_reg(PMW3610_SPI_CLK_ON_REQ, SPI_CLOCK_ON_REQ_ON);

    delay_us(CLOCK_ON_DELAY_US);

    return ret;
}

static int pmw3610_spi_clk_off(void)
{
    return pmw3610_write_reg(PMW3610_SPI_CLK_ON_REQ, SPI_CLOCK_ON_REQ_OFF);
}


bool pmw3610_motion_read(int32_t* x_out, int32_t* y_out)
{
	const struct pmw3610_config *cfg = &pmw3610_cfg;
	uint8_t burst_data[BURST_DATA_LEN_MAX];
	uint8_t burst_data_len;
	int32_t x, y;
	int ret;

	if (cfg->smart_mode) {
		burst_data_len = BURST_DATA_LEN_SMART;
	} else {
		burst_data_len = BURST_DATA_LEN_NORMAL;
	}

	ret = pmw3610_read(PMW3610_BURST_READ, burst_data, burst_data_len);
	if (ret < 0) {
		return false;
	}
    //motion reset
    // pmw3610_write_reg(PMW3610_MOTION, 0);

	if ((burst_data[BURST_MOTION] & MOTION_STATUS_MOTION) == 0x00) {
		return false;
	}

	x = ((burst_data[BURST_DELTA_XY_H] << 4) & 0xf00) | burst_data[BURST_DELTA_X_L];
	y = ((burst_data[BURST_DELTA_XY_H] << 8) & 0xf00) | burst_data[BURST_DELTA_Y_L];

	x = sign_extend(x, PMW3610_DATA_SIZE_BITS - 1);
	y = sign_extend(y, PMW3610_DATA_SIZE_BITS - 1);

    *x_out = x;
    *y_out = y;
    
    
    

    return true;

	// if (cfg->smart_mode) {
	// 	uint16_t shutter_val = sys_get_be16(&burst_data[BURST_SHUTTER_HI]);

	// 	if (data->smart_flag && shutter_val < SHUTTER_SMART_THRESHOLD) {
	// 		pmw3610_spi_clk_on();

	// 		ret = pmw3610_write_reg(PMW3610_SMART_MODE, SMART_MODE_ENABLE);
	// 		if (ret < 0) {
	// 			return;
	// 		}

	// 		pmw3610_spi_clk_off(dev);

	// 		data->smart_flag = false;
	// 	} else if (!data->smart_flag && shutter_val > SHUTTER_SMART_THRESHOLD) {
	// 		pmw3610_spi_clk_on();

	// 		ret = pmw3610_write_reg(PMW3610_SMART_MODE, SMART_MODE_DISABLE);
	// 		if (ret < 0) {
	// 			return;
	// 		}

	// 		pmw3610_spi_clk_off(dev);

	// 		data->smart_flag = true;
	// 	}
	// }
}


// static void pmw3610_motion_work_handler(struct k_work *work)
// {
// 	struct pmw3610_data *data = CONTAINER_OF(
// 			work, struct pmw3610_data, motion_work);
// 	const struct device *dev = data->dev;
// 	const struct pmw3610_config *cfg = dev->config;
// 	uint8_t burst_data[BURST_DATA_LEN_MAX];
// 	uint8_t burst_data_len;
// 	int32_t x, y;
// 	int ret;

// 	if (cfg->smart_mode) {
// 		burst_data_len = BURST_DATA_LEN_SMART;
// 	} else {
// 		burst_data_len = BURST_DATA_LEN_NORMAL;
// 	}

// 	ret = pmw3610_read(PMW3610_BURST_READ, burst_data, burst_data_len);
// 	if (ret < 0) {
// 		return;
// 	}

// 	if ((burst_data[BURST_MOTION] & MOTION_STATUS_MOTION) == 0x00) {
// 		return;
// 	}

// 	x = ((burst_data[BURST_DELTA_XY_H] << 4) & 0xf00) | burst_data[BURST_DELTA_X_L];
// 	y = ((burst_data[BURST_DELTA_XY_H] << 8) & 0xf00) | burst_data[BURST_DELTA_Y_L];

// 	x = sign_extend(x, PMW3610_DATA_SIZE_BITS - 1);
// 	y = sign_extend(y, PMW3610_DATA_SIZE_BITS - 1);

// 	input_report_rel(data->cfg->axis_x, x, false, K_FOREVER);
// 	input_report_rel(data->cfg->axis_y, y, true, K_FOREVER);

// 	if (cfg->smart_mode) {
// 		uint16_t shutter_val = sys_get_be16(&burst_data[BURST_SHUTTER_HI]);

// 		if (data->smart_flag && shutter_val < SHUTTER_SMART_THRESHOLD) {
// 			pmw3610_spi_clk_on();

// 			ret = pmw3610_write_reg(PMW3610_SMART_MODE, SMART_MODE_ENABLE);
// 			if (ret < 0) {
// 				return;
// 			}

// 			pmw3610_spi_clk_off(dev);

// 			data->smart_flag = false;
// 		} else if (!data->smart_flag && shutter_val > SHUTTER_SMART_THRESHOLD) {
// 			pmw3610_spi_clk_on();

// 			ret = pmw3610_write_reg(PMW3610_SMART_MODE, SMART_MODE_DISABLE);
// 			if (ret < 0) {
// 				return;
// 			}

// 			pmw3610_spi_clk_off(dev);

// 			data->smart_flag = true;
// 		}
// 	}
// }

// static void pmw3610_motion_handler(const struct device *gpio_dev,
// 				   struct gpio_callback *cb,
// 				   uint32_t pins)
// {
// 	struct pmw3610_data *data = CONTAINER_OF(
// 			cb, struct pmw3610_data, motion_cb);

// 	k_work_submit(&data->motion_work);
// }

int pmw3610_set_resolution(uint16_t res_cpi)
{
    uint8_t val;
    int ret;

    if (!IN_RANGE(res_cpi, RES_MIN, RES_MAX))
    {
        LOG_ERR("res_cpi out of range: %d", res_cpi);
        return -EINVAL;
    }

    ret = pmw3610_spi_clk_on();
    if (ret < 0)
    {
        return ret;
    }

    ret = pmw3610_write_reg(PMW3610_SPI_PAGE0, SPI_PAGE0_1);
    if (ret < 0)
    {
        return ret;
    }

    ret = pmw3610_read_reg(PMW3610_RES_STEP, &val);
    if (ret < 0)
    {
        return ret;
    }

    val &= ~RES_STEP_RES_MASK;
    val |= res_cpi / RES_STEP;

    ret = pmw3610_write_reg(PMW3610_RES_STEP, val);
    if (ret < 0)
    {
        return ret;
    }

    ret = pmw3610_write_reg(PMW3610_SPI_PAGE1, SPI_PAGE1_0);
    if (ret < 0)
    {
        return ret;
    }

    ret = pmw3610_spi_clk_off();
    if (ret < 0)
    {
        return ret;
    }

    return 0;
}

int pmw3610_force_awake(bool enable)
{
    uint8_t val;
    int ret;

    ret = pmw3610_read_reg(PMW3610_PERFORMANCE, &val);
    if (ret < 0)
    {
        return ret;
    }

    val &= ~PERFORMANCE_FMODE_MASK;
    if (enable)
    {
        val |= PERFORMANCE_FMODE_FORCE_AWAKE;
    }
    else
    {
        val |= PERFORMANCE_FMODE_NORMAL;
    }

    ret = pmw3610_spi_clk_on();
    if (ret < 0)
    {
        return ret;
    }

    ret = pmw3610_write_reg(PMW3610_PERFORMANCE, val);
    if (ret < 0)
    {
        return ret;
    }

    ret = pmw3610_spi_clk_off();
    if (ret < 0)
    {
        return ret;
    }

    return 0;
}

static int pmw3610_configure(void)
{
    const struct pmw3610_config *cfg = &pmw3610_cfg;
    uint8_t val;
    int ret;

    // if (cfg->reset_gpio.port != NULL)
    // {
    //     if (!gpio_is_ready_dt(&cfg->reset_gpio))
    //     {
    //         LOG_ERR("%s is not ready", cfg->reset_gpio.port->name);
    //         return -ENODEV;
    //     }

    //     ret = gpio_pin_configure_dt(&cfg->reset_gpio, GPIO_OUTPUT_ACTIVE);
    //     if (ret != 0)
    //     {
    //         LOG_ERR("Reset pin configuration failed: %d", ret);
    //         return ret;
    //     }

    //     delay_us(RESET_DELAY_MS);

    //     gpio_pin_set_dt(&cfg->reset_gpio, 0);

    //     delay_us(RESET_DELAY_MS);
    // }
    // else
    // {
    ret = pmw3610_write_reg(PMW3610_POWER_UP_RESET, POWER_UP_RESET);
    if (ret < 0)
    {
        return ret;
    }

    delay_us(RESET_DELAY_MS);
    // }

    ret = pmw3610_read_reg(PMW3610_PROD_ID, &val);
    if (ret < 0)
    {
        return ret;
    }

    if (val != PRODUCT_ID_PMW3610)
    {
        LOG_ERR("Invalid product id: %02x", val);
        return -ENOTSUP;
    }

    /* Power-up init sequence */
    ret = pmw3610_spi_clk_on();
    if (ret < 0)
    {
        return ret;
    }

    ret = pmw3610_write_reg(PMW3610_OBSERVATION1, 0);
    if (ret < 0)
    {
        return ret;
    }

    k_sleep(K_MSEC(INIT_OBSERVATION_DELAY_MS));

    ret = pmw3610_read_reg(PMW3610_OBSERVATION1, &val);
    if (ret < 0)
    {
        return ret;
    }

    if ((val & OBSERVATION1_INIT_MASK) != OBSERVATION1_INIT_MASK)
    {
        LOG_ERR("Unexpected OBSERVATION1 value: %02x", val);
        return -EINVAL;
    }

    for (uint8_t reg = PMW3610_MOTION; reg <= PMW3610_DELTA_XY_H; reg++)
    {
        ret = pmw3610_read_reg(reg, &val);
        if (ret < 0)
        {
            return ret;
        }
    }

    ret = pmw3610_write_reg(PMW3610_PERFORMANCE, PERFORMANCE_INIT);
    if (ret < 0)
    {
        return ret;
    }

    ret = pmw3610_write_reg(PMW3610_RUN_DOWNSHIFT, RUN_DOWNSHIFT_INIT);
    if (ret < 0)
    {
        return ret;
    }

    ret = pmw3610_write_reg(PMW3610_REST1_RATE, REST1_RATE_INIT);
    if (ret < 0)
    {
        return ret;
    }

    ret = pmw3610_write_reg(PMW3610_REST1_DOWNSHIFT, REST1_DOWNSHIFT_INIT);
    if (ret < 0)
    {
        return ret;
    }

    /* Configuration */

    if (cfg->invert_x || cfg->invert_y)
    {
        ret = pmw3610_write_reg(PMW3610_SPI_PAGE0, SPI_PAGE0_1);
        if (ret < 0)
        {
            return ret;
        }

        ret = pmw3610_read_reg(PMW3610_RES_STEP, &val);
        if (ret < 0)
        {
            return ret;
        }

        WRITE_BIT(val, RES_STEP_INV_X_BIT, cfg->invert_x);
        WRITE_BIT(val, RES_STEP_INV_Y_BIT, cfg->invert_y);

        ret = pmw3610_write_reg(PMW3610_RES_STEP, val);
        if (ret < 0)
        {
            return ret;
        }

        ret = pmw3610_write_reg(PMW3610_SPI_PAGE1, SPI_PAGE1_0);
        if (ret < 0)
        {
            return ret;
        }
    }

    ret = pmw3610_spi_clk_off();
    if (ret < 0)
    {
        return ret;
    }

    /* The remaining functions call spi_clk_on/off independently. */

    if (cfg->res_cpi > 0)
    {
        pmw3610_set_resolution(cfg->res_cpi);
    }

    pmw3610_force_awake(cfg->force_awake);

    return 0;
}

bool pmw3610_init(void)
{
    const struct pmw3610_config *cfg = &pmw3610_cfg;

    int ret;

    // if (!spi_is_ready_dt(&cfg->spi))
    // {
    //     LOG_ERR("%s is not ready", cfg->spi.bus->name);
    //     return -ENODEV;
    // }

    // data->dev = dev;

    // k_work_init(&data->motion_work, pmw3610_motion_work_handler);

    // if (!gpio_is_ready_dt(&cfg->motion_gpio))
    // {
    //     LOG_ERR("%s is not ready", cfg->motion_gpio.port->name);
    //     return -ENODEV;
    // }

    // ret = gpio_pin_configure_dt(&cfg->motion_gpio, GPIO_INPUT);
    // if (ret != 0)
    // {
    //     LOG_ERR("Motion pin configuration failed: %d", ret);
    //     return ret;
    // }

    // gpio_init_callback(&data->motion_cb, pmw3610_motion_handler,
    //                    BIT(cfg->motion_gpio.pin));

    // ret = gpio_add_callback_dt(&cfg->motion_gpio, &data->motion_cb);
    // if (ret < 0)
    // {
    //     LOG_ERR("Could not set motion callback: %d", ret);
    //     return ret;
    // }

    ret = pmw3610_configure();
    if (ret != 0)
    {
        LOG_ERR("Device configuration failed: %d", ret);
        return false;
    }

    // ret = gpio_pin_interrupt_configure_dt(&cfg->motion_gpio,
    //                                       GPIO_INT_EDGE_TO_ACTIVE);
    // if (ret != 0)
    // {
    //     LOG_ERR("Motion interrupt configuration failed: %d", ret);
    //     return ret;
    // }

    // ret = pm_device_runtime_enable(dev);
    // if (ret < 0)
    // {
    //     LOG_ERR("Failed to enable runtime power management: %d", ret);
    //     return ret;
    // }

    return true;
}

#ifdef CONFIG_PM_DEVICE
static int pmw3610_pm_action(
    enum pm_device_action action)
{
    int ret;

    switch (action)
    {
    case PM_DEVICE_ACTION_SUSPEND:
        ret = pmw3610_write_reg(PMW3610_SHUTDOWN, SHUTDOWN_ENABLE);
        if (ret < 0)
        {
            return ret;
        }
        break;
    case PM_DEVICE_ACTION_RESUME:
        ret = pmw3610_write_reg(PMW3610_POWER_UP_RESET, POWER_UP_WAKEUP);
        if (ret < 0)
        {
            return ret;
        }
        break;
    default:
        return -ENOTSUP;
    }

    return 0;
}
#endif
