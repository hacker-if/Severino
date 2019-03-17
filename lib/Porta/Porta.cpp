#include "Arduino.h"
#include "Porta.h"

// modos:  travado(0), liberado(1), aberto(2), adcionando cartao(3)

Door::Door(int door_pin, int button_pin, int led_pin) {
  pinMode(door_pin, OUTPUT);
  pinMode(led_pin, OUTPUT);
  pinMode(button_pin, INPUT_PULLUP);

  _led_pin = led_pin;
  _button_pin = button_pin;
  _door_pin = door_pin;

  _mode =_temp = _led_temp = 0;
}

void Door::unlock() {
  _temp = millis();
  _mode = 1;
  digitalWrite(_led_pin, HIGH);
  // Serial.println("Porta destravada");
  // mqtt_client.publish(mqtt_outTopic, "Porta destravada");
}

void Door::lock() {
  _mode = 0;
  digitalWrite(_led_pin, LOW);
  // Serial.println("Porta travada");
  // mqtt_client.publish(mqtt_outTopic, "Porta travada");
}

void Door::open() {
  _temp = millis();
  _mode = 2;
  digitalWrite(_door_pin, HIGH);
  digitalWrite(_led_pin, LOW);
  // Serial.println("Porta aberta");
  // mqtt_publish(mqtt_outTopic, "aberto");
}

void Door::add_card() {
  _temp= millis();
  _mode = 3;
}

void Door::update() {
  if (_mode == 1 && millis() - _temp > T_UNLOCK ||
      _mode == 2 && millis() - _temp > T_OPEN ||
      _mode == 3 && millis() - _temp > T_CARD) {
        lock();
  }

  if (digitalRead(_button_pin) == LOW) {
    open();
  }

  if (_mode == 3 && millis() - _led_temp > T_BLINK) {
    digitalWrite(_led_pin, !digitalRead(_led_pin));
    _led_pin = millis();
  }
}
