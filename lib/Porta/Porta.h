#ifndef Door_h
#define Door_h

#define T_OPEN     10000
#define T_UNLOCK    60000L
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

  private:
    int _door_pin;
    int _button_pin;
    int _led_pin;
    uint8_t _mode;
    unsigned long _temp;
    unsigned long _led_temp;
};

#endif
