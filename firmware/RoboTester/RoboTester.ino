/*
  RoboTester v2 - firmware de pruebas para los robots del curso
  -------------------------------------------------------------
  1. Sube este programa UNA vez al Arduino (UNO / Nano).
  2. Cierra el Monitor Serie del IDE.
  3. Abre la página de RoboTester, elige tu proyecto y pulsa Conectar.
  4. Al terminar, vuelve a subir el programa de tu robot.

  Baudios: 9600 · fin de línea: Nueva línea

  COMANDOS (pines: 13, 9, A0...)
    PING                         -> PONG
    MODE p OUT|IN|PULLUP
    DW p 0|1                     escritura digital
    PWM p 0..255
    DR p                         -> D p valor
    AR p                         -> A p valor
    US trig echo                 -> U trig cm   (-1 = sin eco)
    SERVO p 0..180   /  SDETACH p
    MOTOR in1 in2 en vel [ms]    vel -255..255, en = - si no hay pin EN,
                                 ms = se detiene solo después de ese tiempo
    TONE p freq [ms] /  NOTONE p
    BT rx tx [baud]              abre Bluetooth (SoftwareSerial)
    BTS texto                    envía texto por Bluetooth
                                 lo recibido llega como: B texto
    WATCH A p | WATCH D p | WATCH U trig echo
    UNWATCH  /  RATE ms  /  STOP
*/

#include <Servo.h>
#include <SoftwareSerial.h>

const long BAUD = 9600;
const uint8_t MAX_SERVOS = 4;
const uint8_t MAX_WATCH = 8;
const uint8_t MAX_MOTORS = 2;

Servo servos[MAX_SERVOS];
int servoPins[MAX_SERVOS];

struct Watch { char type; int pin; int pin2; char label[5]; };
Watch watches[MAX_WATCH];
uint8_t nWatch = 0;

struct Motor { int in1, in2, en; unsigned long until; };
Motor motors[MAX_MOTORS];
uint8_t nMotors = 0;

SoftwareSerial* bt = NULL;
char btBuf[40];
uint8_t btLen = 0;
unsigned long btLast = 0;

unsigned long rateMs = 150, lastSend = 0;
char line[64];
uint8_t len = 0;

// ---------------- utilidades ----------------
int parsePin(const char* t) {
  if (!t || !*t) return -1;
  if (t[0] == 'A' || t[0] == 'a') return A0 + atoi(t + 1);
  if (t[0] < '0' || t[0] > '9') return -1;
  return atoi(t);
}
void ok(const char* c) { Serial.print(F("OK ")); Serial.println(c); }
void err(const char* m) { Serial.print(F("ERR ")); Serial.println(m); }

float readUltrasonic(int trig, int echo) {
  pinMode(trig, OUTPUT); pinMode(echo, INPUT);
  digitalWrite(trig, LOW); delayMicroseconds(2);
  digitalWrite(trig, HIGH); delayMicroseconds(10);
  digitalWrite(trig, LOW);
  unsigned long d = pulseIn(echo, HIGH, 25000UL);
  if (d == 0) return -1;
  return d / 58.0;
}

int servoSlot(int pin, bool create) {
  for (uint8_t i = 0; i < MAX_SERVOS; i++) if (servoPins[i] == pin) return i;
  if (!create) return -1;
  for (uint8_t i = 0; i < MAX_SERVOS; i++) if (servoPins[i] == -1) {
    servos[i].attach(pin); servoPins[i] = pin; return i;
  }
  return -1;
}

void motorOff(Motor& m) {
  digitalWrite(m.in1, LOW); digitalWrite(m.in2, LOW);
  if (m.en >= 0) analogWrite(m.en, 0);
  m.until = 0;
}

void setMotor(int in1, int in2, int en, int vel, unsigned long ms) {
  vel = constrain(vel, -255, 255);
  pinMode(in1, OUTPUT); pinMode(in2, OUTPUT);
  int mag = abs(vel);
  if (en >= 0) {
    pinMode(en, OUTPUT);
    digitalWrite(in1, vel > 0 ? HIGH : LOW);
    digitalWrite(in2, vel < 0 ? HIGH : LOW);
    analogWrite(en, mag);
  } else {
    if (vel >= 0) { analogWrite(in1, mag); digitalWrite(in2, LOW); }
    else          { digitalWrite(in1, LOW); analogWrite(in2, mag); }
  }
  int idx = -1;
  for (uint8_t i = 0; i < nMotors; i++) if (motors[i].in1 == in1 && motors[i].in2 == in2) idx = i;
  if (idx < 0 && nMotors < MAX_MOTORS) { idx = nMotors++; }
  if (idx < 0) return;
  motors[idx].in1 = in1; motors[idx].in2 = in2; motors[idx].en = en;
  motors[idx].until = (ms && vel) ? millis() + ms : 0;
}

void stopAll() {
  nWatch = 0;
  for (uint8_t i = 0; i < nMotors; i++) motorOff(motors[i]);
  for (uint8_t i = 0; i < MAX_SERVOS; i++)
    if (servoPins[i] != -1) { servos[i].detach(); servoPins[i] = -1; }
}

void sendWatch(Watch& w) {
  Serial.print(w.type); Serial.print(' '); Serial.print(w.label); Serial.print(' ');
  if (w.type == 'A') Serial.println(analogRead(w.pin));
  else if (w.type == 'D') Serial.println(digitalRead(w.pin));
  else Serial.println(readUltrasonic(w.pin, w.pin2), 1);
}

void btFlush() {
  if (!btLen) return;
  btBuf[btLen] = 0;
  Serial.print(F("B ")); Serial.println(btBuf);
  btLen = 0;
}

// ---------------- intérprete ----------------
void handle(char* s) {
  // BTS necesita el resto de la línea completo
  if ((s[0] == 'B' || s[0] == 'b') && (s[1] == 'T' || s[1] == 't') && (s[2] == 'S' || s[2] == 's') && s[3] == ' ') {
    if (!bt) return err("BT no iniciado");
    bt->println(s + 4); return ok("BTS");
  }

  char* cmd = strtok(s, " ");
  if (!cmd) return;
  for (char* c = cmd; *c; c++) *c = toupper(*c);
  char* a1 = strtok(NULL, " ");
  char* a2 = strtok(NULL, " ");
  char* a3 = strtok(NULL, " ");
  char* a4 = strtok(NULL, " ");
  char* a5 = strtok(NULL, " ");
  int p = parsePin(a1);

  if (!strcmp(cmd, "PING"))    { Serial.println(F("PONG")); return; }
  if (!strcmp(cmd, "STOP"))    { stopAll(); return ok("STOP"); }
  if (!strcmp(cmd, "UNWATCH")) { nWatch = 0; return ok("UNWATCH"); }
  if (!strcmp(cmd, "RATE"))    { rateMs = constrain(atol(a1), 30, 5000); return ok("RATE"); }

  if (!strcmp(cmd, "WATCH")) {
    if (!a1 || !a2) return err("WATCH tipo pin");
    char t = toupper(a1[0]);
    int wp = parsePin(a2), wp2 = parsePin(a3);
    if (wp < 0 || (t == 'U' && wp2 < 0)) return err("pin");
    for (uint8_t i = 0; i < nWatch; i++) if (watches[i].type == t && watches[i].pin == wp) return ok("WATCH");
    if (nWatch >= MAX_WATCH) return err("lista llena");
    Watch& w = watches[nWatch++];
    w.type = t; w.pin = wp; w.pin2 = wp2;
    strncpy(w.label, a2, 4); w.label[4] = 0;
    return ok("WATCH");
  }

  if (!strcmp(cmd, "MOTOR")) {
    int i1 = parsePin(a1), i2 = parsePin(a2);
    int en = (a3 && a3[0] != '-') ? parsePin(a3) : -1;
    if (i1 < 0 || i2 < 0 || !a4) return err("MOTOR in1 in2 en vel [ms]");
    setMotor(i1, i2, en, atoi(a4), a5 ? atol(a5) : 0);
    return ok("MOTOR");
  }

  if (!strcmp(cmd, "BT")) {
    int rx = parsePin(a1), tx = parsePin(a2);
    if (rx < 0 || tx < 0) return err("BT rx tx [baud]");
    if (bt) { bt->end(); delete bt; }
    bt = new SoftwareSerial(rx, tx);
    bt->begin(a3 ? atol(a3) : 9600);
    btLen = 0;
    return ok("BT");
  }

  if (p < 0) return err("pin invalido");

  if (!strcmp(cmd, "MODE")) {
    char m = a2 ? toupper(a2[0]) : 'I';
    pinMode(p, m == 'O' ? OUTPUT : (m == 'P' ? INPUT_PULLUP : INPUT));
    ok("MODE");
  } else if (!strcmp(cmd, "DW")) {
    pinMode(p, OUTPUT); digitalWrite(p, (a2 && atoi(a2)) ? HIGH : LOW); ok("DW");
  } else if (!strcmp(cmd, "PWM")) {
    pinMode(p, OUTPUT); analogWrite(p, constrain(atoi(a2), 0, 255)); ok("PWM");
  } else if (!strcmp(cmd, "DR")) {
    Serial.print(F("D ")); Serial.print(a1); Serial.print(' '); Serial.println(digitalRead(p));
  } else if (!strcmp(cmd, "AR")) {
    Serial.print(F("A ")); Serial.print(a1); Serial.print(' '); Serial.println(analogRead(p));
  } else if (!strcmp(cmd, "US")) {
    int e = parsePin(a2); if (e < 0) return err("US trig echo");
    Serial.print(F("U ")); Serial.print(a1); Serial.print(' '); Serial.println(readUltrasonic(p, e), 1);
  } else if (!strcmp(cmd, "SERVO")) {
    int i = servoSlot(p, true); if (i < 0) return err("sin servos libres");
    servos[i].write(constrain(atoi(a2), 0, 180)); ok("SERVO");
  } else if (!strcmp(cmd, "SDETACH")) {
    int i = servoSlot(p, false);
    if (i >= 0) { servos[i].detach(); servoPins[i] = -1; }
    ok("SDETACH");
  } else if (!strcmp(cmd, "TONE")) {
    unsigned long d = a3 ? atol(a3) : 0;
    if (d) tone(p, atoi(a2), d); else tone(p, atoi(a2));
    ok("TONE");
  } else if (!strcmp(cmd, "NOTONE")) {
    noTone(p); ok("NOTONE");
  } else {
    err("comando desconocido");
  }
}

void setup() {
  Serial.begin(BAUD);
  for (uint8_t i = 0; i < MAX_SERVOS; i++) servoPins[i] = -1;
  Serial.println(F("READY RoboTester v2"));
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (len) { line[len] = 0; handle(line); len = 0; }
    } else if (len < sizeof(line) - 1) {
      line[len++] = c;
    }
  }

  // parada automática de motores (seguridad)
  for (uint8_t i = 0; i < nMotors; i++)
    if (motors[i].until && (long)(millis() - motors[i].until) >= 0) motorOff(motors[i]);

  // datos que llegan por Bluetooth
  if (bt) {
    while (bt->available()) {
      char c = bt->read();
      btLast = millis();
      if (c == '\n' || c == '\r') btFlush();
      else if (btLen < sizeof(btBuf) - 1) btBuf[btLen++] = c;
    }
    if (btLen && millis() - btLast > 80) btFlush();
  }

  if (nWatch && millis() - lastSend >= rateMs) {
    lastSend = millis();
    for (uint8_t i = 0; i < nWatch; i++) sendWatch(watches[i]);
  }
}
