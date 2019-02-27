#include "Arduino.h"
#include "LED.h"

  LED::LED(int pin) {
    pinMode(pin, OUTPUT);
    _pin = pin;
    _state = false;
    _mode =_lastms = _ms = 0;
  }
  
  void LED::update() {
    if(_mode != 0 && millis() > _lastms + _ms) {
      if(_mode == 1) {
        _state = false;
        _mode = 0;
        digitalWrite(_pin, LOW);
      } else {
        _lastms = millis();
        _state = !_state;
        digitalWrite(_pin, _state);
      }
    }
  }

  void LED::blink(unsigned long ms) {
    _mode = 2;
    _lastms = millis();
    _ms = ms;
    _state = true;
    digitalWrite(_pin, HIGH);
  }

  void LED::pulse(unsigned long ms) {
    _mode = 1;
    _lastms = millis();
    _ms = ms;
    _state = true;
    digitalWrite(_pin, HIGH);
  }

  bool LED::getState() {
    return _state;
  }

  void LED::setState(bool s) {
    _mode = 0;
    _state = s;
    digitalWrite(_pin, s);    
  }
