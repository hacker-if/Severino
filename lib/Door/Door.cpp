#include "Arduino.h"
#include "Door.h"

Door::Door(int door_pin, int button_pin, int led_pin) {
  pinMode(door_pin, OUTPUT);
  pinMode(led_pin, OUTPUT);
  pinMode(button_pin, INPUT_PULLUP);

  _led_pin = led_pin;
  _button_pin = button_pin;
  _door_pin = door_pin;

  mode =_temp = _led_temp = 0;
}

void Door::unlock() {
  _temp = millis();
  mode = 1;
  digitalWrite(_led_pin, HIGH);
}

void Door::lock() {
  mode = 0;
  digitalWrite(_led_pin, LOW);
  digitalWrite(_door_pin, LOW);
}

void Door::open() {
  _temp = millis();
  mode = 2;
  digitalWrite(_door_pin, HIGH);
  digitalWrite(_led_pin, LOW);
}

void Door::add_card() {
  _temp= millis();
  mode = 3;
}

void Door::update() {
  if (
    (mode == 1 && millis() - _temp > T_UNLOCK)  ||
    (mode == 2 && millis() - _temp > T_OPEN)    ||
    (mode == 3 && millis() - _temp > T_CARD)
    ) {
    lock();
  }

  if (mode == 1 && digitalRead(_button_pin) == LOW) {
    open();
  }

  if (mode == 3 && digitalRead(_button_pin) == LOW) {
    lock();
  }

  if (mode == 3 && millis() - _led_temp > T_BLINK) {
    digitalWrite(_led_pin, !digitalRead(_led_pin));
    _led_temp = millis();
  }
}
