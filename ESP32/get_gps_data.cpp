#include "SPIFFS.h"
#include <SD.h>
#include <WiFi.h>
#include <WebServer.h>
#include "time.h"

#include <HardwareSerial.h>
#include <TinyGPS++.h>

// Define the RX and TX pins for Serial 2
#define RXD2 16
#define TXD2 17

#define GPS_BAUD 9600

// The TinyGPS++ object
TinyGPSPlus gps;

// Create an instance of the HardwareSerial class for Serial 2
HardwareSerial gpsSerial(2);

const char* ssid = "GESPIN_WIFI";       
const char* password = "gespin123"; 

WebServer server(80);

#define INTERVALO_SALVAR 1000  // 5 segundos
#define INTERVALO_LEITURA 1000
#define PINO_CS_SD 5            // Pino CS do SD Card
bool coletaAtiva = false;       
unsigned long ultimaLeitura = 0;

// Servidor NTP para obter data/hora
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -10800;  // Fuso horário do Brasil (UTC-3)
const int   daylightOffset_sec = 0;

// 📆 Função para obter o timestamp (data/hora)
String getTimestamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "Erro no RTC";
  }
  char buffer[30];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buffer);
}

// 📂 Salva coordenadas, RDP, Timestamp, Altitude, HDOP e Satellites no formato CSV
void salvarCoordenadas() {
  if (!coletaAtiva) return;

  if (millis() - ultimaLeitura >= INTERVALO_SALVAR) {
    ultimaLeitura = millis();

    // Obtenha os valores de latitude, longitude, altitude, HDOP e número de satélites
    String latitude = String(gps.location.lat(), 6);              // Latitude
    String longitude = String(gps.location.lng(), 6);             // Longitude
    String altitude = String(gps.altitude.meters(), 6);           // Altitude
    float hdop = gps.hdop.value() / 100.0;                        // HDOP
    int satellites = gps.satellites.value();                      // Número de satélites

    // Timestamp NTP (RTC do ESP32)
    String timestampNTP = getTimestamp();

    // Timestamp GNSS (direto do GPS)
    String timestampGNSS = String(gps.date.year()) + "-" +
                           String(gps.date.month()) + "-" +
                           String(gps.date.day()) + " " +
                           String(gps.time.hour()) + ":" +
                           String(gps.time.minute()) + ":" +
                           String(gps.time.second());

    // Linha no CSV
    String linha = timestampNTP + "," + timestampGNSS + "," + latitude + "," + longitude + "," + altitude + "," + hdop + "," + satellites + "\n";

    // 📝 Salvar no SPIFFS (arquivo CSV)
    File arquivoSPIFFS = SPIFFS.open("/coordenadas.csv", FILE_APPEND);
    if (arquivoSPIFFS) {
      arquivoSPIFFS.print(linha);
      arquivoSPIFFS.close();
    }

    // 📝 Salvar no cartão SD (arquivo CSV)
    File arquivoSD = SD.open("/coordenadas.csv", FILE_APPEND);
    if (arquivoSD) {
      arquivoSD.print(linha);
      arquivoSD.close();
    }

    Serial.println("Coordenada salva: " + linha);
  }
}

// 📂 Envia o arquivo CSV para download
void handleDownload() {
  File arquivo = SD.open("/coordenadas.csv", FILE_READ);
  if (!arquivo) {
    server.send(404, "text/plain", "Arquivo não encontrado!");
    return;
  }

  server.sendHeader("Content-Type", "text/csv");
  server.sendHeader("Content-Disposition", "attachment; filename=coordenadas.csv");
  server.sendHeader("Connection", "close");

  String dados = "";
  while (arquivo.available()) {
    dados += (char)arquivo.read();
  }
  arquivo.close();
  server.send(200, "text/csv", dados);
}

// 📂 Apaga o arquivo CSV do SPIFFS e do cartão SD
void handleClear() {
  SPIFFS.remove("/coordenadas.csv");
  SD.remove("/coordenadas.csv");

  File novoArquivoSPIFFS = SPIFFS.open("/coordenadas.csv", FILE_WRITE);
  novoArquivoSPIFFS.close();

  File novoArquivoSD = SD.open("/coordenadas.csv", FILE_WRITE);
  novoArquivoSD.close();

  server.send(200, "text/plain", "Arquivo apagado!");
}

// 🚀 Inicia a coleta
void handleStart() {
  coletaAtiva = true;
  server.send(200, "text/plain", "Coleta iniciada!");
}

// 🚫 Para a coleta
void handleStop() {
  coletaAtiva = false;
  server.send(200, "text/plain", "Coleta parada!");
}

// 📡 Retorna o status da coleta
void handleStatus() {
  server.send(200, "text/plain", coletaAtiva ? "Coletando" : "Parado");
}

// 🌍 Página WebServer
void handleRoot() {
  String pagina = R"rawliteral(
    <html>
    <head>
      <title>ESP32 GPS Logger</title>
      <style>
        body { font-family: Arial; text-align: center; margin-top: 50px; }
        button { font-size: 20px; padding: 10px; margin: 10px; cursor: pointer; }
      </style>
    </head>
    <body>
      <h1>ESP32 GPS Logger</h1>
      <button onclick="fetch('/start')">Iniciar Coleta</button>
      <button onclick="fetch('/stop')">Parar Coleta</button>
      <button onclick="window.location.href='/download'">Baixar Dados</button>
      <button onclick="fetch('/clear')">Apagar Dados</button>
      <p>Status: <span id="status">Parado</span></p>

      <script>
        setInterval(async () => {
          let res = await fetch('/status');
          let status = await res.text();
          document.getElementById('status').innerText = status;
        }, 2000);
      </script>
    </body>
    </html>
  )rawliteral";

  server.send(200, "text/html", pagina);
}

// 🚀 Configuração inicial
void setup() {
  Serial.begin(115200);

  // Start Serial 2 with the defined RX and TX pins and a baud rate of 9600
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, RXD2, TXD2);
  Serial.println("Serial 2 started at 9600 baud rate");

  // 🔌 Conectar ao Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Conectando ao Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\nWi-Fi Conectado!");
  Serial.println(WiFi.localIP());

  // 📂 Iniciar SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("Erro ao iniciar SPIFFS");
    return;
  }

  // 💾 Iniciar SD Card
  if (!SD.begin(PINO_CS_SD)) {
    Serial.println("Erro ao iniciar o SD Card!");
    return;
  }

  // ⏰ Configurar RTC via NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("RTC configurado!");

  // 🌍 Configuração do servidor web
  server.on("/", HTTP_GET, handleRoot);
  server.on("/download", HTTP_GET, handleDownload);
  server.on("/clear", HTTP_GET, handleClear);
  server.on("/start", HTTP_GET, handleStart);
  server.on("/stop", HTTP_GET, handleStop);
  server.on("/status", HTTP_GET, handleStatus);
  server.begin();

  Serial.println("Servidor Web Iniciado!");
}

// 🚀 Loop principal
void loop() {
  server.handleClient();

  unsigned long start = millis();

  while (millis() - start < INTERVALO_LEITURA) {
    while (gpsSerial.available() > 0) {
      gps.encode(gpsSerial.read());
    }
    if (gps.location.isUpdated()) {
      Serial.print("LAT: ");
      Serial.println(gps.location.lat(), 6);
      Serial.print("LONG: "); 
      Serial.println(gps.location.lng(), 6);
      Serial.print("SPEED (km/h) = "); 
      Serial.println(gps.speed.kmph()); 
      Serial.print("ALT (min)= "); 
      Serial.println(gps.altitude.meters());
      Serial.print("HDOP = "); 
      Serial.println(gps.hdop.value() / 100.0); 
      Serial.print("Satellites = "); 
      Serial.println(gps.satellites.value()); 
      Serial.print("Time in UTC: ");
      Serial.println(String(gps.date.year()) + "/" + String(gps.date.month()) + "/" + String(gps.date.day()) + "," + String(gps.time.hour()) + ":" + String(gps.time.minute()) + ":" + String(gps.time.second()));
      Serial.println("");
    }
  }

  salvarCoordenadas();
}
