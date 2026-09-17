// Touch stub para el emulador de escritorio, PORTAGE 1.43: mismo shim que
// TouchDrvCSTXXX.hpp original pero bajo el nombre de include que usa
// TamaPoke.ino para la placa 1.43 (TouchDrvFT6X36.hpp / FT3168).
// La capa SDL escribe la posicion/estado del raton en estos globales y el
// sketch los lee a traves de la misma API que el driver real.
#pragma once
#include "Arduino.h"

extern volatile int g_touchX, g_touchY;
extern volatile bool g_touchDown;

class TouchDrvFT6X36 {
public:
  void setPins(int, int) {}
  bool begin(class FakeWire &, uint8_t, int, int) { return true; }
  void reset() {}
  void setMaxCoordinates(int, int) {}
  void setMirrorXY(bool, bool) {}
  uint8_t getPoint(int16_t *x, int16_t *y, uint8_t) {
    if (!g_touchDown) return 0;
    *x = g_touchX;
    *y = g_touchY;
    return 1;
  }
};
