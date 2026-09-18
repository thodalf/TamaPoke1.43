#pragma once

// Elige UNA placa de destino. Dos formas de hacerlo, en orden de prioridad:
//
// 1) Build flag (lo que usa tools/build_web.sh para compilar las tres):
//      arduino-cli compile --build-property "build.extra_flags=-DTAMAPOKE_BOARD_143" .
//    Si CUALQUIERA de las tres ya llego definida por build flag, este archivo
//    no toca nada -- por eso el #if de abajo.
//
// 2) Editar la linea activa aqui abajo (Arduino IDE, sin build flags):
//    deja UNA sola descomentada.
#if !defined(TAMAPOKE_BOARD_175) && !defined(TAMAPOKE_BOARD_143) && !defined(TAMAPOKE_BOARD_28ROUND)

#define TAMAPOKE_BOARD_143
// #define TAMAPOKE_BOARD_175
// #define TAMAPOKE_BOARD_28ROUND

#endif

#if defined(TAMAPOKE_BOARD_175) + defined(TAMAPOKE_BOARD_143) + defined(TAMAPOKE_BOARD_28ROUND) != 1
#error "board_select.h: exactamente UNA de TAMAPOKE_BOARD_175 / _143 / _28ROUND debe estar definida"
#endif
