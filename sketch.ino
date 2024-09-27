#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#define LORA_SS 5
#define LORA_RST 14
#define LORA_DIO0 10

const char* ssid = SSID;
const char* password = PASSWORD;
const char* serverName = API/DATA;
const char* instructionsServer = API/INSTRUCTIONS;
const char* measureServer = API/MEASURE; 

unsigned long lastInstructionCheck = 0;
unsigned long lastMeasureCheck = 0; 
const unsigned long instructionCheckInterval = 10000;
const unsigned long measureCheckInterval = 10000; 
int previousThreshold = -1; 
String lastMeasureTime = ""; 

void setup() {
  Serial.begin(115200);
  while (!Serial);

  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  
  if (!LoRa.begin(433E6)) {
    Serial.println("Inicjalizacja LoRa nie powiodła się.");
    while (1);
  }

  Serial.println("Odbiornik LoRa gotowy.");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nPołączono z WiFi");
}

void loop() {
  int packetSize = LoRa.parsePacket();

  if (packetSize > 0) { 
    byte packetType = LoRa.read(); 

    if (packetType == 0x01) { 
      Serial.println("Odebrano prośbę o pomiar.");
    } else if (packetType == 0x02) { 
      int moisture = LoRa.read(); 
      int relayState = LoRa.read(); 
      int threshold = LoRa.read(); 

      if (moisture >= 0 && moisture <= 100 && (relayState == 0 || relayState == 1) && (threshold >= 0 && threshold <= 100)) {
        Serial.print("Odebrano dane: Wilgotność: ");
        Serial.print(moisture);
        Serial.print("%, Stan przekaźnika: ");
        Serial.print(relayState);
        Serial.print(", Próg: ");
        Serial.println(threshold);
        sendDataToServer(moisture, relayState, threshold);
      } else {
        Serial.println("Odebrane dane są nieprawidłowe.");
      }
    }
  }

  unsigned long currentMillis = millis();
  if (currentMillis - lastInstructionCheck >= instructionCheckInterval) {
    getInstructions();
    lastInstructionCheck = currentMillis;
  }

  if (currentMillis - lastMeasureCheck >= measureCheckInterval) {
    checkLastMeasure();
    lastMeasureCheck = currentMillis;
  }
}

void sendDataToServer(int moisture, int relayState, int newMeasurement) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverName);
    http.addHeader("Content-Type", "application/json");

    String payload = "{\"soilMoisture\": " + String(moisture) + ", \"relayState\": " + String(relayState) + ", \"newMeasurement\": " + String(newMeasurement) + "}";

    int httpResponseCode = http.POST(payload);

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("Response Code: " + String(httpResponseCode));
      Serial.println("Response: " + response);
    } else {
      Serial.println("Błąd podczas wysyłania żądania POST");
      Serial.println("Response Code: " + String(httpResponseCode));
    }

    http.end();
  } else {
    Serial.println("Brak połączenia z WiFi");
  }
}


void getInstructions() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(instructionsServer);
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      String response = http.getString();

      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, response);
      
      if (!error) {
        int threshold = doc["threshold"];
        Serial.print("Nowy próg: ");
        Serial.println(threshold);

        if (threshold != previousThreshold && threshold >= 0 && threshold <= 100) {
          sendThresholdToLoRa(threshold);
          previousThreshold = threshold; 
        } else if (threshold < 0 || threshold > 100) {
          Serial.println("Wartość progu jest poza dozwolonym zakresem.");
        }
      } else {
        Serial.print("Błąd deserializacji JSON: ");
        Serial.println(error.f_str());
      }
    } else {
      Serial.println("Błąd podczas pobierania instrukcji");
      Serial.println("Response Code: " + String(httpResponseCode));
    }

    http.end();
  } else {
    Serial.println("Brak połączenia z WiFi do pobrania instrukcji");
  }
}

void checkLastMeasure() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(measureServer);
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      String response = http.getString();

      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, response);

      if (!error) {
        String latestTime = doc["createdAt"];

        Serial.println("Otrzymany czas pomiaru: " + latestTime);

        if (latestTime != lastMeasureTime) {
          Serial.println("Nowa prośba o pomiar: " + latestTime);
          lastMeasureTime = latestTime; // Aktualizuj czas ostatniego pomiaru

          sendRequestForMeasurement();
        } else {
          Serial.println("Brak prośby.");
        }
      } else {
        Serial.print("Błąd deserializacji JSON: ");
        Serial.println(error.f_str());
      }
    } else {
      Serial.println("Błąd podczas pobierania danych pomiarowych");
      Serial.println("Response Code: " + String(httpResponseCode));
    }

    http.end();
  } else {
    Serial.println("Brak połączenia z WiFi do pobrania danych pomiarowych");
  }
}

void sendThresholdToLoRa(int threshold) {
  LoRa.beginPacket();
  LoRa.write(0x02);  
  LoRa.write(threshold); 
  LoRa.endPacket();

  Serial.print("Wysłano nowy próg: ");
  Serial.println(threshold);
}

void sendRequestForMeasurement() {
  LoRa.beginPacket();
  LoRa.write(0x01);  
  LoRa.endPacket();

  Serial.println("Wysłano prośbę o pomiar");
}
