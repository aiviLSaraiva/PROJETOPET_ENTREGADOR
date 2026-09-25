#include <Wire.h>

const byte linhas[4] = {2, 3, 4, 5};
const byte colunas[4] = {6, 7, 8, 9};

char teclas[4][4] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

volatile char teclaParaEnviar = 0;

void enviarTecla() {
  Wire.write(teclaParaEnviar);
  teclaParaEnviar = 0;
}

void setup() {
  Wire.begin(0x10);
  Wire.onRequest(enviarTecla);

  for (byte i = 0; i < 4; i++) {
    pinMode(linhas[i], OUTPUT);
    digitalWrite(linhas[i], HIGH);
  }

  for (byte i = 0; i < 4; i++) {
    pinMode(colunas[i], INPUT_PULLUP);
  }
}

void loop() {

  for (byte linha = 0; linha < 4; linha++) {

    for (byte i = 0; i < 4; i++) {
      digitalWrite(linhas[i], HIGH);
    }

    digitalWrite(linhas[linha], LOW);

    for (byte coluna = 0; coluna < 4; coluna++) {

      if (digitalRead(colunas[coluna]) == LOW) {

        teclaParaEnviar = teclas[linha][coluna];

        while (digitalRead(colunas[coluna]) == LOW) {
          delay(10);
        }

        delay(50);
      }
    }
  }
}