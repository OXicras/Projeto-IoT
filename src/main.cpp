#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

LiquidCrystal_I2C lcd (0x27, 20, 4);
String pedirParaApi(String);
void conectarMQTT();

const int pinBotao = 0;
int contador = 0;
const char* nomeRedeWiFi = "SENAI EDUCACIONAL";
const char* senhaRedeWiFi = "info@134";
String enderecoAPI = "https://api.batmanapi.com/v1/characters/";

const char* BROKER = "broker.emqx.io";
const int PORTA = 1883;
const char* TOPICO_SUB = "senai134/esp32/batmanApiBM/espBruno";
const char* TOPICO_PUB = "senai134/esp32/batmanApiBM/espMatheus";

void aoReceber(char*, byte*, unsigned int);
void tratarJson(String);

WiFiClient clienteWifi;
PubSubClient mqtt(clienteWifi);

JsonDocument doc;

void printApiLcd();

void setup() 
{
  #pragma region Inicializacao
  Serial.begin(115200);
  lcd.init();
  lcd.backlight();
  WiFi.begin(nomeRedeWiFi, senhaRedeWiFi);
  pinMode(pinBotao, INPUT_PULLUP);
  #pragma endregion

  #pragma region Conectando Wifi
  lcd.setCursor(0,0);
  Serial.println();
  while(WiFi.status() != WL_CONNECTED)
  {
    lcd.print("#");
    Serial.print("#");
    delay(500);
  }
  Serial.println();
  lcd.clear();
  #pragma endregion

  #pragma region Conectando MQTT
  mqtt.setServer(BROKER, PORTA);
  mqtt.setCallback(aoReceber);
  String id = "esp32-" + String((uint32_t)ESP.getEfuseMac(), HEX);

  bool conectadoAoBroker = false;
  conectadoAoBroker = mqtt.connect(id.c_str());

  if(conectadoAoBroker)
    {
      Serial.printf("Conectado com sucesso ao Broker %s\n\r", BROKER);
      mqtt.subscribe(TOPICO_SUB);
      Serial.printf("Inscrito no tópico: %s\n\r", TOPICO_SUB);
    }

  else
    {
      Serial.print("Falha ao conectar ao broker %s\n\r");
      Serial.print("Falha: "); Serial.println(mqtt.state());
    }
    #pragma endregion

    rgbLedWrite(48, 0, 0, 0);
}

void loop() 
{
  mqtt.loop();
  
  #pragma region Botão
  bool estadoBotao = digitalRead(pinBotao);
  static bool estadoAnterior = estadoBotao;
  if(estadoBotao != estadoAnterior)
  {
    if(!estadoBotao)
    {
      contador++;
      
      String mensagem = String(contador);
      mqtt.publish(TOPICO_PUB, mensagem.c_str());
      Serial.printf("Mensagem a ser enviada no tópico %s:\n\r%s\n\r", TOPICO_PUB, mensagem.c_str());
    }
  }
  estadoAnterior = estadoBotao; 
  #pragma endregion

  if (contador >= 83)
  {
    contador = 0;
    rgbLedWrite(48, 0, 0, 0);
  }
}

String pedirParaApi(String codigo)
{
  HTTPClient http;
  http.begin(enderecoAPI + codigo + "/");
  int codigoHTTP = http.GET();
  String resposta;

  if(codigoHTTP == 200)
  {
    resposta = http.getString();
  }
  else
  {
    Serial.printf("\n\rDeu erro. Código: %d", codigoHTTP);
  }
  http.end();
  return resposta;
}

void printApiLcd()
{
  const char* nome = doc["data"]["attributes"]["name"];
  const char* apelido = doc["data"]["attributes"]["alias"];
  const char* genero = doc["data"]["attributes"]["gender"];
  const char* cargo = doc["data"]["attributes"]["role"];
  
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.printf("Nome: %s", nome);
  lcd.setCursor(0,1);
  lcd.printf("Apelido: %s", apelido);
  lcd.setCursor(0,2);
  lcd.printf("Status: %s", genero);

  if(strcmp(genero, "Male") == 0)
   rgbLedWrite(48, 0, 0, 255);
  else
   rgbLedWrite(48, 255, 0, 0);

  lcd.setCursor(0,3);
  lcd.printf("Cargo: %s", cargo);
}

void aoReceber(char* topico, uint8_t* payload, unsigned int tamanho)
{
  String mensagem;
  mensagem.reserve(tamanho);

  for(unsigned int i = 0; i < tamanho; i++)
  {
    mensagem += (char)payload[i];
  }

  Serial.printf("Mensagem Recebida no tópico: %s \n\r mensagem: %s\n\r", topico, 
  mensagem.c_str());

  tratarJson(mensagem);
}

void tratarJson(String mensagem)
{
  String batmanApi = pedirParaApi(mensagem);

  if (batmanApi != "")
  {
    doc.clear();

    DeserializationError erroJson = deserializeJson(doc, batmanApi);
    if(erroJson)
    {
      Serial.printf("Erro ao deserializar: %s\n\r", erroJson.c_str());
      return;
    }
      
  String mensagem;
  serializeJson(doc, mensagem);
  }

  DeserializationError codigoErro = deserializeJson(doc, mensagem);

  if (!codigoErro)
    printApiLcd();
  else
    Serial.printf("Erro %s\n\r", codigoErro);
}

void conectarMQTT() {
  static unsigned long ultimaTentativa = 0;
  
  if (!mqtt.connected() && (millis() - ultimaTentativa > 2000)) {
    ultimaTentativa = millis();
    Serial.println("Tentando conectar ao MQTT");
    String id = "esp32-" + String((uint32_t)ESP.getEfuseMac(), HEX);
    
    if (mqtt.connect(id.c_str())) {
      mqtt.subscribe(TOPICO_SUB);
      Serial.printf("Inscrito no tópico: %s\n\r", TOPICO_SUB);
    } else {
      Serial.print("Falha ao conectar. Código de estado: "); 
      Serial.println(mqtt.state());
    }
  }
}