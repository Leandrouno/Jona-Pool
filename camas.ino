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

LiquidCrystal_I2C pantalla(0x27, 16, 2); // Dirección común para LCD I2C
ESP8266WebServer servidorWeb(80);

// Pines
const int pinReleBronceado = D1;
const int pinReleEnfriado = D2;
const int pinBuzzer = D3;
const int pinBoton = D5; // Botón físico

unsigned long tiempoInicio = 0;
unsigned long duracionBronceado = 0;
unsigned long duracionTotal = 0;
bool estaEnProceso = false;
bool estaBronceando = false;
bool estaEnfriando = false;
bool listoParaIniciar = false;

void configurar() {
  Serial.begin(115200);
  pinMode(pinReleBronceado, OUTPUT);
  pinMode(pinReleEnfriado, OUTPUT);
  pinMode(pinBuzzer, OUTPUT);
  pinMode(pinBoton, INPUT_PULLUP);
  digitalWrite(pinReleBronceado, LOW);
  digitalWrite(pinReleEnfriado, LOW);
  digitalWrite(pinBuzzer, LOW);

  pantalla.begin();
  pantalla.backlight();
  pantalla.setCursor(0, 0);
  pantalla.print("Bienvenido");

  WiFiManager gestorWiFi;
  gestorWiFi.autoConnect("Configuracion-Camas");

  servidorWeb.on("/", manejarRaiz);
  servidorWeb.on("/iniciar", manejarInicio);
  servidorWeb.begin();

  Serial.println("Servidor iniciado en:");
  Serial.println(WiFi.localIP());
}

void bucle() {
  servidorWeb.handleClient();

  if (listoParaIniciar && digitalRead(pinBoton) == LOW) {
    tiempoInicio = millis();
    estaEnProceso = true;
    estaBronceando = false;
    estaEnfriando = false;
    listoParaIniciar = false;
    pantalla.clear();
  }

  if (estaEnProceso) {
    unsigned long tiempoTranscurrido = millis() - tiempoInicio;
    unsigned long segundosRestantes = (duracionTotal - tiempoTranscurrido) / 1000;

    if (tiempoTranscurrido < 15000) {
      pantalla.setCursor(0, 0);
      pantalla.print("Inicia en: ");
      pantalla.print((15 - tiempoTranscurrido / 1000));
      pantalla.print("s     ");
    } else {
      if (!estaBronceando && !estaEnfriando) {
        estaBronceando = true;
        digitalWrite(pinReleBronceado, HIGH);
        digitalWrite(pinReleEnfriado, HIGH);
      }

      if (estaBronceando && tiempoTranscurrido > duracionBronceado + 15000) {
        estaBronceando = false;
        estaEnfriando = true;
        digitalWrite(pinReleBronceado, LOW);
      }

      if (estaEnfriando && tiempoTranscurrido > duracionTotal) {
        estaEnfriando = false;
        estaEnProceso = false;
        digitalWrite(pinReleEnfriado, LOW);
        digitalWrite(pinBuzzer, LOW);
        pantalla.clear();
        pantalla.setCursor(0, 0);
        pantalla.print("Bienvenido");
      } else {
        pantalla.setCursor(0, 0);
        pantalla.print(segundosRestantes);
        pantalla.print(" seg     ");

        pantalla.setCursor(0, 1);
        pantalla.print(estaBronceando ? "Bronceando     " : "Enfriando      ");

        if (segundosRestantes <= 15) {
          digitalWrite(pinBuzzer, millis() / 300 % 2);
        }
      }
    }
  }
}

void manejarRaiz() {
  servidorWeb.send(200, "text/html", R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Camas Bronceado</title>
  <style>
    body { font-family: sans-serif; background: #f3f3f3; text-align: center; padding-top: 50px; }
    .btn {
      display: inline-block; margin: 10px; padding: 20px 40px;
      background: #4CAF50; color: white; font-size: 20px;
      border: none; border-radius: 10px; cursor: pointer;
      box-shadow: 0 4px 6px rgba(0,0,0,0.1);
    }
    .btn:hover { background: #45a049; }
  </style>
</head>
<body>
  <script>
    if (!localStorage.loginOK) {
      let usuario = prompt("Usuario:");
      let contrasena = prompt("Contraseña:");
      if (usuario !== "camas" || contrasena !== "camas2025") {
        alert("Acceso denegado");
        location.reload();
      } else {
        localStorage.loginOK = true;
      }
    }
  </script>
  <h1>Control de Camas</h1>
  <p>Luego de elegir el tiempo, presiona el botón físico para iniciar.</p>
  <button class="btn" onclick="iniciar(10)">10 Min</button>
  <button class="btn" onclick="iniciar(15)">15 Min</button>
  <button class="btn" onclick="iniciar(20)">20 Min</button>
  <script>
    function iniciar(minutos) {
      fetch(`/iniciar?min=${minutos}`);
    }
  </script>
</body>
</html>
  )rawliteral");
}

void manejarInicio() {
  if (servidorWeb.hasArg("min")) {
    int minutos = servidorWeb.arg("min").toInt();
    if (minutos > 0) {
      duracionBronceado = minutos * 60 * 1000;
      duracionTotal = duracionBronceado + 60000; // +1 minuto de enfriamiento
      listoParaIniciar = true;
      pantalla.clear();
      pantalla.setCursor(0, 0);
      pantalla.print("Listo para iniciar");
      pantalla.setCursor(0, 1);
      pantalla.print("Presione boton...");
    }
  }
  servidorWeb.send(200, "text/plain", "OK");
}
