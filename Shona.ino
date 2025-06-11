/*
 * Control de Camas de Bronceado con ESP8266
 * Funciones: Servidor Web con login, control de relés, buzzer, LCD I2C y botón físico
 * Configuración WiFi desde navegador (WiFiManager)
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LiquidCrystal_I2C.h>
#include <WiFiManager.h>
#include <Wire.h>

LiquidCrystal_I2C pantalla(0x27, 16, 2);
ESP8266WebServer servidorWeb(80);

const int pinReleBronceado = D1;
const int pinReleEnfriado = D2;
const int pinBuzzer = D3;
const int pinBoton = D5;

unsigned long tiempoInicio = 0;
unsigned long duracionBronceado = 0;
unsigned long duracionTotal = 0;
bool estaEnProceso = false;
bool estaBronceando = false;
bool estaEnfriando = false;
bool listoParaIniciar = false;
bool procesoForzado = false;
unsigned long segundosRestantesGlobal = 0;

String obtenerEstado() {
  String estado;
  if (estaEnProceso) estado = estaBronceando ? "🟠 Bronceando" : "🔵 Enfriando";
  else if (listoParaIniciar) estado = "🕒 Esperando botón...";
  else estado = "✅ Libre";

  if (estaEnProceso) {
    unsigned int min = segundosRestantesGlobal / 60;
    unsigned int seg = segundosRestantesGlobal % 60;
    estado += " | ⏳ " + String(min) + "m " + String(seg) + "s";
  }
  return estado;
}

void detenerProceso() {
  if (!estaEnProceso) return;
  Serial.println("[USUARIO] Proceso detenido manualmente");
  estaBronceando = false;
  estaEnfriando = true;
  procesoForzado = true;
  tiempoInicio = millis();
  duracionBronceado = 0;
  duracionTotal = 60000 + 15000; // 1 minuto de enfriado + retardo
  digitalWrite(pinReleBronceado, LOW);
  digitalWrite(pinReleEnfriado, HIGH);
  pantalla.clear();
  pantalla.setCursor(0, 0);
  pantalla.print("Adelantado");
  pantalla.setCursor(0, 1);
  pantalla.print("Enfriando 1 min");
}

void bucle() {
  servidorWeb.handleClient();

  if (!estaEnProceso && !listoParaIniciar) {
    pantalla.setCursor(0, 0);
    pantalla.print("Estado: Libre   ");
    pantalla.setCursor(0, 1);
    pantalla.print("Esperando orden");
  }

  if (listoParaIniciar && digitalRead(pinBoton) == LOW) {
    Serial.println("[BOTON] Iniciando proceso...");
    tiempoInicio = millis();
    estaEnProceso = true;
    estaBronceando = false;
    estaEnfriando = false;
    listoParaIniciar = false;
    procesoForzado = false;
    pantalla.clear();
  }

  if (estaEnProceso) {
    unsigned long tiempoTranscurrido = millis() - tiempoInicio;
    segundosRestantesGlobal = (duracionTotal - tiempoTranscurrido) / 1000;

    if (tiempoTranscurrido < 15000 && !procesoForzado) {
      pantalla.setCursor(0, 0);
      pantalla.print("Inicia en: ");
      pantalla.print((15 - tiempoTranscurrido / 1000));
      pantalla.setCursor(0, 1);
      pantalla.print("Preparando...   ");
    } else {
      if (!estaBronceando && !estaEnfriando) {
        estaBronceando = true;
        digitalWrite(pinReleBronceado, HIGH);
        digitalWrite(pinReleEnfriado, HIGH);
        Serial.println("[ESTADO] Bronceando");
      }
      if (estaBronceando && tiempoTranscurrido > duracionBronceado + 15000) {
        estaBronceando = false;
        estaEnfriando = true;
        digitalWrite(pinReleBronceado, LOW);
        Serial.println("[ESTADO] Enfriando");
      }
      if (estaEnfriando && tiempoTranscurrido > duracionTotal) {
        estaEnfriando = false;
        estaEnProceso = false;
        digitalWrite(pinReleEnfriado, LOW);
        digitalWrite(pinBuzzer, LOW);
        pantalla.clear();
        pantalla.setCursor(0, 0);
        pantalla.print("Bienvenido");
        Serial.println("[FIN] Proceso completado.");
      } else {
        unsigned int min = segundosRestantesGlobal / 60;
        unsigned int seg = segundosRestantesGlobal % 60;
        pantalla.setCursor(0, 0);
        pantalla.print("Quedan: ");
        pantalla.print(min);
        pantalla.print("m");
        pantalla.print(seg);
        pantalla.print("s   ");
        pantalla.setCursor(0, 1);
        pantalla.print(estaBronceando ? "Bronceando     " : "Enfriando      ");
        if (segundosRestantesGlobal <= 15) {
          digitalWrite(pinBuzzer, millis() / 300 % 2);
        }
      }
    }
  }
}

void manejarDetener() {
  detenerProceso();
  servidorWeb.send(200, "text/plain", "Proceso detenido. Enfriando por 1 minuto");
}

void manejarEstado() {
  servidorWeb.send(200, "text/plain", obtenerEstado());
}

void manejarRaiz() {
  servidorWeb.send(200, "text/html; charset=utf-8", R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <title>Control de Camas</title>
  <style>
    body { font-family: sans-serif; background: #f3f3f3; text-align: center; padding-top: 50px; }
    .btn {
      display: inline-block; margin: 10px; padding: 20px 40px;
      background: #4CAF50; color: white; font-size: 20px;
      border: none; border-radius: 10px; cursor: pointer;
      box-shadow: 0 4px 6px rgba(0,0,0,0.1);
    }
    .btn:hover { background: #45a049; }
    .btn-secundario {
      margin-top: 20px;
      background: #2196F3;
    }
    .btn-secundario:hover {
      background: #1e88e5;
    }
    .btn-detener {
      background: #f44336;
    }
    .btn-detener:hover {
      background: #d32f2f;
    }
    #estado {
      font-size: 22px;
      font-weight: bold;
      color: #333;
      margin: 20px;
    }
  </style>
</head>
<body>
  <h1>Control de Camas</h1>
  <p id="estado">Cargando...</p>
  <button class="btn" onclick="iniciar(10)">10 Min</button>
  <button class="btn" onclick="iniciar(15)">15 Min</button>
  <button class="btn" onclick="iniciar(20)">20 Min</button>
  <br><br>
  <button class="btn btn-secundario" onclick="actualizarEstado()">Actualizar estado</button>
  <button class="btn btn-detener" onclick="detener()">Detener</button>
  <script>
    function iniciar(minutos) {
      fetch(`/iniciar?min=${minutos}`).then(() => actualizarEstado());
    }
    function actualizarEstado() {
      fetch("/estado")
        .then(response => response.text())
        .then(data => document.getElementById("estado").textContent = data);
    }
    function detener() {
      fetch("/detener").then(() => actualizarEstado());
    }
    setInterval(actualizarEstado, 1000);
    actualizarEstado();
  </script>
</body>
</html>
  )rawliteral");
}

void manejarInicio() {
  if (servidorWeb.hasArg("min")) {
    int minutos = servidorWeb.arg("min").toInt();
    if (minutos > 0) {
      duracionBronceado = minutos * 60000;
      duracionTotal = duracionBronceado + 60000;
      listoParaIniciar = true;
      pantalla.clear();
      pantalla.setCursor(0, 0);
      pantalla.print("Listo para iniciar");
      pantalla.setCursor(0, 1);
      pantalla.print("Presione boton...");
      Serial.printf("[WEB] Tiempo recibido: %d minutos\n", minutos);
    }
  }
  servidorWeb.send(200, "text/plain", "OK");
}

void configurar() {
  Serial.begin(115200);
  Serial.println("[INICIO] Iniciando sistema...");
  pinMode(pinReleBronceado, OUTPUT);
  pinMode(pinReleEnfriado, OUTPUT);
  pinMode(pinBuzzer, OUTPUT);
  pinMode(pinBoton, INPUT_PULLUP);
  digitalWrite(pinReleBronceado, LOW);
  digitalWrite(pinReleEnfriado, LOW);
  digitalWrite(pinBuzzer, LOW);

  Wire.begin(D2, D1);
  pantalla.begin(16, 2);
  pantalla.backlight();
  pantalla.setCursor(0, 0);
  pantalla.print("Bienvenido");

  WiFiManager gestorWiFi;
  gestorWiFi.autoConnect("Configuracion-Camas");

  Serial.print("[WiFi] Conectado. IP: ");
  Serial.println(WiFi.localIP());

  pantalla.clear();
  pantalla.setCursor(0, 0);
  pantalla.print("IP:");
  pantalla.setCursor(0, 1);
  pantalla.print(WiFi.localIP());
  delay(3000);
  pantalla.clear();

  servidorWeb.on("/", manejarRaiz);
  servidorWeb.on("/iniciar", manejarInicio);
  servidorWeb.on("/estado", manejarEstado);
  servidorWeb.on("/detener", manejarDetener);
  servidorWeb.begin();
  Serial.println("[WEB] Servidor web iniciado.");
}

void setup() {
  configurar();
}

void loop() {
  bucle();
}
