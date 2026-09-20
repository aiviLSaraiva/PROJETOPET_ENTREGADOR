#include <Wire.h>
#include <Servo.h>

// =====================================================
// I2C
// =====================================================

#define LCD 0x27
#define PCF8574 0x20

// =====================================================
// MOTORES - L293D
// =====================================================

#define MOTOR_E_IN1 7
#define MOTOR_E_IN2 8
#define MOTOR_E_EN  5

#define MOTOR_D_IN1 9
#define MOTOR_D_IN2 10
#define MOTOR_D_EN  6

#define VELOCIDADE 128

// =====================================================
// LDRs DE LINHA
// =====================================================

#define LDR_ESQ A0
#define LDR_CEN A1
#define LDR_DIR A2

#define LIMIAR_LDR 300

// =====================================================
// LDRs DAS ESTAÇÕES
// =====================================================

#define LDR_EST_ESQ A3
#define LDR_EST_DIR 4

// =====================================================
// HC-SR04
// =====================================================

#define TRIG_ESQ 11
#define ECHO_ESQ 12

#define TRIG_DIR 3
#define ECHO_DIR 13

#define DISTANCIA_OBSTACULO 30

// =====================================================
// SERVO DA TAMPA
// =====================================================

#define SERVO_TAMPA 1

#define POSICAO_FECHADA 0
#define POSICAO_ABERTA  90

#define TEMPO_TAMPA 5000

Servo servoTampa;

// =====================================================
// BOTÕES NO PCF8574
// =====================================================

#define D0 0b00001000
#define D1 0b00000001
#define D2 0b00000010
#define D3 0b00000100

// =====================================================
// LEDs NO PCF8574
// LEDs ativos em LOW
// =====================================================

#define LEDS_APAGADOS 0b11111111
#define LED_VERMELHO  0b11101111
#define LED_AMARELO   0b11011111
#define LED_VERDE     0b10111111

// =====================================================
// ESTADOS
// =====================================================

enum Estado {
  AGUARDANDO_DESTINO,
  MANOBRA_INICIAL,
  EM_ENTREGA,
  CHEGOU_DESTINO,
  AGUARDANDO_RETORNO,
  MANOBRA_U,
  RETORNANDO
};

Estado estadoAtual = AGUARDANDO_DESTINO;

// =====================================================
// VARIÁVEIS
// =====================================================

byte ultimoDestino = 0;

int ultimaDirecao = 0;

unsigned long inicioManobra = 0;
unsigned long inicioTampa = 0;

bool tampaAberta = false;

byte botoes;
byte botoesAnteriores = 0b00001111;

// =====================================================
// TEMPOS DAS MANOBRAS
// =====================================================

#define TEMPO_MANOBRA_ESQUERDA 2000
#define TEMPO_MANOBRA_DIREITA  2000
#define TEMPO_MANOBRA_RETO     3000
#define TEMPO_MANOBRA_U        2000

// =====================================================
// LCD
// =====================================================

void lcdExpande(byte dados) {
  Wire.beginTransmission(LCD);
  Wire.write(dados | 0x08);
  Wire.write(dados | 0x0C);
  Wire.write(dados | 0x08);
  Wire.endTransmission();
}

void lcdComando(byte comando) {
  byte alto = comando & 0xF0;
  byte baixo = (comando << 4) & 0xF0;

  lcdExpande(alto);
  lcdExpande(baixo);
}

void lcdCaractere(byte caractere) {
  byte alto = caractere & 0xF0;
  byte baixo = (caractere << 4) & 0xF0;

  Wire.beginTransmission(LCD);
  Wire.write(alto | 0x09);
  Wire.write(alto | 0x0D);
  Wire.write(alto | 0x09);
  Wire.endTransmission();

  Wire.beginTransmission(LCD);
  Wire.write(baixo | 0x09);
  Wire.write(baixo | 0x0D);
  Wire.write(baixo | 0x09);
  Wire.endTransmission();
}

void lcdTexto(const char *texto) {
  while (*texto) {
    lcdCaractere(*texto);
    texto++;
  }
}

void lcdLimpar() {
  lcdComando(0x01);
  delay(2);
}

void lcdInicializar() {
  delay(50);

  lcdComando(0x33);
  lcdComando(0x32);
  lcdComando(0x28);
  lcdComando(0x0C);
  lcdComando(0x06);
  lcdComando(0x01);

  delay(5);
}

void mostrarDestino(const char *destino) {
  lcdLimpar();

  lcdComando(0x80);
  lcdTexto("Destino:");

  lcdComando(0xC0);
  lcdTexto(destino);
}

void mostrarChegada(const char *estacao) {
  lcdLimpar();

  lcdComando(0x80);
  lcdTexto("Chegou:");

  lcdComando(0xC0);
  lcdTexto(estacao);
}

// =====================================================
// PCF8574
// =====================================================

void escreverPCF(byte estado) {
  Wire.beginTransmission(PCF8574);
  Wire.write(estado);
  Wire.endTransmission();
}

byte lerBotoes() {
  Wire.requestFrom(PCF8574, (byte)1);

  if (Wire.available()) {
    return Wire.read();
  }

  return 0xFF;
}

// =====================================================
// LED DO ÚLTIMO DESTINO
// =====================================================

void atualizarLED() {

  if (ultimoDestino == 1) {
    escreverPCF(LED_VERMELHO);
  }
  else if (ultimoDestino == 2) {
    escreverPCF(LED_AMARELO);
  }
  else if (ultimoDestino == 3) {
    escreverPCF(LED_VERDE);
  }
  else {
    escreverPCF(LEDS_APAGADOS);
  }
}

// =====================================================
// MOTORES
// =====================================================

void moveForward(int velocidade) {

  digitalWrite(MOTOR_E_IN1, HIGH);
  digitalWrite(MOTOR_E_IN2, LOW);

  digitalWrite(MOTOR_D_IN1, HIGH);
  digitalWrite(MOTOR_D_IN2, LOW);

  analogWrite(MOTOR_E_EN, velocidade);
  analogWrite(MOTOR_D_EN, velocidade);
}

void turnLeft(int velocidade) {

  digitalWrite(MOTOR_E_IN1, LOW);
  digitalWrite(MOTOR_E_IN2, HIGH);

  digitalWrite(MOTOR_D_IN1, HIGH);
  digitalWrite(MOTOR_D_IN2, LOW);

  analogWrite(MOTOR_E_EN, velocidade);
  analogWrite(MOTOR_D_EN, velocidade);
}

void turnRight(int velocidade) {

  digitalWrite(MOTOR_E_IN1, HIGH);
  digitalWrite(MOTOR_E_IN2, LOW);

  digitalWrite(MOTOR_D_IN1, LOW);
  digitalWrite(MOTOR_D_IN2, HIGH);

  analogWrite(MOTOR_E_EN, velocidade);
  analogWrite(MOTOR_D_EN, velocidade);
}

void stopMotors() {

  analogWrite(MOTOR_E_EN, 0);
  analogWrite(MOTOR_D_EN, 0);

  digitalWrite(MOTOR_E_IN1, LOW);
  digitalWrite(MOTOR_E_IN2, LOW);

  digitalWrite(MOTOR_D_IN1, LOW);
  digitalWrite(MOTOR_D_IN2, LOW);
}

// =====================================================
// SEGUIMENTO DE LINHA
// =====================================================

void followTrack() {

  int esquerda = analogRead(LDR_ESQ);
  int centro = analogRead(LDR_CEN);
  int direita = analogRead(LDR_DIR);

  bool pistaEsquerda = esquerda < LIMIAR_LDR;
  bool pistaCentro = centro < LIMIAR_LDR;
  bool pistaDireita = direita < LIMIAR_LDR;

  if (pistaEsquerda && !pistaCentro && !pistaDireita) {

    turnLeft(VELOCIDADE);
    ultimaDirecao = -1;
  }

  else if (pistaEsquerda && pistaCentro && !pistaDireita) {

    turnLeft(VELOCIDADE);
    ultimaDirecao = -1;
  }

  else if (!pistaEsquerda && pistaCentro && !pistaDireita) {

    moveForward(VELOCIDADE);
    ultimaDirecao = 0;
  }

  else if (!pistaEsquerda && pistaCentro && pistaDireita) {

    turnRight(VELOCIDADE);
    ultimaDirecao = 1;
  }

  else if (!pistaEsquerda && !pistaCentro && pistaDireita) {

    turnRight(VELOCIDADE);
    ultimaDirecao = 1;
  }

  else if (!pistaEsquerda && !pistaCentro && !pistaDireita) {

    if (ultimaDirecao == -1) {
      turnLeft(VELOCIDADE);
    }
    else if (ultimaDirecao == 1) {
      turnRight(VELOCIDADE);
    }
    else {
      stopMotors();
    }
  }

  else {

    moveForward(VELOCIDADE);
    ultimaDirecao = 0;
  }
}

// =====================================================
// DISTÂNCIA
// =====================================================

long medirDistancia(int trigPin, int echoPin) {

  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);

  digitalWrite(trigPin, LOW);

  long duracao = pulseIn(echoPin, HIGH, 30000);

  if (duracao == 0) {
    return -1;
  }

  return duracao / 58;
}

// =====================================================
// OBSTÁCULO
// =====================================================

bool obstaculoDetectado() {

  long distanciaEsquerda =
    medirDistancia(TRIG_ESQ, ECHO_ESQ);

  long distanciaDireita =
    medirDistancia(TRIG_DIR, ECHO_DIR);

  if (distanciaEsquerda == -1 ||
      distanciaDireita == -1) {

    return true;
  }

  if (distanciaEsquerda < DISTANCIA_OBSTACULO ||
      distanciaDireita < DISTANCIA_OBSTACULO) {

    return true;
  }

  return false;
}

// =====================================================
// ESTAÇÃO
// =====================================================

bool estacaoDetectada() {

  int esquerda = analogRead(LDR_EST_ESQ);
  int direita = digitalRead(LDR_EST_DIR);

  bool estacaoEsquerda =
    esquerda < LIMIAR_LDR;

  bool estacaoDireita =
    direita == LOW;

  if (estacaoEsquerda && estacaoDireita) {
    return true;
  }

  return false;
}

// =====================================================
// SETUP
// =====================================================

void setup() {

  // Motores
  pinMode(MOTOR_E_IN1, OUTPUT);
  pinMode(MOTOR_E_IN2, OUTPUT);
  pinMode(MOTOR_E_EN, OUTPUT);

  pinMode(MOTOR_D_IN1, OUTPUT);
  pinMode(MOTOR_D_IN2, OUTPUT);
  pinMode(MOTOR_D_EN, OUTPUT);

  // Ultrassônicos
  pinMode(TRIG_ESQ, OUTPUT);
  pinMode(ECHO_ESQ, INPUT);

  pinMode(TRIG_DIR, OUTPUT);
  pinMode(ECHO_DIR, INPUT);

  // Estação direita
  pinMode(LDR_EST_DIR, INPUT);

  // I2C
  Wire.begin();

  // LCD
  lcdInicializar();

  lcdComando(0x80);
  lcdTexto("ELEVADOR");

  lcdComando(0xC0);
  lcdTexto("HORIZONTAL");

  delay(1500);

  // Servo
  servoTampa.attach(SERVO_TAMPA);
  servoTampa.write(POSICAO_FECHADA);

  // Motores parados
  stopMotors();

  // LEDs apagados
  escreverPCF(LEDS_APAGADOS);
}

// =====================================================
// LOOP
// =====================================================

void loop() {

  // ===================================================
  // LEITURA DOS BOTÕES
  // ===================================================

  botoes = lerBotoes();

  // ---------------------------------------------------
  // D0 - retorno
  // ---------------------------------------------------

  if ((botoesAnteriores & D0) &&
      !(botoes & D0)) {

    delay(30);

    botoes = lerBotoes();

    if (!(botoes & D0) &&
        estadoAtual == AGUARDANDO_RETORNO) {

      lcdLimpar();

      lcdComando(0x80);
      lcdTexto("Retorno");

      lcdComando(0xC0);
      lcdTexto("Estacao 0");

      inicioManobra = millis();

      estadoAtual = MANOBRA_U;
    }
  }

  // ---------------------------------------------------
  // D1 - destino 1
  // ---------------------------------------------------

  if ((botoesAnteriores & D1) &&
      !(botoes & D1)) {

    delay(30);

    botoes = lerBotoes();

    if (!(botoes & D1) &&
        estadoAtual == AGUARDANDO_DESTINO) {

      ultimoDestino = 1;

      mostrarDestino("D1");

      inicioManobra = millis();

      estadoAtual = MANOBRA_INICIAL;
    }
  }

  // ---------------------------------------------------
  // D2 - destino 2
  // ---------------------------------------------------

  if ((botoesAnteriores & D2) &&
      !(botoes & D2)) {

    delay(30);

    botoes = lerBotoes();

    if (!(botoes & D2) &&
        estadoAtual == AGUARDANDO_DESTINO) {

      ultimoDestino = 2;

      mostrarDestino("D2");

      inicioManobra = millis();

      estadoAtual = MANOBRA_INICIAL;
    }
  }

  // ---------------------------------------------------
  // D3 - destino 3
  // ---------------------------------------------------

  if ((botoesAnteriores & D3) &&
      !(botoes & D3)) {

    delay(30);

    botoes = lerBotoes();

    if (!(botoes & D3) &&
        estadoAtual == AGUARDANDO_DESTINO) {

      ultimoDestino = 3;

      mostrarDestino("D3");

      inicioManobra = millis();

      estadoAtual = MANOBRA_INICIAL;
    }
  }

  // ===================================================
  // SEGURANÇA - OBSTÁCULO
  // ===================================================

  if (obstaculoDetectado()) {

    stopMotors();

    delay(20);

    atualizarLED();

    botoesAnteriores = botoes;

    return;
  }

  // ===================================================
  // MÁQUINA DE ESTADOS
  // ===================================================

  switch (estadoAtual) {

    // -------------------------------------------------
    // AGUARDANDO DESTINO
    // -------------------------------------------------

    case AGUARDANDO_DESTINO:

      stopMotors();

      break;


    // -------------------------------------------------
    // MANOBRA INICIAL
    // -------------------------------------------------

    case MANOBRA_INICIAL:

      if (ultimoDestino == 1) {

        turnLeft(VELOCIDADE);

        if (millis() - inicioManobra >=
            TEMPO_MANOBRA_ESQUERDA) {

          stopMotors();

          estadoAtual = EM_ENTREGA;
        }
      }

      else if (ultimoDestino == 2) {

        moveForward(VELOCIDADE);

        if (millis() - inicioManobra >=
            TEMPO_MANOBRA_RETO) {

          stopMotors();

          estadoAtual = EM_ENTREGA;
        }
      }

      else if (ultimoDestino == 3) {

        turnRight(VELOCIDADE);

        if (millis() - inicioManobra >=
            TEMPO_MANOBRA_DIREITA) {

          stopMotors();

          estadoAtual = EM_ENTREGA;
        }
      }

      break;


    // -------------------------------------------------
    // EM ENTREGA
    // -------------------------------------------------

    case EM_ENTREGA:

      if (estacaoDetectada()) {

        stopMotors();

        if (ultimoDestino == 1) {
          mostrarChegada("Estacao 1");
        }
        else if (ultimoDestino == 2) {
          mostrarChegada("Estacao 2");
        }
        else if (ultimoDestino == 3) {
          mostrarChegada("Estacao 3");
        }

        // Abre a tampa
        servoTampa.write(POSICAO_ABERTA);

        tampaAberta = true;

        inicioTampa = millis();

        estadoAtual = CHEGOU_DESTINO;
      }

      else {

        followTrack();
      }

      break;


    // -------------------------------------------------
    // CHEGOU AO DESTINO
    // -------------------------------------------------

    case CHEGOU_DESTINO:

      stopMotors();

      // Tampa permanece aberta durante 5 segundos

      if (tampaAberta &&
          millis() - inicioTampa >= TEMPO_TAMPA) {

        // Fecha a tampa
        servoTampa.write(POSICAO_FECHADA);

        tampaAberta = false;

        estadoAtual = AGUARDANDO_RETORNO;
      }

      break;


    // -------------------------------------------------
    // AGUARDANDO RETORNO
    // -------------------------------------------------

    case AGUARDANDO_RETORNO:

      stopMotors();

      break;


    // -------------------------------------------------
    // MANOBRA U
    // -------------------------------------------------

    case MANOBRA_U:

      turnLeft(VELOCIDADE);

      if (millis() - inicioManobra >=
          TEMPO_MANOBRA_U) {

        stopMotors();

        estadoAtual = RETORNANDO;
      }

      break;


    // -------------------------------------------------
    // RETORNANDO
    // -------------------------------------------------

    case RETORNANDO:

      if (estacaoDetectada()) {

        stopMotors();

        mostrarChegada("Estacao 0");

        estadoAtual = AGUARDANDO_DESTINO;
      }

      else {

        followTrack();
      }

      break;
  }

  // ===================================================
  // RESTAURA LED DO ÚLTIMO DESTINO
  // ===================================================

  atualizarLED();

  // ===================================================
  // GUARDA ESTADO DOS BOTÕES
  // ===================================================

  botoesAnteriores = botoes;

  delay(20);
}