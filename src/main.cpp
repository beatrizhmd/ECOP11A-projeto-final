/**
 * =================================================================================================
 *
 * PROJETO FINAL: CONTROLE DE ENCHENTES
 * VERSÃO DE APRESENTAÇÃO - MODO DUPLO (REAL + SIMULAÇÃO)
 *
 * =================================================================================================
 *
 * Autor: Austin & amigos (AUTOMIND)
 * Curso: Engenharia de Controle e Automação - ECAE00
 * Data: 08/07/2025
 *
 * -- DESCRIÇÃO --
 * Sistema de controle para o nível de um rio em uma maquete de 17cm.
 * Opera em dois modos, selecionáveis pelo teclado:
 * 1. MODO REAL: Utiliza um sensor ultrassónico com filtro de média aparada para leituras estáveis.
 * 2. MODO SIMULAÇÃO: Permite a inserção manual da distância pelo teclado para uma demonstração controlada.
 * O sistema controla uma comporta (Servo) e um alarme (Relé) com base na distância (real ou simulada).
 * O nível de alerta de perigo é configurável pelo utilizador.
 *
 */

// --- Bibliotecas ---
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <ESP32Servo.h>
#include <algorithm> // Necessária para a função de ordenação do filtro

// --- Definições de Pinos e Constantes ---
LiquidCrystal_I2C lcd(0x27, 16, 2);
const int TRIGGER_PIN = 5;
const int ECHO_PIN = 18;
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {19, 23, 32, 33};
byte colPins[COLS] = {25, 26, 27, 14};
Keypad customKeypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);
Servo servoComporta;
const int SERVO_PIN = 13;
const int RELE_PIN = 12;

// --- Variáveis de Controle ---
const int ALTURA_MAX_CM = 17;
int distanciaAtual = ALTURA_MAX_CM;
int nivelAlerta = 8;
int nivelAtencao = 12;
String valorDigitado = "";
bool modoDeConfiguracao = false;
bool modoSimulacao = false;

/**
 * @brief Função de Setup: inicializa todos os componentes.
 */
void setup() {
  Serial.begin(115200);
  pinMode(TRIGGER_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  servoComporta.attach(SERVO_PIN);
  pinMode(RELE_PIN, OUTPUT);
  digitalWrite(RELE_PIN, LOW);
  lcd.init();
  lcd.backlight();
  lcd.print("Iniciando...");
  lcd.setCursor(0, 1);
  lcd.print("Sistema vFinal");
  delay(2000);
  atualizarSistema();
}

/**
 * @brief Loop principal: verifica o teclado e executa a operação normal.
 */
void loop() {
  char key = customKeypad.getKey();
  if (key) {
    processaTecla(key);
  }
  if (!modoDeConfiguracao) {
    operacaoNormal();
  }
}

/**
 * @brief Filtro de Média Aparada: faz 7 leituras, descarta a maior e a menor, e calcula a média das 5 restantes.
 * @return A distância filtrada e estável em cm.
 */
int getFilteredDistance() {
  const int numReadings = 7;
  int readings[numReadings];
  int readingCount = 0;

  while(readingCount < numReadings){
    digitalWrite(TRIGGER_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIGGER_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIGGER_PIN, LOW);
    long duration = pulseIn(ECHO_PIN, HIGH, 25000);
    if(duration > 0){
      readings[readingCount] = duration * 0.0343 / 2;
      readingCount++;
    }
  }

  std::sort(readings, readings + numReadings);

  long total = 0;
  for (int i = 1; i < numReadings - 1; i++) {
    total += readings[i];
  }

  int distance = total / (numReadings - 2);

  if (distance <= 0 || distance > ALTURA_MAX_CM) return ALTURA_MAX_CM;
  return distance;
}

/**
 * @brief Processa as teclas para os diferentes modos de operação.
 */
void processaTecla(char tecla) {
  // Tecla 'A': Ativa o modo de simulação
  if (tecla == 'A') {
    modoSimulacao = true;
    lcd.clear();
    lcd.print("MODO SIMULACAO");
    lcd.setCursor(0,1);
    lcd.print("Digite dist+ '#'");
    valorDigitado = "";
  }
  // Tecla 'B': Volta para o modo real
  else if (tecla == 'B') {
    modoSimulacao = false;
    modoDeConfiguracao = false; // Garante que sai de qualquer outro modo
    lcd.clear();
    lcd.print("MODO REAL");
    delay(1500);
    atualizarSistema();
  }
  // Tecla '*': Entra/sai do modo de configuração de alerta
  else if (tecla == '*') {
    modoDeConfiguracao = !modoDeConfiguracao;
    valorDigitado = "";
    if (modoDeConfiguracao) {
      lcd.clear();
      lcd.print("Novo Alerta(cm):");
      lcd.setCursor(0, 1);
      lcd.print("Digite valor+ '#' ");
    } else {
      atualizarSistema();
    }
  }
  // Tecla '#': Confirma a entrada de dados
  else if (tecla == '#') {
    if (valorDigitado.length() > 0) {
      if (modoDeConfiguracao) {
        nivelAlerta = valorDigitado.toInt();
        nivelAtencao = (ALTURA_MAX_CM + nivelAlerta) / 2;
        lcd.clear();
        lcd.print("Alerta Salvo!");
      } else if (modoSimulacao) {
        distanciaAtual = valorDigitado.toInt();
        lcd.clear();
        lcd.print("Dist. Simulada!");
      }
    }
    valorDigitado = "";
    modoDeConfiguracao = false;
    delay(1500);
    atualizarSistema();
  }
  // Teclas numéricas
  else if (isDigit(tecla)) {
    if (valorDigitado.length() < 3) {
      valorDigitado += tecla;
      lcd.setCursor(0, 1);
      lcd.print(valorDigitado);
    }
  }
}

/**
 * @brief Rotina de operação normal do sistema.
 */
void operacaoNormal() {
  // Se não estiver em modo de simulação, lê o sensor.
  if (!modoSimulacao) {
    distanciaAtual = getFilteredDistance();
  }
  atualizarSistema();
  delay(300);
}

/**
 * @brief Atualiza todos os atuadores e o display com base nos dados atuais.
 */
void atualizarSistema() {
  int angulo = map(distanciaAtual, nivelAtencao, nivelAlerta, 0, 90);
  angulo = constrain(angulo, 0, 90);
  servoComporta.write(angulo);

  bool alarmeAtivo = (distanciaAtual <= nivelAlerta);
  digitalWrite(RELE_PIN, alarmeAtivo ? HIGH : LOW);

  String statusNivel;
  if (distanciaAtual <= nivelAlerta) statusNivel = "PERIGO!";
  else if (distanciaAtual <= nivelAtencao) statusNivel = "ATENCAO";
  else statusNivel = "SEGURO";

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(modoSimulacao ? "SIMUL" : "REAL");
  lcd.setCursor(7, 0);
  lcd.print("D:");
  lcd.print(distanciaAtual);
  lcd.print("cm");
  
  lcd.setCursor(0, 1);
  lcd.print("Cmp:");
  lcd.print(angulo);
  lcd.print((char)223);
  lcd.print(" Alm:");
  lcd.print(alarmeAtivo ? "ON" : "OFF");
}
