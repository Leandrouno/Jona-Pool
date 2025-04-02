#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <ArduinoJson.h>

const char* ssid = "";
const char* password = "";

IPAddress ip(192, 168, 0, 13);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);

int paso[4][4] = {
  {1, 1, 0, 0},
  {0, 1, 1, 0},
  {0, 0, 1, 1},
  {1, 0, 0, 1}
};

int IN1 = D1, IN2 = D2, IN3 = D3, IN4 = D4;
int lamparaPin = D8;
WiFiServer server(80);
int posicion = 0;
unsigned long tiempoApertura = 0;
int tiempoSolicitud = 0;
int pasosMotor = 0;

#define EEPROM_SIZE 512 

void setup() {
    EEPROM.begin(EEPROM_SIZE);
    posicion = EEPROM.read(0);
    pinMode(IN1, OUTPUT);
    pinMode(IN2, OUTPUT);
    pinMode(IN3, OUTPUT);
    pinMode(IN4, OUTPUT);
    pinMode(lamparaPin, OUTPUT);
    digitalWrite(lamparaPin, LOW);
    
    Serial.begin(115200);
    delay(10);

    WiFi.mode(WIFI_STA);
    WiFi.config(ip, gateway, subnet);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected");
    server.begin();
    Serial.println("Servidor iniciado");
}

void loop() {
    WiFiClient client = server.available();
    if (!client) {
        verificarCierreAutomatico();
        return;
    }
    String request = client.readStringUntil('\r');
    Serial.println("Solicitud: " + request);
    client.flush();

    if (!verificarCredenciales(request)) {
        enviarRespuesta(client, "error", "Acceso denegado");
        return;
    }

    procesarAccion(request, client);
    verificarCierreAutomatico();
}

void procesarAccion(String request, WiFiClient &client) {
    if (request.indexOf("lampara=") != -1) {
        bool encender = request.indexOf("lampara=encender") != -1;
        digitalWrite(lamparaPin, encender ? HIGH : LOW);
        enviarRespuesta(client, "ok", encender ? "Lámpara Encendida" : "Lámpara Apagada");
        return;
    }

    if (request.indexOf("/abrir") != -1) {
        if (posicion == 1) {
            int tiempoRestante = (tiempoApertura + (tiempoSolicitud * 1000) - millis()) / 1000;
            enviarRespuesta(client, "error", ("Compuerta ya abierta, se cerrará en " + String(tiempoRestante) + " segundos").c_str());
            return;
        }
        String tiempoStr = getQueryParam(request, "tiempo");
        if (tiempoStr == "") {
            enviarRespuesta(client, "error", "Debe enviar el parametro 'tiempo'");
            return;
        }
        int tiempo = tiempoStr.toInt();
        if (tiempo <= 0) {
            enviarRespuesta(client, "error", "El tiempo debe ser mayor a 0");
            return;
        }
        moverMotor(true);
        enviarRespuesta(client, "ok", "Compuerta abierta");
        tiempoApertura = millis();
        tiempoSolicitud = tiempo;
        return;
    }

    if (request.indexOf("/cerrar") != -1) {
        moverMotor(false);
        enviarRespuesta(client, "ok", "Compuerta cerrada");
        tiempoApertura = 0;
        pasosMotor = 0;
        return;
    }

    if (request.indexOf("/estado") != -1) {
        enviarRespuestaEstado(client);
        return;
    }

    enviarRespuesta(client, "error", "Parametro inexistente");
}

void verificarCierreAutomatico() {
    if (tiempoApertura > 0 && millis() - tiempoApertura >= tiempoSolicitud * 1000) {
        moverMotor(false);
        tiempoApertura = 0;
        pasosMotor = 0;
    }
}

void enviarRespuesta(WiFiClient& client, const char* status, const char* mensaje) {
    StaticJsonDocument<200> json;
    json["estado"] = status;
    json["mensaje"] = mensaje;
    String respuesta;
    serializeJson(json, respuesta);
    client.println("HTTP/1.1 200 OK\nContent-Type: application/json\nConnection: close\n");
    client.println();
    client.println(respuesta);
}

void enviarRespuestaEstado(WiFiClient& client) {
    StaticJsonDocument<200> json;
    json["estado"] = "ok";
    json["compuerta"] = posicion == 1 ? "abierta" : "cerrada";
    json["lampara"] = digitalRead(lamparaPin) == HIGH ? "encendida" : "apagada";
    json["pasos_motor"] = pasosMotor;

    if (posicion == 1 && tiempoApertura > 0) {
        int tiempoRestante = (tiempoApertura + (tiempoSolicitud * 1000) - millis()) / 1000;
        if (tiempoRestante > 0) {
            json["tiempo_restante"] = tiempoRestante;
        } else {
            json["tiempo_restante"] = 0;  // Si el tiempo ya expiró, mostrar 0
        }
    }

    String respuesta;
    serializeJson(json, respuesta);
    client.println("HTTP/1.1 200 OK\nContent-Type: application/json\nConnection: close\n");
    client.println();
    client.println(respuesta);
}


String getQueryParam(String request, String param) {
    int startIndex = request.indexOf(param + "=");
    if (startIndex == -1) return "";
    startIndex += param.length() + 1;
    int endIndex = request.indexOf("&", startIndex);
    if (endIndex == -1) endIndex = request.length();
    return request.substring(startIndex, endIndex);
}

bool verificarCredenciales(String request) {
    return request.indexOf("usuario=Jona") != -1 && request.indexOf("llave=Pool1234") != -1;
}

void moverMotor(bool abrir) {
  
    if ((abrir && posicion == 1) || (!abrir && posicion == 0)) return;
    pasosMotor++;
    for (int i = 0; i < 180; i++) {
        for (int j = 0; j < 4; j++) {
            digitalWrite(IN1, abrir ? paso[j][0] : paso[3 - j][0]);
            digitalWrite(IN2, abrir ? paso[j][1] : paso[3 - j][1]);
            digitalWrite(IN3, abrir ? paso[j][2] : paso[3 - j][2]);
            digitalWrite(IN4, abrir ? paso[j][3] : paso[3 - j][3]);
            delay(2);
        }
         pasosMotor++;
    }
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW);
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, LOW);
    posicion = abrir ? 1 : 0;
    EEPROM.write(0, posicion);
    EEPROM.commit();
}
