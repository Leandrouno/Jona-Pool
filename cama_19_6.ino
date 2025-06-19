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

void setup() {
  Serial.begin(115200);
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

  WiFiManager wm;
  wm.autoConnect("Configuracion-Camas");

  servidorWeb.on("/", manejarRaiz);
  servidorWeb.on("/iniciar", manejarInicio);
  servidorWeb.on("/estado", manejarEstado);
  servidorWeb.on("/detener", manejarDetener);
  servidorWeb.begin();

  Serial.println("Servidor iniciado:");
  Serial.println(WiFi.localIP());
}

void loop() {
  servidorWeb.handleClient();

  if (listoParaIniciar && digitalRead(pinBoton) == LOW) {
    Serial.println("Botón presionado. Iniciando proceso...");
    tiempoInicio = millis();
    estaEnProceso = true;
    estaBronceando = false;
    estaEnfriando = false;
    procesoForzado = false;
    listoParaIniciar = false;
    pantalla.clear();
  }

if (estaEnProceso) {
  unsigned long tiempoTranscurrido = millis() - tiempoInicio;
  unsigned long tiempoRestante = duracionTotal > tiempoTranscurrido
                                ? duracionTotal - tiempoTranscurrido
                                : 0;
  segundosRestantesGlobal = tiempoRestante / 1000;

  if (tiempoTranscurrido < 15000 && !procesoForzado) {
    pantalla.setCursor(0, 0);
    pantalla.print("Inicia en: ");
    pantalla.print(15 - tiempoTranscurrido / 1000);
    pantalla.print("s    ");
  } else {
    if (!estaBronceando && !estaEnfriando) {
      estaBronceando = true;
      digitalWrite(pinReleBronceado, HIGH);
      digitalWrite(pinReleEnfriado, HIGH);
      Serial.println("Bronceando iniciado.");
    }

    if (estaBronceando && tiempoTranscurrido > duracionBronceado + 15000) {
      estaBronceando = false;
      estaEnfriando = true;
      digitalWrite(pinReleBronceado, LOW);
      Serial.println("Bronceo terminado. Enfriando...");
    }

    if (tiempoTranscurrido > duracionTotal) {
      estaEnProceso = false;
      estaEnfriando = false;
      digitalWrite(pinReleEnfriado, LOW);
      digitalWrite(pinBuzzer, LOW);
      pantalla.clear();
      pantalla.setCursor(0, 0);
      pantalla.print("Bienvenido");
      Serial.println("Proceso finalizado.");
    } else {
      unsigned int minutos = segundosRestantesGlobal / 60;
      unsigned int segundos = segundosRestantesGlobal % 60;

      pantalla.setCursor(0, 0);
      pantalla.print("Quedan: ");
      pantalla.print(minutos);
      pantalla.print("m ");
      pantalla.print(segundos);
      pantalla.print("s ");

      pantalla.setCursor(0, 1);
      pantalla.print(estaBronceando ? "Bronceando    " : "Enfriando     ");

      // ---- BUZZER ----
      unsigned long finBronceado = 15000UL + duracionBronceado;
      bool ultimos15Bronceado = estaBronceando &&
                                (finBronceado > tiempoTranscurrido) &&
                                (finBronceado - tiempoTranscurrido <= 15000UL);

      bool ultimos15Totales = (segundosRestantesGlobal <= 15);

      if (ultimos15Bronceado || ultimos15Totales) {
        digitalWrite(pinBuzzer, (millis() / 300) % 2);
      } else {
        digitalWrite(pinBuzzer, LOW);
      }
    }
  }
}

}

String obtenerEstado() {
  String estado;
  if (estaEnProceso)
    estado = estaBronceando ? "🟠 Bronceando" : "🔵 Enfriando";
  else if (listoParaIniciar)
    estado = "🕒 Esperando botón...";
  else
    estado = "✅ Libre";

  if (estaEnProceso) {
    unsigned int min = segundosRestantesGlobal / 60;
    unsigned int seg = segundosRestantesGlobal % 60;
    estado += " | ⏳ " + String(min) + "m " + String(seg) + "s";
  }

  return estado;
}

void manejarEstado() {
  servidorWeb.send(200, "text/plain; charset=utf-8", obtenerEstado());
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
      Serial.printf("Proceso preparado para %d minutos\n", minutos);
    }
  }
  servidorWeb.send(200, "text/plain", "OK");
}

void manejarDetener() {
  if (estaEnProceso) {
    Serial.println("Detención forzada solicitada.");
    estaBronceando = false;
    estaEnfriando = true;
    procesoForzado = true;
    tiempoInicio = millis() - (duracionTotal - 60000);
    digitalWrite(pinReleBronceado, LOW);
    digitalWrite(pinReleEnfriado, HIGH);
    Serial.println("Rele bronceado LOW");
    Serial.println("Rele enfriado HIGH");
  }
  servidorWeb.send(200, "text/plain", "Proceso detenido");
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
    .oculto { display: none; }
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
  <div id="bloqueo">
    <h2>🔒 Verificación</h2>
  </div>
  <div id="contenido" class="oculto">
    <h1>Control de Camas</h1>
    <p id="estado">Cargando...</p>
    <button class="btn" onclick="iniciar(1)">1 Min</button>
    <button class="btn" onclick="iniciar(10)">10 Min</button>
    <button class="btn" onclick="iniciar(15)">15 Min</button>
    <button class="btn" onclick="iniciar(20)">20 Min</button>
    <br><br>
    <button class="btn btn-secundario" onclick="actualizarEstado()">Actualizar estado</button>
    <button class="btn btn-detener" onclick="detener()">Detener</button>
  </div>
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
    document.getElementById("bloqueo").style.display = "none";
    document.getElementById("contenido").classList.remove("oculto");

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
