/**
 * @file printer_driver.h
 * @brief USB Printer Driver for ESP32 using ESP-IDF
 * 
 * This driver handles USB host communication with thermal printers
 * supporting ESC/POS commands and the USB Printer Class.
 * 
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the USB printer driver
 * 
 * This function starts the USB host, creates necessary tasks, and prepares
 * the driver for printer detection and communication.
 * 
 * @return esp_err_t 
 *         - ESP_OK: Driver initialized successfully
 *         - ESP_ERR_NO_MEM: Memory allocation failed
 *         - ESP_FAIL: Driver initialization failed
 */
esp_err_t printer_init(void);

/**
 * @brief Deinitialize the USB printer driver
 * 
 * Stops the USB host, cleans up resources, and deletes tasks.
 * Should be called when printer functionality is no longer needed.
 */
void printer_deinit(void);

/**
 * @brief Send raw data to printer
 * 
 * Sends raw binary data to the printer. The data is queued and sent
 * asynchronously. Supports ESC/POS commands and any printer-specific protocols.
 * 
 * @param data Pointer to data buffer
 * @param length Length of data in bytes
 * @return esp_err_t 
 *         - ESP_OK: Data queued successfully
 *         - ESP_ERR_INVALID_STATE: Driver not initialized
 *         - ESP_ERR_INVALID_ARG: Invalid parameters
 *         - ESP_ERR_INVALID_SIZE: Data too large
 *         - ESP_ERR_NO_MEM: Print queue full
 */
esp_err_t printer_send_raw(const uint8_t *data, size_t length);

/**
 * @brief Send text string to printer
 * 
 * Convenience function to send text strings to the printer.
 * The text is sent as-is without any formatting.
 * 
 * @param text Null-terminated string to print
 * @return esp_err_t 
 *         - ESP_OK: Text queued successfully
 *         - ESP_ERR_INVALID_STATE: Driver not initialized
 *         - ESP_ERR_INVALID_ARG: Invalid text pointer
 */
esp_err_t printer_send_text(const char *text);

/**
 * @brief Check if printer is ready
 * 
 * Returns the current readiness state of the printer.
 * The printer is ready when a compatible USB printer is connected,
 * claimed, and initialized.
 * 
 * @return true Printer is connected and ready
 * @return false Printer is not connected or not ready
 */
bool printer_is_ready(void);

// ============================================
// ESC/POS COMMAND DEFINITIONS
// ============================================

/**
 * @brief ESC/POS command definitions
 * 
 * Common ESC/POS commands for thermal printer control.
 * Use these with printer_send_raw() for formatted printing.
 */

// Text alignment
#define ESC_ALIGN_LEFT          "\x1B\x61\x00"    ///< Align text left
#define ESC_ALIGN_CENTER        "\x1B\x61\x01"    ///< Align text center  
#define ESC_ALIGN_RIGHT         "\x1B\x61\x02"    ///< Align text right

// Character styles
#define ESC_BOLD_ON             "\x1B\x45\x01"    ///< Enable bold text
#define ESC_BOLD_OFF            "\x1B\x45\x00"    ///< Disable bold text
#define ESC_UNDERLINE_ON        "\x1B\x2D\x01"    ///< Enable underline
#define ESC_UNDERLINE_OFF       "\x1B\x2D\x00"    ///< Disable underline
#define ESC_INVERSE_ON          "\x1B\x34"        ///< Enable inverse video
#define ESC_INVERSE_OFF         "\x1B\x35"        ///< Disable inverse video

// Character sizes
#define ESC_NORMAL_SIZE         "\x1D\x21\x00"    ///< Normal character size
#define ESC_DOUBLE_HEIGHT       "\x1B\x21\x10"    ///< Double height characters
#define ESC_DOUBLE_WIDTH        "\x1B\x21\x20"    ///< Double width characters
#define ESC_DOUBLE_SIZE_ON      "\x1D\x21\x11"    ///< Double height and width
#define ESC_DOUBLE_SIZE_OFF     ESC_NORMAL_SIZE   ///< Return to normal size

// Paper control
#define ESC_LINEFEED            "\x0A"            ///< Line feed
#define ESC_FEED_N(n)           "\x1B\x64"        ///< Feed n lines (use: ESC_FEED_N "\x03")
#define ESC_FEED_1              "\x1B\x64\x01"    ///< Feed 1 line
#define ESC_FEED_2              "\x1B\x64\x02"    ///< Feed 2 lines  
#define ESC_FEED_3              "\x1B\x64\x03"    ///< Feed 3 lines

// Cutting
#define ESC_CUT_PARTIAL         "\x1D\x56\x41\x03" ///< Partial paper cut
#define ESC_CUT_FULL            "\x1D\x56\x41\x00" ///< Full paper cut

// Initialization
#define ESC_INIT                "\x1B\x40"        ///< Initialize printer

// Barcode commands
#define ESC_BARCODE_HEIGHT(n)   "\x1D\x68"        ///< Set barcode height (use: ESC_BARCODE_HEIGHT "\x40")
#define ESC_BARCODE_WIDTH(n)    "\x1D\x77"        ///< Set barcode width (use: ESC_BARCODE_WIDTH "\x02")
#define ESC_BARCODE_TEXT_BELOW  "\x1D\x48\x02"    ///< Print barcode text below
#define ESC_BARCODE_TEXT_NONE   "\x1D\x48\x00"    ///< Don't print barcode text

// ============================================
// CONVENIENCE FUNCTIONS (Optional - could be implemented)
// ============================================

/**
 * @brief Send formatted text to printer
 * 
 * Note: This function would need to be implemented in printer_driver.c
 * Sends printf-style formatted text to the printer.
 * 
 * @param format Format string
 * @param ... Variable arguments
 * @return esp_err_t 
 */
// esp_err_t printer_printf(const char *format, ...);

/**
 * @brief Initialize printer with ESC/POS commands
 * 
 * Note: This function would need to be implemented in printer_driver.c
 * Sends initialization sequence to prepare the printer.
 * 
 * @return esp_err_t 
 */
// esp_err_t printer_initialize(void);

/**
 * @brief Feed specified number of lines
 * 
 * Note: This function would need to be implemented in printer_driver.c
 * Advances the paper by the specified number of lines.
 * 
 * @param lines Number of lines to feed
 * @return esp_err_t 
 */
// esp_err_t printer_feed_lines(uint8_t lines);

/**
 * @brief Perform paper cut
 * 
 * Note: This function would need to be implemented in printer_driver.c
 * Cuts the paper (partial or full cut).
 * 
 * @param full_cut True for full cut, false for partial cut
 * @return esp_err_t 
 */
// esp_err_t printer_cut_paper(bool full_cut);

#ifdef __cplusplus
}
#endif