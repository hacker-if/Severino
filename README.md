# Severino
Controlador da fechadura do HackerSpace.IFUSP usando um NodeMCU

## NodeMCU
NodeMCU é um controlador basedo na ESP8266 usando o framework [ESP8266 Arduino](https://arduino-esp8266.readthedocs.io/en/2.5.0-beta1/index.html),que permite programar a placa usando a linguagem e as bibliotecas do Arquido.

## PlatformIO
O projeto foi escrito usando o [PlatformIO](https://platformio.org/install). É uma extenção do vscode ou Atom para progamar diversos controladores.
Visite o site pra saber mais.

É possível rodar o programa pelo Arduino IDE. Mude o nome do arquino ``main.cpp`` para ``main.ino`` e retire a primeira linha, que importa a biblioteca ``Arduino.h`` e pronto.

## Bibliotecas utilizadas
Algorítimo de BLAKE2s para autenticação: [Arduino Cryptography Library](https://rweather.github.io/arduinolibs/crypto.html)

Conversão para Base64: [base64_arduino](https://github.com/adamvr/arduino-base64)
