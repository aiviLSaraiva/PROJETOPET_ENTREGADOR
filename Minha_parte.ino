#include <LiquidCrystal_I2C.h>
#include <Keypad.h>// Biblioteca utilizada para o keypad
#include <Wire.h>
#include <Servo.h>
const byte LINHAS = 4; // O Keypad tem 4 linhas
const byte COLUNAS = 4; // O Keypad tem 3 colunas, OBS: O unico keypad disponibilizado no Tinkercad
// tem 4 colunas, o que é desnecessario para o nosso projeto, portanto basta não conectar o fio
// à quarta coluna.
// Mapeamento dos botões

char hexaKeys[LINHAS][COLUNAS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

// Pinos das linhas
byte linPins[LINHAS] = {9, 8, 7, 6}; 
// Pinos das colunas (conecte APENAS os 3 primeiros pinos das colunas do componente)
byte colPins[COLUNAS] = {5, 4, 3, 2}; 

Keypad customKeypad = Keypad(makeKeymap(hexaKeys), linPins, colPins, LINHAS, COLUNAS); 

String senhaGerada = "1234";// Fazer depois um esquema para a cada viagem gerar uma senha aleatoria
String senhaDigitada = "";
String caminho = "";
char l1, l2;
LiquidCrystal_I2C lcd_1(0x27, 16, 2);

bool inicio = true;
bool parado = true;
bool aberto = true;
bool caminhoDefinido = false;
bool selecionarCaminho(char c){
  if(c == 'A' || c == 'B' || c == 'C' || c == 'D'){
    caminho += c;
    lcd_1.clear();
    lcd_1.setCursor(0, 0);
    lcd_1.print("Caminho(X,Y): " + caminho);
  }
  if(c == '*'){
    caminho = "";
    lcd_1.clear();
    lcd_1.print("Limpo!");
    delay(1000);
    lcd_1.clear();
    lcd_1.print("Caminho(X,Y): " + caminho);
    delay(2000);
  }
  if(c == '#'){
    lcd_1.clear();
    lcd_1.setCursor(0, 0);
    lcd_1.print("Caminho(X,Y): " + caminho);
    lcd_1.setCursor(0, 1);
    if(caminho.length() == 2){
      if(caminho.charAt(0) != caminho.charAt(1)){
        l1 = caminho.charAt(0);
        l2 = caminho.charAt(1);
        lcd_1.print("Caminho Valido");
        delay(3000);
        lcd_1.clear();
        lcd_1.setCursor(0, 0);
        lcd_1.print("Sai de " + l1);
        lcd_1.setCursor(0, 1);
        lcd_1.print("Chega em " + l2);
        delay(3000);
        return true;
      } else{
        lcd_1.print("Caminho Invalido");
        delay(3000);
      }
    }   
  }
  return false;
}
bool digitandoSenha(char c){
  lcd_1.clear();
  lcd_1.setCursor(0, 0);
  lcd_1.print("Digite a senha:");
  delay(2000);
  while(true){
    char c = customKeypad.getKey();
    if(c != NULL){
      if(c == '#'){
      lcd_1.clear();
      lcd_1.setCursor(0, 0);
      lcd_1.print("Senha: " + senhaDigitada);
      lcd_1.setCursor(0, 1);
        if(senhaDigitada == senhaGerada){
          lcd_1.print("Liberado");
          //comandos de abertura do compartimento
          destrancar();
          aberto = true;
          delay(2000);
          return true;
        }else{
            lcd_1.print("Negado");
            delay(2000);
        }
      senhaDigitada = "";
      lcd_1.clear();
      lcd_1.print("Digite a senha:");
      }else if(c == '*'){
        senhaDigitada = "";
        lcd_1.clear();
        lcd_1.print("Limpo!");
        delay(1000);
        lcd_1.clear();
        lcd_1.print("Digite a senha:");
      } else{
        senhaDigitada += c;
        lcd_1.clear();
        lcd_1.setCursor(0, 0);
        lcd_1.print("Senha: " + senhaDigitada);
      }
    }
    c = NULL;
  }
}

Servo tranca;
int pos = 0;
void trancar(){
  for (pos = 90; pos >= 0; pos -= 1) {
    // tell servo to go to position in variable 'pos'
    tranca.write(pos);
    Serial.println(pos);
    // wait 15 ms for servo to reach the position
    delay(15); // Wait for 15 millisecond(s)
  }
}
void destrancar(){
  for (pos = 0; pos <= 90; pos += 1) {// Abrir tranca
    // tell servo to go to position in variable 'pos'
    tranca.write(pos);
    Serial.println(pos);
    // wait 15 ms for servo to reach the position
    delay(15); // Wait for 15 millisecond(s)
  }
}

void gerarSenha(){
  senhaGerada = "";
  for(int i = 0; i < 4; i++){
    senhaGerada += random(0, 10);
  }
  lcd_1.clear();
  lcd_1.setCursor(0, 0);
  lcd_1.print("Senha: " + senhaGerada);
  lcd_1.setCursor(0, 1);
  lcd_1.print("Clique #");
  while(true){
    char c = customKeypad.getKey();
    if(c == '#'){
      break;
    }
  }
  aberto = false;
  delay(1000);
}
void mover(){
  inicio = false;
  parado = false;
  String progresso = "";
  lcd_1.clear();
  lcd_1.setCursor(0,0);
  lcd_1.print(String("Movendo: ") + l1 + "--> " + l2);
  lcd_1.setCursor(0,1);
  for(int i = 0; i < 16; i++){
    char c = customKeypad.getKey();
    if(c == '*'){
      parar();
      return;
    }
    lcd_1.setCursor(i, 1);
    lcd_1.print(".");
    delay(50);
  }
  chegar();
}
void parar(){
  parado = true;
  lcd_1.clear();
  lcd_1.setCursor(0,0);
  lcd_1.print("Interrompido");
  delay(2000);
}
void chegar(){
  parado = true;
  caminhoDefinido = false;
  lcd_1.clear();
  lcd_1.setCursor(0,0);
  lcd_1.print("Chegou!");
  delay(2000);
}
bool confirmar(char c){
  if(c == '#'){
    return true;
  }
  else{
    return false;
  }
}
bool continua(){
  lcd_1.clear();
  lcd_1.setCursor(0,0);
  lcd_1.print("Quer recomecar?");
  lcd_1.setCursor(0,1);
  lcd_1.print("Sim(#), Nao(*)");
  delay(250);
  while(true){
    char c = customKeypad.getKey();
    if(c == '*'){
      return false;
      //volta pra base
    }
    if(c == '#'){
      inicio = true;
      parado = true;
      aberto = false;
      caminhoDefinido = false;
      return true;
    }
  }
}
void setup() {
  Serial.begin(9600);
  tranca.attach(11);
  randomSeed(analogRead(A0));
  lcd_1.init();
  lcd_1.backlight();
  lcd_1.setCursor(0, 0);
  lcd_1.print("De: X,Para: Y");
  lcd_1.setCursor(0, 1);
  lcd_1.print("XY (A,B,C,D)");
}
//bool inicio = true;
//bool parado = true;
//bool aberto = false;
void loop() {
  char customKey = customKeypad.getKey();
  if(inicio && parado && aberto && !caminhoDefinido){
    if(customKey && selecionarCaminho(customKey)){
      caminhoDefinido = true;
      destrancar();
    }
  }
  if(inicio && parado && aberto && caminhoDefinido){
    if(customKey && confirmar(customKey)){ //ja fechou o compartimento com o objeto dentro
      aberto = false;
    }
  }
  if(inicio && parado && !aberto && caminhoDefinido){
    delay(2000);
    trancar();
    gerarSenha();
    mover();
  }
  if(!inicio && parado && !aberto && !caminhoDefinido){
    if(customKey && digitandoSenha(customKey)){
    }
  }
  if(!inicio && parado && aberto && !caminhoDefinido){
    if(continua()){
      lcd_1.clear();
      lcd_1.setCursor(0, 0);
      lcd_1.print("De: X,Para: Y");
      lcd_1.setCursor(0, 1);
      lcd_1.print("XY (A,B,C,D)");
      delay(1500);
    }else{
      //volta pra base
    }
  }
  
  delay(100);
}
