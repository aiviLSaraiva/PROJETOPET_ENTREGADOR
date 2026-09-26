#include <Wire.h> //Comunicação I2C, utilizado no LCD(imbutido) e na ponte H(motores)
#include <Servo.h> //Motor responsavel pela tranca do compartimento
#include <LiquidCrystal_I2C.h> //LCD com integração I2C
#include <Keypad.h>//Teclado 
// =====================================================
// VARIÁVEIS
// =====================================================

char ultimoDestino;//Local selecionado tendo 4 opções, 3 de ida B, C, D e 1 de volta A, que é a base para
//carregar a bateria do carrinho

int ultimaDirecao = 0;//responsavel por auxiliar em qual direção o carrinho deve ir baseado tambem na ultimaDirecao

unsigned long inicioManobra = 0; //tempo desde que começou a manobra

// =====================================================
// PCF8574
// =====================================================
#define PCF8574 0x20 //Endereço do extensor de pinos
byte estadoPCF = 0b00000000; //estado dos pinos do PCF todos em low, no caso só vai ser utilizado 4 pinos pros motores DC
//envia o byte estadoPCF inteiro pelo I2C, aplicando de uma vez o nível atual de todos os 8 pinos do PCF8574.
void escreverPCF() {
  Wire.beginTransmission(PCF8574);
  Wire.write(estadoPCF);
  Wire.endTransmission();
}
//liga ou desliga um bit específico dentro de estadoPCF e chama escreverPCF() para efetivar essa mudança no chip.
void definirPinoPCF(byte pino, bool nivel) {
  if (nivel) estadoPCF |= (1 << pino);
  else       estadoPCF &= ~(1 << pino);
  escreverPCF();
}
//lê o byte atual do PCF8574 via I2C e retorna se o bit daquele pino específico está em nível alto.
bool lerPinoPCF(byte pino) {
  Wire.requestFrom((uint8_t)PCF8574, (uint8_t)1);
  if (Wire.available()) {
    return Wire.read() & (1 << pino);
  }
  return false;
}

//Esses aqui são alguns bits responsaveis por cada pino do PCF8574.
//4 deles são relacionados ao motor
// nomes dos bits, pra ficar legível no resto do código
#define BIT_MOTOR_E_IN1 0 // 0 e o 1 são os pinos de direção sua combinação é responsavel por dizer 
#define BIT_MOTOR_E_IN2 1 //se o motor esquerdo  vai pra frente, ou pra tras.
#define BIT_MOTOR_D_IN1 2 //Mesma explicação para o motor direito
#define BIT_MOTOR_D_IN2 3

// =====================================================
// LCD
// =====================================================
LiquidCrystal_I2C lcd(0x27, 16, 2); //Endereço do PCF(Acoplado no LCD), 16 colunas e 2 linhas de espaço do LCD

// =====================================================
// Keypad
// =====================================================
//Usado para uma eficiente e fácil interação visual com o usuário

const byte LINHAS = 4; // O Keypad tem 4 linhas
const byte COLUNAS = 4; // O Keypad tem 4 colunas

//Mapeamento dos botões
char teclas[LINHAS][COLUNAS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte pinosLinhas[LINHAS] = {4, 5, 6, 7}; //Pinos do arduino para cada linha
byte pinosColunas[COLUNAS] = {8, 10, 11, 12}; // para cada coluna

//Função que gera o objeto keypad com todas as carcteristicas fornecidas
Keypad keypad = Keypad(makeKeymap(teclas), pinosLinhas, pinosColunas, LINHAS, COLUNAS);

// =====================================================
// LDRs DE LINHA
// =====================================================
//Sensores de luz responsáveis por manter o robo seguindo a linha
//Mapeamento dos Pinos do arduino.
#define LDR_ESQ A0
#define LDR_CEN A1
#define LDR_DIR A2

#define LIMIAR_LDR 300 //Para valores menores que o Limiar, o LDR receberá HIGH, em uma função, para maiores LOW.

// =====================================================
// LDRs DAS ESTAÇÕES
// =====================================================
//São os sensores responsáveis pela identificação de chegada da estação
//Mapeamento dos Pinos do arduino.
#define LDR_EST_ESQ A3
#define LDR_EST_DIR 0
// =====================================================
// PING 
// =====================================================
//São os sensores ultrassônicos responsaveis por identificar se há obstaculos no caminho
//Nesse Sensor Ultrassonico o ping substiui ao mesmo tempo o Trigger e o Echo, em um pino só
//Mapeamento dos Pinos do arduino
#define PING_ESQ 3
#define PING_DIR 2
#define DISTANCIA_LIMITE 30 // em um raio de 30 cm identifica um objeto
// Dispara o pulso de trigger e mede o tempo de eco do sensor PING)))
long medirDistancia(int pino) {
  // Pulso de Trigger (Saída)
  pinMode(pino, OUTPUT);
  digitalWrite(pino, LOW);
  delayMicroseconds(2);
  digitalWrite(pino, HIGH);
  delayMicroseconds(5); 
  digitalWrite(pino, LOW);

  // Leitura do Echo (Entrada)
  pinMode(pino, INPUT);
  long tempo = pulseIn(pino, HIGH);
  return tempo / 29 / 2; // Converte o tempo de viagem do som em centímetros
}
//Consegue detectar um objeto a 30 cm do sensor esquerdo ou direito
bool detectarObstaculo(){
  if(medirDistancia(PING_ESQ) <= DISTANCIA_LIMITE && medirDistancia(PING_ESQ) > 2){
    
    return true;
  }
  delay(30);
  if(medirDistancia(PING_DIR) <= DISTANCIA_LIMITE && medirDistancia(PING_DIR) > 2){
    return true;
  }
  return false;
}
//Verifica se o carro está impedido por um obstaculo(objeto)
bool presoObstaculo(){
  if(detectarObstaculo()){ //Se detectar pelo menos uma vez
    long tempoPerdido = millis();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Obstaculo a");
    lcd.setCursor(0,1);
    lcd.print("Frente");
    stopMotors();
    while(detectarObstaculo()){ //Enquanto continuar o objeto ele permanece preso
        delay(50);
    }
    lcd.clear();
    inicioManobra += millis() - tempoPerdido; //Desconta o tempo perdido ao ficar preso, 
    //para que a manobra seja sempre o mesmo tempo pre-definido independente de pausas
    return true;
  }
  return false; //se não detectou nenhum obstaculo
}
// =====================================================
// Senha
// =====================================================
String senhaGerada; //senha de segurança sempre gerada aleatoriamente para cada viagem
//Cria uma senha aleatoria
void gerarSenha(){
  randomSeed(micros()); // Gera a semente randomica para a futura senha
  senhaGerada = "";
  for(int i = 0; i < 4; i++){ //4 digitos
    senhaGerada += random(0, 10);
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Senha: " + senhaGerada);
  lcd.setCursor(0, 1);
  lcd.print("Clique #");
  confirmar();
}
//Todo o processo de pedir e processar a senha
void pedirSenha(){
  String senhaDigitada = "";
  while(true){
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Digite a senha:");
    while(true){
      char c = keypad.getKey();
      if(c != '\0' && c != 'A' && c != 'B' && c != 'C' && c != 'D'){
        if(c == '#'){ //Confirma
          lcd.clear();
          lcd.setCursor(0,0);
          if(senhaDigitada == senhaGerada){ //Se esta correta
            lcd.print("Senha Correta");
            lcd.setCursor(0,1);
            lcd.print("Acesso Liberado");
            return;
          }
          else{
            lcd.print("Senha Incorreta"); //Se esta incorreta
            lcd.setCursor(0,1);
            lcd.print("Acesso Negado");
            senhaDigitada = "";
            break;
          }
        }
        else if(c == '*'){ //limpar a sennha digitada
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("Senha Limpa");
          senhaDigitada = "";
          break;
        }
        else{              //adiciona os digitos 
          senhaDigitada += c;
          lcd.setCursor(0,1);
          lcd.print(senhaDigitada);
        }
      }
      c = '\0';
    }
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Tente Novamente");
    delay(300);
  }
}

// =====================================================
// COMPARTIMENTO(Servo Tampa e Objetos)
// =====================================================
//
#define SERVO_TAMPA 13 //pino arduino
//Angulo em graus correspondente há:
#define POSICAO_FECHADA 0
#define POSICAO_ABERTA  90
Servo servoTampa;
void abrirTampa(){
  int graus = POSICAO_FECHADA;
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Abrindo Tranca");
  for(graus; graus <= POSICAO_ABERTA; graus += 5){
    servoTampa.write(graus);
    delay(15);
  }
  lcd.clear();
  delay(50);
}

void fecharTampa(){
  int graus = POSICAO_ABERTA;
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Fechando Tranca");
  for(graus; graus >= POSICAO_FECHADA; graus -= 5){
    servoTampa.write(graus);
    delay(15);
  }
  lcd.clear();
  delay(50);
}
//Conferir que o usuario colocou o objeto no compartimento
void colocarObjeto(){
  abrirTampa();
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Coloque o objeto");
  lcd.setCursor(0,1);
  lcd.print("Confirme #");
  confirmar();
  gerarSenha();
  fecharTampa();
}
//Conferir que o usuario retirou o objeto no compartimento
void retirarObjeto(){
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Retire o objeto");
  lcd.setCursor(0,1);
  lcd.print("Confirme #");
  confirmar();
  fecharTampa();
}

// =====================================================
// TEMPOS DAS MANOBRAS
// =====================================================
//Isso é util quando o carrinho sai da base e volta para saber o tempo suficiente pra cada manobra 
#define TEMPO_MANOBRA_ESQUERDA 500// caminho à B
#define TEMPO_MANOBRA_DIREITA  500// à D
#define TEMPO_MANOBRA_RETO     750// à C
#define TEMPO_MANOBRA_U        500// à A (base)

// =====================================================
// MOTORES - L293D
// =====================================================
#define MOTOR_EN  9 // pino de Habilitação(Enable), responsavel por controlar a velocidade pelo sinal PWM
#define VELOCIDADE 128 //Velocidade constante que os motores vão rodar

//Se a optar pelo caminho C, a primeira manobra é seguir em frente
void moveForward(int velocidade) {
  definirPinoPCF(BIT_MOTOR_E_IN1, HIGH);
  definirPinoPCF(BIT_MOTOR_E_IN2, LOW);
  definirPinoPCF(BIT_MOTOR_D_IN1, HIGH);
  definirPinoPCF(BIT_MOTOR_D_IN2, LOW);
  analogWrite(MOTOR_EN, velocidade); // pino único, PWM nativo
  lcd.setCursor(14, 0); // Apenas estetico
  lcd.print("^");
  lcd.setCursor(13, 1);
  lcd.print(" | ");
}
//Se optar pelo caminho B, a primeira manobra é dobrar à esquerda
void turnLeft(int velocidade) {
  definirPinoPCF(BIT_MOTOR_E_IN1, LOW);
  definirPinoPCF(BIT_MOTOR_E_IN2, HIGH);
  definirPinoPCF(BIT_MOTOR_D_IN1, HIGH);
  definirPinoPCF(BIT_MOTOR_D_IN2, LOW);
  analogWrite(MOTOR_EN, velocidade);
  lcd.setCursor(14, 0);
  lcd.print(" ");
  lcd.setCursor(13, 1);
  lcd.print("<- ");
}
//Se optar pelo caminho D, a primeira manobra é dobrar à direita
void turnRight(int velocidade) {
  definirPinoPCF(BIT_MOTOR_E_IN1, HIGH);
  definirPinoPCF(BIT_MOTOR_E_IN2, LOW);
  definirPinoPCF(BIT_MOTOR_D_IN1, LOW);
  definirPinoPCF(BIT_MOTOR_D_IN2, HIGH);
  analogWrite(MOTOR_EN, velocidade);
  lcd.setCursor(14, 0);
  lcd.print(" ");
  lcd.setCursor(13, 1);
  lcd.print(" ->");
}
//Para os motores
void stopMotors() {

  analogWrite(MOTOR_EN, 0);
  definirPinoPCF(BIT_MOTOR_E_IN1, LOW);
  definirPinoPCF(BIT_MOTOR_E_IN2, LOW);
  definirPinoPCF(BIT_MOTOR_D_IN1, LOW);
  definirPinoPCF(BIT_MOTOR_D_IN2, LOW);
}

// =====================================================
// SEGUIMENTO DE LINHA
// =====================================================
//após fazer a manobra inicial, o robo segue a linha.
void followTrack() {
  //Leitura dos 3 LDR's
  int esquerda = analogRead(LDR_ESQ);
  int centro = analogRead(LDR_CEN);
  int direita = analogRead(LDR_DIR);

  bool pistaEsquerda = esquerda < LIMIAR_LDR; //Sensor Visualiza a linha(escura) na esquerda
  bool pistaCentro = centro < LIMIAR_LDR; //no centro
  bool pistaDireita = direita < LIMIAR_LDR;//na direita

  if (pistaEsquerda && !pistaCentro && !pistaDireita) { //Se só o sensor esquerdo visualiza a linha logo deve curvar à esquerda
 
    turnLeft(VELOCIDADE);
    
    ultimaDirecao = -1; //esquerda
  }

  else if (pistaEsquerda && pistaCentro && !pistaDireita) {

    turnLeft(VELOCIDADE);
    ultimaDirecao = -1;
  }

  else if (!pistaEsquerda && pistaCentro && !pistaDireita) {

    moveForward(VELOCIDADE);
    ultimaDirecao = 0; // centro
  }

  else if (!pistaEsquerda && pistaCentro && pistaDireita) {

    turnRight(VELOCIDADE);
    ultimaDirecao = 1; //direita
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
// ESTAÇÃO
// =====================================================
//Detecta quando chegar à estação baseado nos outros dois sensores frontais LDR's
bool estacaoDetectada() {

  int esquerda = analogRead(LDR_EST_ESQ);
  bool estacaoEsquerda = esquerda < LIMIAR_LDR;
  bool estacaoDireita = !digitalRead(LDR_EST_DIR); 

  return estacaoEsquerda && estacaoDireita;
}
//Pede para o usuario dizer onde o robo vai ir  na ida
void iniciar(){
  while(true){
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Clique B, C ou D");
    lcd.setCursor(0,1);
    char local = selecionarIda();
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Quer ir para ");
    lcd.print(local);
    lcd.print("?");
    lcd.setCursor(0,1);
    lcd.print("Sim(#) ou Nao(*)");
    if(confirmar()){
      ultimoDestino = local;
      break;
    }
    delay(50);
  }
}
// Função utilizada pela função iniciar()
char selecionarIda(){
  while(true){
    char c = keypad.getKey();
    if(c == 'B' || c == 'C' || c == 'D'){
      return c;
    }
  }
}

//Pede para o usuario dizer onde o robo volta por enquanto só a opção A
void retornar(){
  while(true){
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Retorne a Base");
    lcd.setCursor(0,1);
    lcd.print("Digite A");
    char local = selecionarBase();
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Quer ir para ");
    lcd.print(local);
    lcd.print("?");
    lcd.setCursor(0,1);
    lcd.print("Sim(#) ou Nao(*)");
    if(confirmar()){
      ultimoDestino = local;
      break;
    }
    delay(50);
  }
}

//Função utilizada pela função retornar()
char selecionarBase(){
  while(true){
    char c = keypad.getKey();
    if(c == 'A'){
      return c;
    }
  }
}

//Printa no lcd onde o carro chegou
void mostrarChegada(){
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Carrinho Chegou");
  lcd.setCursor(0,1);
  lcd.print("a estacao ");
  lcd.print(ultimoDestino);
  delay(300);
}

// =====================================================
// INTERAÇÃO COM USUÁRIO
// =====================================================
//Serve para o usuario confirmar (#) ou negar(*), muito utilizada em outras funções
bool confirmar(){
  while(true){
    char c = keypad.getKey();
    if(c == '#') return true;
    if(c == '*') return false;
  }
}

// =====================================================
// ESTADOS
// =====================================================
//São há maioria dos possíveis estados que o carrinho pode se encontrar no codigo eles constantemente mudam cronologicamente
enum Estado { 
  MANOBRA_INICIAL,
  EM_ENTREGA,
  CHEGOU_DESTINO,
  AGUARDANDO_RETORNO,
  MANOBRA_U,
  RETORNANDO
};
Estado estadoAtual;
//Função responsável pela alternância de estados
void maquinaEstados(){
  while(true){ //roda o fluxo inteiro ate chegar em Retornando e fechar a função
    switch(estadoAtual){
    case MANOBRA_INICIAL: //Baseado no local de ida escolhido direciona qual manobra fazer primeiro
      if (ultimoDestino == 'B') {
        I1:
        lcd.print("Manobrando a");
        lcd.setCursor(0,1);
        lcd.print("Esquerda...");
        turnLeft(VELOCIDADE);
        while(millis() - inicioManobra <= //Enquanto estiver na manobra verifica se não tem obstaculo
            TEMPO_MANOBRA_ESQUERDA){
          if(presoObstaculo()) goto I1;
        }
      }

      if (ultimoDestino == 'C') {
        I2:
        lcd.print("Seguindo reto");
        moveForward(VELOCIDADE);
        while(millis() - inicioManobra <=
            TEMPO_MANOBRA_RETO){
          if(presoObstaculo()) goto I2;
        }
      }
      if (ultimoDestino == 'D') {
        I3:
        lcd.print("Manobrando a");
        lcd.setCursor(0,1);
        lcd.print("Direita...");
        turnRight(VELOCIDADE);
        while(millis() - inicioManobra <=
            TEMPO_MANOBRA_DIREITA){
          if(presoObstaculo()) goto I3;
          
        }
      }
      lcd.clear();
      stopMotors(); // Ao terminar a manobra para temporariamente
      estadoAtual = EM_ENTREGA; //Passa para proxima parte
      break;
    
    case EM_ENTREGA: //Segue sempre a linha, verifica se não há obstaculos, e se alguma estação é detectada encerrando
      I4:
      lcd.print("Seguindo o");
      lcd.setCursor(0,1);
      lcd.print("Caminho");
      while(true){
        if(presoObstaculo()) goto I4;
        followTrack();
        if(estacaoDetectada()) break;
      }
      lcd.clear();
      stopMotors();
      mostrarChegada();
      pedirSenha();
      abrirTampa();
      estadoAtual = CHEGOU_DESTINO;
      break;
    case CHEGOU_DESTINO: //Ao chegar pede a retirada do objeto
      retirarObjeto();
      estadoAtual = AGUARDANDO_RETORNO;
      break;
    case AGUARDANDO_RETORNO: //Pede para selecionar o local de retorno
      retornar();
      estadoAtual = MANOBRA_U;
      break;
    case MANOBRA_U: //Processa a manobra em U(meia volta) necessaria para voltar a base A, verificando se não há obstaculo
      inicioManobra = millis();
      lcd.clear();
      I5:
      lcd.print("Manobrando");
      lcd.setCursor(0,1);
      lcd.print("Em U...");
      turnLeft(VELOCIDADE);
      while(millis() - inicioManobra <=
          TEMPO_MANOBRA_U){
        if(presoObstaculo) goto I5;
      }
      lcd.clear();
      stopMotors();
      estadoAtual = RETORNANDO;
      break;
    case RETORNANDO: //Segue a linha para retornar a base, verificando se há obstaculos, ou se ja achou a estação encerrando
      I6:
      lcd.print("Seguindo o");
      lcd.setCursor(0,1);
      lcd.print("Caminho");
      Serial.println("RETORNANDO");
      while(true){
        if(presoObstaculo()) goto I6;
        followTrack();
        if(estacaoDetectada()) break;
      }
      lcd.clear();
      stopMotors();
      mostrarChegada();
      return;
      break;
    }
  }
} 

void setup() {
  // Motores
  pinMode(MOTOR_EN, OUTPUT); //Pino do Motor Enable usado para os dois motores DC
  // I2C
  Wire.begin();
  // LCD
  lcd.init();
  lcd.backlight();
  //LRD
  pinMode(LDR_EST_DIR, INPUT);
  // Servo
  servoTampa.attach(SERVO_TAMPA);
  servoTampa.write(POSICAO_FECHADA);
  // Motores parados
  stopMotors();
}

void loop() {
  lcd.setCursor(0, 0);
  lcd.print("==Carrinho CT==");
  lcd.setCursor(0, 1);
  lcd.print("Clique #");
  confirmar();
  iniciar();
  colocarObjeto(); //Abre a tampa, gera senha, fecha tampa
  estadoAtual = MANOBRA_INICIAL; //para iniciar aa função maquinaEstados()
  inicioManobra = millis(); //marca o tempo em que começou a execução da manobra inicial
  maquinaEstados(); //Processa o restante do fluxo do carrinho até o final
  delay(500);
  lcd.clear(); // pronto para recomeçar tudo
}