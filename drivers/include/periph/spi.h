/*
 * Copyright (C) 2014-2016 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup    drivers_periph_spi SPI
 * @ingroup     drivers_periph
 * @brief       Low-level SPI peripheral driver
 *
 * This interface defines an abstraction for using a CPU's hardware SPI units.
 * The interface only supports SPI master mode.
 *
 * As SPI buses can have multiple devices connected to them they are to be
 * considered as shared resources. To reflect this, the SPI interface is based
 * on a transaction model. This requires that the bus needs to be acquired
 * before usage and released afterwards, using the `spi_acquire()` and the
 * `spi_release()` functions.
 *
 * This interface supports both software and hardware chip select lines. This is
 * reflected by the cpi_cs_t type, which overloads the gpio_t type with platform
 * specific values for defining platform dependent hardware chip select lines.
 *
 * Some devices have however very uncommon requirements on the usage and the
 * timings of their chip select line. For those cases this interface allows to
 * manage the chip select line manually from the user code (e.g. by calling
 * gpio_set/clear explicitly) while deactivating the SPI driver internal chip
 * select handling by passing @ref GPIO_UNDEF as CS parameter.
 *
 * In the time, when the SPI bus is not used, the SPI unit should be in
 * low-power mode to save energy.
 *
 * The SPI unit's initialization is split into 3 parts:
 * 1. `spi_init()` should be called once for each SPI unit defined by a board
 *    during system initialization.
 * 2. `spi_init_cs()` should be called during device driver initialization, as
 *    each chip select pin/line is used uniquely by a specific device, i.e. chip
 *    select lines are no shared resource.
 * 3. `spi_aquire()` needs to be called for each new transaction. This function
 *    configures the bus with specific parameters (clock, mode) for the duration
 *    of that transaction.
 *
 * @{
 * @file
 * @brief       Low-level SPI peripheral driver interface definition
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 */

#ifndef PERIPH_SPI_H
#define PERIPH_SPI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#include "periph_cpu.h"
#include "periph_conf.h"
#include "periph/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Default SPI device access macro
 */
#ifndef SPI_DEV
#define SPI_DEV(x)      (x)
#endif

/**
 * @brief   Define global value for undefined SPI device
 */
#ifndef SPI_UNDEF
#define SPI_UNDEF       (UINT_MAX)
#endif

/**
 * @brief   Define value for unused CS line
 */
#ifndef SPI_CS_UNDEF
#define SPI_CS_UNDEF    (GPIO_UNDEF)
#endif

/**
 * @brief   Default SPI hardware chip select access macro
 *
 * Per default, we map all hardware chip select lines to be not defined. If an
 * implementation makes use of HW chip select lines, this value needs to be
 * overridden by the corresponding CPU.
 */
#ifndef SPI_HWCS
#define SPI_HWCS(x)     (SPI_CS_UNDEF)
#endif

/**
 * @brief   Default type for SPI devices
 */
#ifndef HAVE_SPI_T
typedef unsigned int spi_t;
#endif

/**
 * @brief   Chip select pin type overlaps with gpio_t so it can be casted to
 *          this
 */
#ifndef HAVE_SPI_CS_T
typedef gpio_t spi_cs_t;
#endif

/**
 * @brief   Status codes used by the SPI driver interface
 */
enum {
    SPI_OK          =  0,   /**< everything went as planned */
    SPI_NODEV       = -1,   /**< invalid SPI bus specified */
    SPI_NOCS        = -2,   /**< invalid chip select line specified */
    SPI_NOMODE      = -3,   /**< selected mode is not supported */
    SPI_NOCLK       = -4    /**< selected clock value is not supported */
};

/**
 * @brief   Available SPI modes, defining the configuration of clock polarity
 *          and clock phase
 *
 * RIOT is using the mode numbers as commonly defined by most vendors
 * (https://en.wikipedia.org/wiki/Serial_Peripheral_Interface_Bus#Mode_numbers):
 *
 * - MODE_0: CPOL=0, CPHA=0 - The first data bit is sampled by the receiver on
 *           the first SCK rising SCK edge (this mode is used most often).
 * - MODE_1: CPOL=0, CPHA=1 - The first data bit is sampled by the receiver on
 *           the second rising SCK edge.
 * - MODE_2: CPOL=1, CPHA=0 - The first data bit is sampled by the receiver on
 *           the first falling SCK edge.
 * - MODE_3: CPOL=1, CPHA=1 - The first data bit is sampled by the receiver on
 *           the second falling SCK edge.
 */
#ifndef HAVE_SPI_MODE_T
typedef enum {
    SPI_MODE_0 = 0,         /**< CPOL=0, CPHA=0 */
    SPI_MODE_1,             /**< CPOL=0, CPHA=1 */
    SPI_MODE_2,             /**< CPOL=1, CPHA=0 */
    SPI_MODE_3              /**< CPOL=1, CPHA=1 */
} spi_mode_t;
#endif

/**
 * @brief   Available SPI clock speeds
 *
 * The actual speed of the bus can vary to some extend, as the combination of
 * CPU clock and available prescaler values on certain platforms may not make
 * the exact values possible.
 */
#ifndef HAVE_SPI_CLK_T
typedef enum {
    SPI_CLK_100KHZ = 0,     /**< drive the SPI bus with 100KHz */
    SPI_CLK_400KHZ,         /**< drive the SPI bus with 400KHz */
    SPI_CLK_1MHZ,           /**< drive the SPI bus with 1MHz */
    SPI_CLK_5MHZ,           /**< drive the SPI bus with 5MHz */
    SPI_CLK_10MHZ           /**< drive the SPI bus with 10MHz */
} spi_clk_t;
#endif

/**
 * @brief   Basic initialization of the given SPI bus
 *
 * This function does the basic initialization including pin configuration for
 * MISO, MOSI, and CLK pins. After initialization, the given device should be
 * in power down state.
 *
 * This function is intended to be called by the board initialization code
 * during system startup to prepare the (shared) SPI device for further usage.
 * It uses the board specific initialization parameters as defined in the
 * board's `periph_conf.h`.
 *
 * Errors (e.g. invalid @p bus parameter) are not signaled through a return
 * value, but should be signaled using the assert() function internally.
 *
 * @note    This function MUST not be called more than once per bus!
 *
 * @param[in] bus       SPI device to initialize
 */
void spi_init(spi_t bus);

/**
 * @brief   Initialize the used SPI bus pins, i.e. MISO, MOSI, and CLK
 *
 *
 * After calling spi_init, the pins must be initialized (i.e. spi_init is
 * calling) this function internally. In normal cases, this function will not be
 * used. But there are some devices (e.g. CC110x), that use SPI bus lines also
 * for other purposes and need the option to dynamically re-configure one or
 * more of the used pins. So they can take control over certain pins and return
 * control back to the SPI driver using this function.
 *
 * The pins used are configured in the board's periph_conf.h.
 *
 * @param[in] bus       SPI device the pins are configure for
 */
void spi_init_pins(spi_t bus);

/**
 * @brief   Initialize the given chip select pin
 *
 * The chip select can be any generic GPIO pin (e.g. GPIO_PIN(x,y)), or it can
 * be a hardware chip select line. The existence and number of of hardware chip
 * select lines depends on the underlying platform and the actual pins used for
 * hardware chip select lines are defined in the board's `periph_conf.h`.
 *
 * Define the used chip select line using the @ref SPI_HWCS(x) macro for
 * hardware chip select line `x` or the GPIO_PIN(x,y) macro for using any
 * GPIO pin for manual chip select.
 *
 * @param[in] bus       SPI device that is used with the given CS line
 * @param[in] cs        chip select pin to initialize
 *
 * @return              SPI_OK on success
 * @return              SPI_NODEV on invalid device
 * @return              SPI_NOCS on invalid CS pin/line
 */
int spi_init_cs(spi_t bus, spi_cs_t cs);

/**
 * @brief   Start a new SPI transaction
 *
 * Starting a new SPI transaction will get exclusive access to the SPI bus
 * and configure it according to the given values. If another SPI transaction
 * is active when this function is called, this function will block until the
 * other transaction is complete (spi_relase was called).
 *
 * @note    This function expects the @p bus and the @p cs parameters to be
 *          valid (they are checked in spi_init and spi_init_cs before)
 *
 * @param[in] bus       SPI device to access
 * @param[in] cs        chip select pin/line to use, set to SPI_CS_UNDEF if chip
 *                      select should not be handled by the SPI driver
 * @param[in] mode      mode to use for the new transaction
 * @param[in] clk       bus clock speed to use for the transaction
 *
 * @return              SPI_OK on success
 * @return              SPI_NOMODE if given mode is not supported
 * @return              SPI_NOCLK if given clock speed is not supported
 */
int spi_acquire(spi_t bus, spi_cs_t cs, spi_mode_t mode, spi_clk_t clk);

/**
 * @brief   Finish an ongoing SPI transaction by releasing the given SPI bus
 *
 * After release, the given SPI bus should be fully powered down until acquired
 * again.
 *
 * @param[in] bus       SPI device to release
 */
void spi_release(spi_t bus);

/**
 * @brief Transfer one byte on the given SPI bus
 *
 * @param[in] bus       SPI device to use
 * @param[in] cs        chip select pin/line to use, set to SPI_CS_UNDEF if chip
 *                      select should not be handled by the SPI driver
 * @param[in] cont      if true, keep device selected after transfer
 * @param[in] out       byte to send out, set NULL if only receiving
 *
 * @return              the received byte
 */
uint8_t spi_transfer_byte(spi_t bus, spi_cs_t cs, bool cont, uint8_t out);

/**
 * @brief   Transfer a number bytes using the given SPI bus
 *
 * @param[in]  bus      SPI device to use
 * @param[in]  cs       chip select pin/line to use, set to SPI_CS_UNDEF if chip
 *                      select should not be handled by the SPI driver
 * @param[in]  cont     if true, keep device selected after transfer
 * @param[in]  out      buffer to send data from, set NULL if only receiving
 * @param[out] in       buffer to read into, set NULL if only sending
 * @param[in]  len      number of bytes to transfer
 */
void spi_transfer_bytes(spi_t bus, spi_cs_t cs, bool cont,
                        const void *out, void *in, size_t len);

/**
 * @brief   Transfer one byte to/from a given register address
 *
 * This function is a shortcut function for easier handling of SPI devices that
 * implement a register based access scheme.
 *
 * @param[in] bus       SPI device to use
 * @param[in]  cs       chip select pin/line to use, set to SPI_CS_UNDEF if chip
 *                      select should not be handled by the SPI driver
 * @param[in] reg       register address to transfer data to/from
 * @param[in] out       byte to send, set NULL if only receiving data
 *
 * @return              value that was read from the given register address
 */
uint8_t spi_transfer_reg(spi_t bus, spi_cs_t cs, uint8_t reg, uint8_t out);

/**
 * @brief   Transfer a number of bytes to/from a given register address
 *
 * This function is a shortcut function for easier handling of SPI devices that
 * implement a register based access scheme.
 *
 * @param[in]  bus      SPI device to use
 * @param[in]  cs       chip select pin/line to use, set to SPI_CS_UNDEF if chip
 *                      select should not be handled by the SPI driver
 * @param[in]  reg      register address to transfer data to/from
 * @param[in]  out      buffer to send data from, set NULL if only receiving
 * @param[out] in       buffer to read into, set NULL if only sending
 * @param[in]  len      number of bytes to transfer
 */
void spi_transfer_regs(spi_t bus, spi_cs_t cs, uint8_t reg,
                       const void *out, void *in, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* PERIPH_SPI_H */
/** @} */
