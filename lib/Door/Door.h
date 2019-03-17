#ifndef Door_h
#define Door_h

#define T_OPEN      5000
#define T_UNLOCK    30000L
#define T_BLINK     500
#define T_CARD      30000L

#include "Arduino.h"

class Door {
  public:
    Door(int door_pin, int button_pin, int led_pin);
    void lock();
    void unlock();
    void open();
    void add_card();
    void update();
    uint8_t mode; // modos: travado(0), liberado(1), aberto(2), add cartao(3)

  private:
    int _door_pin;
    int _button_pin;
    int _led_pin;
    unsigned long _temp;
    unsigned long _led_temp;
};

#endif
