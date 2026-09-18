#pragma once
#include "board_select.h"

// ===========================================================================
// Waveshare ESP32-S3-Touch-AMOLED-1.75
// Fuente: github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75
//   (libraries/Mylibrary/pin_config.h), y verificado en placa (rtcbat.cpp/
//   audio.cpp originales, proyecto PlaneRadar2.0).
// ===========================================================================
#if defined(TAMAPOKE_BOARD_175)

#define TAMAPOKE_HAS_PMU 1              // AXP2101
#define TAMAPOKE_HAS_AUDIO_ES8311 1
#define TAMAPOKE_DISPLAY_QSPI_AMOLED 1  // CO5300 por QSPI, igual forma que SH8601
#define TAMAPOKE_SD_NATIVE_SDMMC 1      // SD_MMC 1-bit, pines propios (no compartidos)

#define XPOWERS_CHIP_AXP2101

// Pantalla AMOLED 466x466, driver CO5300 por QSPI
#define LCD_SDIO0 4
#define LCD_SDIO1 5
#define LCD_SDIO2 6
#define LCD_SDIO3 7
#define LCD_SCLK 38
#define LCD_CS 12
#define LCD_RESET 39
#define LCD_WIDTH 466
#define LCD_HEIGHT 466

// Tactil capacitivo CST9217 por I2C
#define IIC_SDA 15
#define IIC_SCL 14
#define TP_INT 11
#define TP_RESET 40

// Audio ES8311. El MCLK real es GPIO42 (verificado en placa); el codec se
// configura con reloj derivado del BCLK, asi que el MCLK apenas importa.
#define I2S_MCK_IO 42
#define I2S_BCK_IO 9
#define I2S_DI_IO 10
#define I2S_WS_IO 45
#define I2S_DO_IO 8
#define PA 46

// Ranura TF, pines propios (SD_MMC 1-bit)
#define SDMMC_CLK 2
#define SDMMC_CMD 1
#define SDMMC_DATA 3
#define SDMMC_CS 41

// ===========================================================================
// Waveshare ESP32-S3-Touch-AMOLED-1.43
// Fuente: esquematico oficial (files.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.43/
//   ESP32-S3-Touch-AMOLED-1.43-Schematic.pdf) -- ver PORTAGE_1.43.md para el
//   detalle de cada pin y de los dos hallazgos de placa real (TP_INT/TP_RESET
//   no existen; el SH8601 necesita 0xFE/0xC4 antes que cualquier otro comando).
// ===========================================================================
#elif defined(TAMAPOKE_BOARD_143)

// Esta placa NO tiene PMU AXP2101 (a diferencia de la 1.75): sin gestion de
// bateria por I2C, sin boton PWR dedicado. batBegin()/pwrSetup() son no-ops
// (ver rtcbat.cpp). Tampoco tiene codec ES8311 (ver audio.cpp, stub) -- el
// esquematico no tiene ni un bloque de audio.
#undef TAMAPOKE_HAS_PMU
#undef TAMAPOKE_HAS_AUDIO_ES8311
#define TAMAPOKE_DISPLAY_QSPI_AMOLED 1  // SH8601 por QSPI
#define TAMAPOKE_SD_SPI_DEDICATED 1     // SD por SPI, pines propios (no SDMMC)

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
// TP_INT / TP_RESET: confirmado contra el esquematico oficial que esta placa
// NO los expone como pines propios -- el conector J9 "LCD" (modulo de
// pantalla con tactil integrado) solo saca TP_SDA/TP_SCL hacia el bus I2C
// compartido con el IMU y el RTC. No son placeholders pendientes de
// verificar: el touch funciona en polling puro (ver handleTouch en
// TamaPoke.ino), que es el modo correcto para esta placa.
#define TP_INT -1
#define TP_RESET -1

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

// ===========================================================================
// Waveshare ESP32-S3 2.8inch Capacitive Touch Round Display (ESP32-S3-Touch-LCD-2.8C)
// Fuente: esquematico oficial (files.waveshare.com/wiki/ESP32-S3-Touch-LCD-2.8C/
//   ESP32-S3-Touch-LCD-2.8C_schematic_diagram.pdf) y el codigo de ejemplo
//   oficial (.../ESP32-S3-Touch-LCD-2.8C-Demo.zip, carpeta Arduino/examples/
//   LVGL_Arduino: Display_ST7701.cpp, TCA9554PWR.cpp, Touch_GT911.cpp).
//
// NUNCA PROBADO EN PLACA -- ver PORTAGE_28ROUND.md. Arquitectura muy distinta
// de las otras dos: panel RGB565 paralelo (16 lineas de datos + HSYNC/VSYNC/
// DE/PCLK), no QSPI, y varias lineas de control (LCD_RESET, LCD_CS, TP_RESET,
// SD_CS) van detras de un expansor I2C TCA9554, no en GPIO directo.
// ===========================================================================
#elif defined(TAMAPOKE_BOARD_28ROUND)

#undef TAMAPOKE_HAS_PMU              // solo un IC cargador (ETA6098) + LDO, sin PMU
#undef TAMAPOKE_HAS_AUDIO_ES8311     // sin codec de audio en el esquematico
#define TAMAPOKE_DISPLAY_RGB_ST7701 1
#define TAMAPOKE_SD_SPI_SHARED_LCD 1  // SD por SPI, bus COMPARTIDO con el init del LCD; CS por el expansor

#define LCD_WIDTH 480
#define LCD_HEIGHT 480

// Panel RGB565 (16 bits): orden b0..b4, g0..g5, r0..r4 -- el mismo orden que
// espera Arduino_ESP32RGBPanel (useBigEndian=false). Los nombres de señal del
// esquematico (B1..B5/G0..G5/R1..R5) son del propio panel; lo que importa es
// la POSICION en el bus, no la etiqueta.
#define LCD_DE 40
#define LCD_VSYNC 39
#define LCD_HSYNC 38
#define LCD_PCLK 41
#define LCD_B0 5
#define LCD_B1 45
#define LCD_B2 48
#define LCD_B3 47
#define LCD_B4 21
#define LCD_G0 14
#define LCD_G1 13
#define LCD_G2 12
#define LCD_G3 11
#define LCD_G4 10
#define LCD_G5 9
#define LCD_R0 46
#define LCD_R1 3
#define LCD_R2 8
#define LCD_R3 18
#define LCD_R4 17
#define LCD_BACKLIGHT 6  // PWM

// Sub-bus de 3 hilos para el init del ST7701 (registros, no pixeles): MOSI/
// SCK son GPIO reales; el CS y el RESET del panel van por el expansor.
#define LCD_INIT_MOSI 1
#define LCD_INIT_SCK 2

// Bus I2C principal: tactil GT911, IMU QMI8658, RTC PCF85063 y el expansor
// TCA9554, todos en el mismo bus.
#define IIC_SDA 15
#define IIC_SCL 7
#define TP_INT 16
// TP_RESET NO es un GPIO directo -- va por el expansor (ver TCA9554_EXIO2).

// Expansor TCA9554 (direccion I2C 0x20): que pin de placa hace que.
#define EXIO_LCD_RESET 1
#define EXIO_TP_RESET 2
#define EXIO_LCD_CS 3
#define EXIO_SD_CS 4
#define EXIO_IMU_INT2 5
#define EXIO_IMU_INT1 6
#define EXIO_RTC_INT 7
#define EXIO_BUZZER 8

// IMU_INT1/INT2 y RTC_INT van los TRES detras del expansor (EXIO_IMU_INT1/
// EXIO_IMU_INT2/EXIO_RTC_INT) -- ningun GPIO directo del ESP32 los expone, a
// diferencia de la 1.43. No hay IRQ de verdad posible sin leer el expansor
// por I2C de todos modos, asi que no se pierde nada: como TP_INT/TP_RESET en
// la 1.43, -1 aqui es el valor correcto, no un placeholder.
#define IMU_INT -1
#define RTC_INT -1

#define BAT_ADC 4

// Ranura TF, bus SPI COMPARTIDO con el init del LCD (mismos MOSI/SCK que
// LCD_INIT_MOSI/LCD_INIT_SCK); CS por el expansor.
#define SDMMC_CLK LCD_INIT_SCK
#define SDMMC_CMD LCD_INIT_MOSI
#define SDMMC_DATA 42     // MISO -- unico pin de datos propio de la SD en este bus

#else
#error "pin_config.h: ninguna TAMAPOKE_BOARD_* definida -- revisa board_select.h"
#endif
