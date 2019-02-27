#ifndef LED_h
#define LED_h

#include "Arduino.h"

class LED {
  public:
    LED(int pin);
    void update();
    void blink(unsigned long ms);
    void pulse(unsigned long ms);
    bool getState();
    void setState(bool s); 

  private:
  int _pin;
  bool _state;
  uint8_t _mode;
  unsigned long _lastms;
  unsigned long _ms;
};

#endif
