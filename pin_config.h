#pragma once

// Pines para la Waveshare ESP32-S3-Touch-AMOLED-1.43
// Fuente: waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.43 (tabla de pines oficial)
//
// Esta placa NO tiene PMU AXP2101 (a diferencia de la 1.75): sin gestion de
// bateria por I2C, sin boton PWR dedicado. batBegin()/pwrSetup() son no-ops
// en esta rama (ver rtcbat.cpp).

// Pantalla AMOLED 466x466, driver SH8601 por QSPI (la 1.75 usa CO5300)
#define LCD_SDIO0 11
#define LCD_SDIO1 12
#define LCD_SDIO2 13
#define LCD_SDIO3 14
#define LCD_SCLK 10
#define LCD_CS 9
#define LCD_RESET 21
#define LCD_EN 42          // habilita el rail de la pantalla; la 1.75 no tiene este pin (lo hace el PMU)
#define LCD_WIDTH 466
#define LCD_HEIGHT 466

// Tactil capacitivo FT3168, bus I2C compartido con IMU y RTC
#define IIC_SDA 47
#define IIC_SCL 48
// TP_INT / TP_RESET: NO confirmados en la tabla de pines publica de Waveshare.
// A verificar contra el esquematico antes de flashear:
// https://files.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.43/ESP32-S3-Touch-AMOLED-1.43-Schematic.pdf
#define TP_INT -1    // PLACEHOLDER -- confirmar en el esquematico
#define TP_RESET -1  // PLACEHOLDER -- confirmar en el esquematico

// IMU QMI8658 (mismo bus I2C)
#define IMU_INT 8

// RTC PCF85063 (mismo bus I2C)
#define RTC_INT 15

// Bateria: solo divisor resistivo simple leido por ADC, sin PMU dedicada
#define BAT_ADC 4

// Ranura TF, cableada en SPI dedicado (no SDMMC 4-bit)
#define SDMMC_CLK 41
#define SDMMC_CMD 39   // = MOSI
#define SDMMC_DATA 40  // = MISO
#define SDMMC_CS 38
