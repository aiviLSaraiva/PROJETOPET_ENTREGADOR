#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

// ===== PINOS =====

// Motores
const byte MOTOR_ESQ_IN1 = 7;
const byte MOTOR_ESQ_IN2 = 8;
const byte MOTOR_ESQ_PWM = 5;

const byte MOTOR_DIR_IN1 = 9;
const byte MOTOR_DIR_IN2 = 10;
const byte MOTOR_DIR_PWM = 6;

// Seguidor de linha
const byte LDR_ESQ = A0;
const byte LDR_CEN = A1;
const byte LDR_DIR = A2;

// Estação
const byte LDR_EST_ESQ = A3;
const byte LDR_EST_DIR = 4;

// Ultrassônicos
const byte TRIG_ESQ = 11;
const byte ECHO_ESQ = 12;

const byte TRIG_DIR = 3;
const byte ECHO_DIR = 13;

// Servo 
const byte PINO_SERVO = 2;

// I2C
const byte LCD_ENDERECO = 0x27;
const byte PCF_LED_ENDERECO = 0x20;
const byte TECLADO_ENDERECO = 0x10;

LiquidCrystal_I2C lcd(LCD_ENDERECO, 16, 2);
Servo servoPorta;


// ===== CONFIGURAÇÕES =====

const byte VELOCIDADE = 128;

const int LIMIAR_LDR = 300;
const int LIMIAR_LDR_ESTACAO = 100;

const int DISTANCIA_OBSTACULO = 30;

// Servo (ângulos reais, em graus)
const int ANGULO_SERVO_FECHADO = 0;
const int ANGULO_SERVO_ABERTO = 90;
const unsigned long TEMPO_MOVIMENTO_SERVO = 500;

// Manobras
const unsigned long TEMPO_CURVA = 700;
const unsigned long TEMPO_U = 1400;


// ===== DIREÇÕES =====

enum Direcao {
  NORTE,
  LESTE,
  SUL,
  OESTE
};

enum Manobra {
  RETO,
  DIREITA,
  ESQUERDA,
  U
};


// ===== ESTADOS =====

enum Estado {
  ESCOLHENDO_DESTINO,
  MOSTRANDO_SENHA,
  MANOBRA_INICIAL,
  SAINDO_DA_ORIGEM,
  EM_VIAGEM,
  AGUARDANDO_SENHA,
  PORTA_ABERTA,
  SISTEMA_BLOQUEADO
};

Estado estadoAtual = ESCOLHENDO_DESTINO;


// ===== NAVEGAÇÃO =====

byte estacaoAtual = 0;
Direcao direcaoAtual = NORTE;
byte origemViagem = 0;
byte destino = 0;
bool destinoSelecionado = false;
Direcao direcaoDesejada = NORTE;
Manobra manobraAtual = RETO;


// ===== CONTROLE DA VIAGEM =====

unsigned long inicioManobra = 0;
bool saiuDaOrigem = false;

// Evita contar, no tempo da manobra, o tempo em que o robô ficou
// parado esperando um obstáculo sair da frente.
bool pausadoPorObstaculo = false;
unsigned long inicioPausa = 0;


// ===== SEGUIDOR =====

int ultimaDirecao = 0;


// ===== SENHA =====

char senhaViagem[5] = { '0', '0', '0', '0', '\0' };
char senhaDigitada[5] = { '\0', '\0', '\0', '\0', '\0' };
byte quantidadeSenha = 0;

// Controle de tentativas da senha da viagem
byte tentativasSenha = 0;
const byte MAX_TENTATIVAS_SENHA = 3;

// Código de manutenção para desbloqueio
char senhaManutencao[5] = { '1', '4', '0', '8', '\0' };
char codigoManutencao[5] = { '\0', '\0', '\0', '\0', '\0' };
byte quantidadeManutencao = 0;


// ===== CONTROLE DO TECLADO =====

// O teclado não é consultado durante a viagem.
unsigned long ultimaLeituraTeclado = 0;
const unsigned long INTERVALO_TECLADO = 50;


// ===== TELAS =====

void mostrarOrigemDestino() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ORIGEM: D");
  lcd.print(estacaoAtual);
  lcd.setCursor(0, 1);
  lcd.print("DESTINO? (0-3)");
}

void mostrarDestinoEscolhido() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("DESTINO: D");
  lcd.print(destino);
  lcd.setCursor(0, 1);
  lcd.print("#=OK  *=VOLTA");
}

void mostrarSenhaViagem() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SENHA:");
  lcd.print(senhaViagem);
  lcd.setCursor(0, 1);
  lcd.print("#=INICIA *=VOLTA");
}

void mostrarViagem() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("D");
  lcd.print(origemViagem);
  lcd.print(" -> D");
  lcd.print(destino);
  lcd.setCursor(0, 1);
  lcd.print("EM VIAGEM");
}

void mostrarSenhaDestino() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("DIGITE A SENHA");
  lcd.setCursor(0, 1);
  for (byte i = 0; i < quantidadeSenha; i++) {
    lcd.print('*');
  }
}

void mostrarSenhaCorreta() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ACESSO LIBERADO");
  lcd.setCursor(0, 1);
  lcd.print("PORTA ABERTA");
}

void mostrarSenhaErrada() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SENHA INCORRETA");
  lcd.setCursor(0, 1);
  lcd.print("TENTE NOVAMENTE");

  delay(1200);

  mostrarSenhaDestino();
}

void mostrarSistemaBloqueado() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SISTEMA BLOQ.");
  lcd.setCursor(0, 1);
  lcd.print("COD. MANUTENCAO");
}

void mostrarCodigoManutencao() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("COD. MANUTENCAO");
  lcd.setCursor(0, 1);

  for (byte i = 0; i < quantidadeManutencao; i++) {
    lcd.print('*');
  }
}


// ===== MOTORES =====

void motoresFrente(byte velocidade) {
  digitalWrite(MOTOR_ESQ_IN1, HIGH);
  digitalWrite(MOTOR_ESQ_IN2, LOW);

  digitalWrite(MOTOR_DIR_IN1, HIGH);
  digitalWrite(MOTOR_DIR_IN2, LOW);

  analogWrite(MOTOR_ESQ_PWM, velocidade);
  analogWrite(MOTOR_DIR_PWM, velocidade);
}

void virarDireita(byte velocidade) {
  digitalWrite(MOTOR_ESQ_IN1, HIGH);
  digitalWrite(MOTOR_ESQ_IN2, LOW);

  digitalWrite(MOTOR_DIR_IN1, LOW);
  digitalWrite(MOTOR_DIR_IN2, HIGH);

  analogWrite(MOTOR_ESQ_PWM, velocidade);
  analogWrite(MOTOR_DIR_PWM, velocidade);
}

void virarEsquerda(byte velocidade) {
  digitalWrite(MOTOR_ESQ_IN1, LOW);
  digitalWrite(MOTOR_ESQ_IN2, HIGH);

  digitalWrite(MOTOR_DIR_IN1, HIGH);
  digitalWrite(MOTOR_DIR_IN2, LOW);

  analogWrite(MOTOR_ESQ_PWM, velocidade);
  analogWrite(MOTOR_DIR_PWM, velocidade);
}

void manobraU(byte velocidade) {
  // Mesmo giro da curva à esquerda, só que por mais tempo (TEMPO_U).
  virarEsquerda(velocidade);
}

void pararMotores() {
  analogWrite(MOTOR_ESQ_PWM, 0);
  analogWrite(MOTOR_DIR_PWM, 0);
}


// ===== SEGUIDOR DE LINHA =====

void followTrack() {
  int esquerda = analogRead(LDR_ESQ);
  int centro = analogRead(LDR_CEN);
  int direita = analogRead(LDR_DIR);

  bool pistaEsquerda = esquerda < LIMIAR_LDR;
  bool pistaCentro = centro < LIMIAR_LDR;
  bool pistaDireita = direita < LIMIAR_LDR;

  if (pistaEsquerda && !pistaCentro && !pistaDireita) {
    virarEsquerda(VELOCIDADE);
    ultimaDirecao = -1;
  }
  else if (pistaEsquerda && pistaCentro && !pistaDireita) {
    virarEsquerda(VELOCIDADE);
    ultimaDirecao = -1;
  }
  else if (!pistaEsquerda && pistaCentro && !pistaDireita) {
    motoresFrente(VELOCIDADE);
    ultimaDirecao = 0;
  }
  else if (!pistaEsquerda && pistaCentro && pistaDireita) {
    virarDireita(VELOCIDADE);
    ultimaDirecao = 1;
  }
  else if (!pistaEsquerda && !pistaCentro && pistaDireita) {
    virarDireita(VELOCIDADE);
    ultimaDirecao = 1;
  }
  else if (!pistaEsquerda && !pistaCentro && !pistaDireita) {
    if (ultimaDirecao == -1) {
      virarEsquerda(VELOCIDADE);
    }
    else if (ultimaDirecao == 1) {
      virarDireita(VELOCIDADE);
    }
    else {
      pararMotores();
    }
  }
  else {
    motoresFrente(VELOCIDADE);
    ultimaDirecao = 0;
  }
}


// ===== DETECÇÃO DE ESTAÇÃO =====

bool estacaoDetectada() {
  int esquerda = analogRead(LDR_EST_ESQ);
  int direita = digitalRead(LDR_EST_DIR);

  bool estacaoEsquerda = esquerda < LIMIAR_LDR_ESTACAO;
  bool estacaoDireita = direita == LOW;

  return estacaoEsquerda && estacaoDireita;
}


// ===== ULTRASSÔNICOS =====

long medirDistancia(byte trig, byte echo) {
  digitalWrite(trig, LOW);
  delayMicroseconds(2);

  digitalWrite(trig, HIGH);
  delayMicroseconds(10);

  digitalWrite(trig, LOW);

  unsigned long duracao = pulseIn(echo, HIGH, 30000);

  if (duracao == 0) {
    // Sem eco dentro do tempo limite = nada refletiu no alcance do
    // sensor, ou seja, caminho livre.
    return DISTANCIA_OBSTACULO + 1;
  }

  return duracao / 58;
}

bool obstaculoDetectado() {
  long esquerda = medirDistancia(TRIG_ESQ, ECHO_ESQ);
  long direita = medirDistancia(TRIG_DIR, ECHO_DIR);

  if (esquerda < DISTANCIA_OBSTACULO) {
    return true;
  }

  if (direita < DISTANCIA_OBSTACULO) {
    return true;
  }

  return false;
}


// ===== SERVO =====

void servoAbrir() {
  servoPorta.attach(PINO_SERVO);
  servoPorta.write(ANGULO_SERVO_ABERTO);
  delay(TEMPO_MOVIMENTO_SERVO);
  servoPorta.detach();
}

void servoFechar() {
  servoPorta.attach(PINO_SERVO);
  servoPorta.write(ANGULO_SERVO_FECHADO);
  delay(TEMPO_MOVIMENTO_SERVO);
  servoPorta.detach();
}


// ===== LEDS (PCF8574) =====
// LOW = ligado, HIGH = desligado

void pcf8574Enviar(byte valor) {
  Wire.beginTransmission(PCF_LED_ENDERECO);
  Wire.write(valor);
  Wire.endTransmission();
}

void desligarTodosLeds() {
  pcf8574Enviar(0xFF);
}

void ledDestino(byte destinoEscolhido) {
  byte valor = 0xFF;

  if (destinoEscolhido == 1) {
    valor &= ~(1 << 4); // vermelho
  }
  else if (destinoEscolhido == 2) {
    valor &= ~(1 << 5); // amarelo
  }
  else if (destinoEscolhido == 3) {
    valor &= ~(1 << 6); // verde
  }

  pcf8574Enviar(valor);
}


// ===== NAVEGAÇÃO =====

byte obterDirecaoDestino(byte origem, byte destinoEscolhido) {
  if (origem == 0) {
    if (destinoEscolhido == 1) return LESTE;
    if (destinoEscolhido == 2) return NORTE;
    if (destinoEscolhido == 3) return OESTE;
  }

  if (origem == 1) {
    if (destinoEscolhido == 0) return SUL;
    if (destinoEscolhido == 2) return NORTE;
    if (destinoEscolhido == 3) return LESTE;
  }

  if (origem == 2) {
    if (destinoEscolhido == 0) return SUL;
    if (destinoEscolhido == 1) return LESTE;
    if (destinoEscolhido == 3) return OESTE;
  }

  if (origem == 3) {
    if (destinoEscolhido == 0) return LESTE;
    if (destinoEscolhido == 1) return OESTE;
    if (destinoEscolhido == 2) return NORTE;
  }

  return direcaoAtual;
}

byte obterOrientacaoChegada(byte origem, byte destinoEscolhido) {
  if (origem == 0 && destinoEscolhido == 1) return NORTE;
  if (origem == 0 && destinoEscolhido == 2) return NORTE;
  if (origem == 0 && destinoEscolhido == 3) return OESTE;

  if (origem == 1 && destinoEscolhido == 0) return OESTE;
  if (origem == 1 && destinoEscolhido == 2) return OESTE;
  if (origem == 1 && destinoEscolhido == 3) return LESTE;

  if (origem == 2 && destinoEscolhido == 0) return SUL;
  if (origem == 2 && destinoEscolhido == 1) return SUL;
  if (origem == 2 && destinoEscolhido == 3) return SUL;

  if (origem == 3 && destinoEscolhido == 0) return LESTE;
  if (origem == 3 && destinoEscolhido == 1) return OESTE;
  if (origem == 3 && destinoEscolhido == 2) return LESTE;

  return direcaoAtual;
}

byte calcularManobra(byte atual, byte desejada) {
  if (atual == desejada) {
    return RETO;
  }

  if (
    (atual == NORTE && desejada == LESTE) ||
    (atual == LESTE && desejada == SUL) ||
    (atual == SUL && desejada == OESTE) ||
    (atual == OESTE && desejada == NORTE)
  ) {
    return DIREITA;
  }

  if (
    (atual == NORTE && desejada == OESTE) ||
    (atual == OESTE && desejada == SUL) ||
    (atual == SUL && desejada == LESTE) ||
    (atual == LESTE && desejada == NORTE)
  ) {
    return ESQUERDA;
  }

  return U;
}


// ===== TECLADO =====
// Só é consultado quando o robô está parado.

char lerTecla() {
  if (millis() - ultimaLeituraTeclado < INTERVALO_TECLADO) {
    return 0;
  }

  ultimaLeituraTeclado = millis();

  Wire.requestFrom(TECLADO_ENDERECO, (byte)1);

  if (Wire.available()) {
    char tecla = Wire.read();

    if (tecla != 0) {
      return tecla;
    }
  }

  return 0;
}


// ===== SENHA =====

void gerarSenha() {
  for (byte i = 0; i < 4; i++) {
    senhaViagem[i] = '0' + random(0, 10);
  }

  senhaViagem[4] = '\0';
}

void limparSenhaDigitada() {
  quantidadeSenha = 0;

  for (byte i = 0; i < 5; i++) {
    senhaDigitada[i] = '\0';
  }
}

bool senhaEstaCorreta() {
  if (quantidadeSenha != 4) {
    return false;
  }

  for (byte i = 0; i < 4; i++) {
    if (senhaDigitada[i] != senhaViagem[i]) {
      return false;
    }
  }

  return true;
}


// ===== CÓDIGO DE MANUTENÇÃO =====

void limparCodigoManutencao() {
  quantidadeManutencao = 0;

  for (byte i = 0; i < 5; i++) {
    codigoManutencao[i] = '\0';
  }
}

bool codigoManutencaoCorreto() {
  if (quantidadeManutencao != 4) {
    return false;
  }

  for (byte i = 0; i < 4; i++) {
    if (codigoManutencao[i] != senhaManutencao[i]) {
      return false;
    }
  }

  return true;
}


// ===== PREPARAR VIAGEM =====

void prepararViagem() {
  origemViagem = estacaoAtual;

  direcaoDesejada = (Direcao)obterDirecaoDestino(origemViagem, destino);
  manobraAtual = (Manobra)calcularManobra(direcaoAtual, direcaoDesejada);

  saiuDaOrigem = false;
  pausadoPorObstaculo = false;

  ledDestino(destino);
  mostrarViagem();

  inicioManobra = millis();

  if (manobraAtual == RETO) {
    estadoAtual = SAINDO_DA_ORIGEM;
  }
  else {
    estadoAtual = MANOBRA_INICIAL;
  }
}


// ===== MANOBRA INICIAL =====

void executarManobraInicial() {
  if (obstaculoDetectado()) {
    pararMotores();

    if (!pausadoPorObstaculo) {
      pausadoPorObstaculo = true;
      inicioPausa = millis();
    }

    return;
  }

  // Desconta do cronômetro da manobra o tempo em que ficou parado.
  if (pausadoPorObstaculo) {
    inicioManobra += (millis() - inicioPausa);
    pausadoPorObstaculo = false;
  }

  unsigned long tempo = millis() - inicioManobra;

  if (manobraAtual == DIREITA) {
    virarDireita(VELOCIDADE);

    if (tempo >= TEMPO_CURVA) {
      pararMotores();
      estadoAtual = SAINDO_DA_ORIGEM;
    }
  }
  else if (manobraAtual == ESQUERDA) {
    virarEsquerda(VELOCIDADE);

    if (tempo >= TEMPO_CURVA) {
      pararMotores();
      estadoAtual = SAINDO_DA_ORIGEM;
    }
  }
  else if (manobraAtual == U) {
    manobraU(VELOCIDADE);

    if (tempo >= TEMPO_U) {
      pararMotores();
      estadoAtual = SAINDO_DA_ORIGEM;
    }
  }
  else {
    estadoAtual = SAINDO_DA_ORIGEM;
  }
}


// ===== SAIR DA ORIGEM =====

void sairDaOrigem() {
  if (obstaculoDetectado()) {
    pararMotores();
    return;
  }

  followTrack();

  // Enquanto estiver sobre a marca da origem, não pode considerar
  // que já chegou ao destino.
  if (!estacaoDetectada()) {
    saiuDaOrigem = true;
    estadoAtual = EM_VIAGEM;
    mostrarViagem();
  }
}


// ===== VIAGEM =====

void executarViagem() {
  if (obstaculoDetectado()) {
    pararMotores();
    return;
  }

  followTrack();

  // Só procura estação depois de ter saído da origem.
  if (saiuDaOrigem && estacaoDetectada()) {
    pararMotores();

    estacaoAtual = destino;
    direcaoAtual = (Direcao)obterOrientacaoChegada(origemViagem, destino);

    desligarTodosLeds();
    limparSenhaDigitada();

    // Começa uma nova contagem de tentativas ao chegar ao destino.
    tentativasSenha = 0;

    estadoAtual = AGUARDANDO_SENHA;
    mostrarSenhaDestino();
  }
}


// ===== ESCOLHA DO DESTINO =====

void processarEscolhaDestino(char tecla) {
  if (tecla == 0) {
    return;
  }

  if (tecla == '*') {
    destinoSelecionado = false;
    mostrarOrigemDestino();
    return;
  }

  if (tecla >= '0' && tecla <= '3') {
    byte novoDestino = tecla - '0';

    if (novoDestino == estacaoAtual) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("DESTINO INVALIDO");
      lcd.setCursor(0, 1);
      lcd.print("ESCOLHA OUTRO");

      delay(1000);

      mostrarOrigemDestino();
      return;
    }

    destino = novoDestino;
    destinoSelecionado = true;
    mostrarDestinoEscolhido();
    return;
  }

  if (tecla == '#' && destinoSelecionado) {
    gerarSenha();
    mostrarSenhaViagem();

    // A viagem só começa quando o usuário confirmar de novo com #.
    estadoAtual = MOSTRANDO_SENHA;
  }
}


// ===== APÓS MOSTRAR A SENHA (espera # para começar) =====

void processarSenhaMostrada(char tecla) {
  if (tecla == 0) {
    return;
  }

  if (tecla == '#') {
    prepararViagem();
    return;
  }

  if (tecla == '*') {
    destinoSelecionado = false;
    mostrarOrigemDestino();
    estadoAtual = ESCOLHENDO_DESTINO;
    return;
  }
}


// ===== SENHA NA ESTAÇÃO DE DESTINO =====

void processarSenhaDestino(char tecla) {
  if (tecla == 0) {
    return;
  }

  if (tecla == '*') {
    limparSenhaDigitada();
    mostrarSenhaDestino();
    return;
  }

  if (tecla >= '0' && tecla <= '9') {
    if (quantidadeSenha < 4) {
      senhaDigitada[quantidadeSenha] = tecla;
      quantidadeSenha++;
      mostrarSenhaDestino();
    }
    return;
  }

  if (tecla == '#') {
    if (senhaEstaCorreta()) {
      mostrarSenhaCorreta();
      servoAbrir();

      // Senha correta: zera as tentativas.
      tentativasSenha = 0;

      estadoAtual = PORTA_ABERTA;
    }
    else {
      tentativasSenha++;

      if (tentativasSenha >= MAX_TENTATIVAS_SENHA) {
        limparSenhaDigitada();
        limparCodigoManutencao();

        estadoAtual = SISTEMA_BLOQUEADO;

        mostrarSistemaBloqueado();

        delay(1500);

        mostrarCodigoManutencao();
      }
      else {
        mostrarSenhaErrada();
        limparSenhaDigitada();
      }
    }
  }
}


// ===== SISTEMA BLOQUEADO =====

void processarSistemaBloqueado(char tecla) {
  if (tecla == 0) {
    return;
  }

  if (tecla == '*') {
    limparCodigoManutencao();
    mostrarCodigoManutencao();
    return;
  }

  if (tecla >= '0' && tecla <= '9') {
    if (quantidadeManutencao < 4) {
      codigoManutencao[quantidadeManutencao] = tecla;
      quantidadeManutencao++;
      mostrarCodigoManutencao();
    }

    return;
  }

  if (tecla == '#') {
    if (codigoManutencaoCorreto()) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("MANUTENCAO OK");
      lcd.setCursor(0, 1);
      lcd.print("DESBLOQUEADO");

      delay(1500);

      limparCodigoManutencao();
      limparSenhaDigitada();

      tentativasSenha = 0;

      estadoAtual = AGUARDANDO_SENHA;
      mostrarSenhaDestino();
    }
    else {
      limparCodigoManutencao();

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("CODIGO ERRADO");
      lcd.setCursor(0, 1);
      lcd.print("TENTE NOVAMENTE");

      delay(1200);

      mostrarCodigoManutencao();
    }
  }
}


// ===== PORTA ABERTA =====

void processarPortaAberta(char tecla) {
  if (tecla == 0) {
    return;
  }

  if (tecla == '#' || tecla == '*') {
    servoFechar();

    limparSenhaDigitada();
    destinoSelecionado = false;
    mostrarOrigemDestino();

    estadoAtual = ESCOLHENDO_DESTINO;
  }
}


// ===== SETUP =====

void setup() {
  pinMode(MOTOR_ESQ_IN1, OUTPUT);
  pinMode(MOTOR_ESQ_IN2, OUTPUT);
  pinMode(MOTOR_ESQ_PWM, OUTPUT);

  pinMode(MOTOR_DIR_IN1, OUTPUT);
  pinMode(MOTOR_DIR_IN2, OUTPUT);
  pinMode(MOTOR_DIR_PWM, OUTPUT);

  pinMode(TRIG_ESQ, OUTPUT);
  pinMode(ECHO_ESQ, INPUT);

  pinMode(TRIG_DIR, OUTPUT);
  pinMode(ECHO_DIR, INPUT);

  pinMode(LDR_EST_DIR, INPUT);

  Wire.begin();

  lcd.init();
  lcd.backlight();

  desligarTodosLeds();
  pararMotores();

  estacaoAtual = 0;
  direcaoAtual = NORTE;
  destinoSelecionado = false;
  estadoAtual = ESCOLHENDO_DESTINO;

  randomSeed(micros());

  mostrarOrigemDestino();
}


// ===== LOOP =====

void loop() {
  char tecla = 0;

  // O teclado só é acessado nos estados em que ele é realmente
  // necessário; durante manobra e viagem não há requisição I2C.
  if (
    estadoAtual == ESCOLHENDO_DESTINO ||
    estadoAtual == MOSTRANDO_SENHA ||
    estadoAtual == AGUARDANDO_SENHA ||
    estadoAtual == PORTA_ABERTA ||
    estadoAtual == SISTEMA_BLOQUEADO
  ) {
    tecla = lerTecla();
  }

  switch (estadoAtual) {
    case ESCOLHENDO_DESTINO:
      processarEscolhaDestino(tecla);
      break;

    case MOSTRANDO_SENHA:
      processarSenhaMostrada(tecla);
      break;

    case MANOBRA_INICIAL:
      executarManobraInicial();
      break;

    case SAINDO_DA_ORIGEM:
      sairDaOrigem();
      break;

    case EM_VIAGEM:
      executarViagem();
      break;

    case AGUARDANDO_SENHA:
      processarSenhaDestino(tecla);
      break;

    case PORTA_ABERTA:
      processarPortaAberta(tecla);
      break;

    case SISTEMA_BLOQUEADO:
      processarSistemaBloqueado(tecla);
      break;
  }
}
