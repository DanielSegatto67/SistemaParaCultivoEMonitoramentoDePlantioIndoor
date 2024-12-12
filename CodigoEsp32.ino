#include <dht11.h> //Biblioteca DHT
#include <WiFi.h> //Biblioteca Wifi
#include <time.h> // Biblioteca para pegar a hora atual
#include <Firebase_ESP_Client.h> //Biblioteca para conectar com o banco de dados(FIREBASE)

/* 1. Define the WiFi credentials para conectar na rede Wifi*/
#define WIFI_SSID "************"
#define WIFI_PASSWORD "**********"

/* 2. Define the API Key para conectar com o banco de dados*/
#define API_KEY "************************************"

/* 3. Define o usuário e senha para a conexão do projeto */
#define USER_EMAIL "******************"
#define USER_PASSWORD "*********************"

/* 4. Define the RTDB URL */
#define DATABASE_URL "******************************" 

// Define Firebase Data object
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Definição das entradas e saídas
#define DHT11PIN 27 
const int UmidadeSolo = 32;
const int sensorLuz = 25; //sensor de luz
dht11 DHT11;

// Declaração das variáveis
float temperatura_antiga = -10;

int luminosidade;
int luminosidade_antiga = -10;
String luminosidadeString;
int estadoLuz;

int umidade;
int umidadeMinima;
float umidadeTratado;
float umidadeTratado_antiga = -10; 

int umidadeAr_antiga = -5;
int suspenderLampada;
int suspenderIrrigacao;

int horaAtual;
int minutoAtual;

String dataTratado, horaTratado;

String horaInicio, horaFim;

int dia, mes, ano = 0;

int horaInicioTratado;
int minutoInicioTratado;

int horaFimTratado;
int minutoFimTratado;

int travaIrrigacao;
int habilitarTravaIrrigacao;

int umidadeMinimaAux = 35;
int horaInicioAux = 8;
int minutoInicioAux = 0; 
int horaFimAux = 20;
int minutoFimAux = 0; 
int suspenderLampadaAux = 0; 
int suspenderIrrigacaoAux = 0; 
int habilitarTravaIrrigacaoAux = 1;

unsigned long timerTravaIrrigacao;
unsigned long timerirrigacao = -1;

unsigned long sendDataPrevMillis = 0;
unsigned long sendDataPrevMillisHist = 0;

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -3 * 3600; // GMT-3
const int daylightOffset_sec = 0; // Sem horário de verão

void setup() {
  //Configuração dos terminais
  pinMode(26, OUTPUT); //Relé
  pinMode(14, OUTPUT); //Relé
  pinMode(sensorLuz, INPUT);

  digitalWrite(26, HIGH);
  digitalWrite(14, HIGH);

  Serial.begin(115200); //Define velocidade de operação

  //Conexão com Wifi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  //Configuração Firebase
  config.api_key = API_KEY;

  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  config.database_url = DATABASE_URL;

  Firebase.reconnectNetwork(true);

  fbdo.setBSSLBufferSize(4096, 1024);
  fbdo.setResponseSize(2048);

  Firebase.begin(&config, &auth);
  Firebase.setDoubleDigits(5);

  config.timeout.serverResponse = 10 * 1000;

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer); // Configurações de horário
}

void verificarHorario() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
      Serial.println("Failed to obtain time");
      horaInicioTratado = horaInicioAux;
      minutoInicioTratado = minutoInicioAux;
      horaFimTratado = horaFimAux;
      minutoFimTratado = minutoFimAux;
    } else {
      horaAtual = timeinfo.tm_hour;
      minutoAtual = timeinfo.tm_min;

      dia = timeinfo.tm_mday;
      mes = timeinfo.tm_mon + 1; 
      ano = timeinfo.tm_year + 1900; 

      // Extrai a parte da hora e converte para int
      horaInicioTratado = horaInicio.substring(0, 2).toInt();
      minutoInicioTratado = horaInicio.substring(3, 5).toInt();

      horaFimTratado = horaFim.substring(0, 2).toInt();
      minutoFimTratado = horaFim.substring(3, 5).toInt();
    }


  //Serial.println("Hora atual: " + horaAtual + ":" + minutoAtual);
  Serial.println("Hora atual: " + String(horaAtual) + ":" + String(minutoAtual));

  if (suspenderLampada == 0) {
    // Determina se o intervalo cruza a meia-noite
    bool horarioInvertido = horaInicioTratado > horaFimTratado;

    // Verifica se a lâmpada deve estar ligada
    bool lampadaLigada = false;
    if (horarioInvertido) {
      // Intervalo cruza a meia-noite (ex: das 22:00 às 06:00)
      lampadaLigada = 
        (horaAtual > horaInicioTratado || (horaAtual == horaInicioTratado && minutoAtual >= minutoInicioTratado)) || 
        (horaAtual < horaFimTratado || (horaAtual == horaFimTratado && minutoAtual <= minutoFimTratado));
    } else {
      // Intervalo normal (ex: das 08:00 às 20:00)
      lampadaLigada = 
        (horaAtual > horaInicioTratado || (horaAtual == horaInicioTratado && minutoAtual >= minutoInicioTratado)) &&
        (horaAtual < horaFimTratado || (horaAtual == horaFimTratado && minutoAtual <= minutoFimTratado));
    }

    // Controle do relé da lâmpada
    if (lampadaLigada) {
      Serial.println("Estado da lâmpada: Ligada");
      digitalWrite(26, LOW); // Liga o relé
    } else {
      Serial.println("Estado da lâmpada: Desligada");
      digitalWrite(26, HIGH); // Desliga o relé
    }
  } else {
    Serial.println("Trava de acionamento da lâmpada ativada!");
    Serial.println("Estado da lâmpada: Desligada");
    digitalWrite(26, HIGH); // Força o desligamento da lâmpada
  }
}

void loop() {

  estadoLuz = digitalRead(sensorLuz);

  if (digitalRead(sensorLuz) == HIGH) {
    luminosidade = 0;
  } else {
    luminosidade = 1;
  }
  
  int chk = DHT11.read(DHT11PIN);

  umidade = 4095 - analogRead(UmidadeSolo);
  umidadeTratado = map((int)umidade, 0, 4095, 0, 100);

  if (Firebase.ready() && (millis() - sendDataPrevMillis > 5000 || sendDataPrevMillis == 0)) {
    sendDataPrevMillis = millis();

    //Funções para enviar dados
    if (abs((float)DHT11.humidity - umidadeAr_antiga) > 1) {
      if (Firebase.RTDB.setInt(&fbdo, "/umidadeAr", (float)DHT11.humidity)) {
        umidadeAr_antiga = (float)DHT11.humidity;
      } else {
        Serial.println("FAILED: " + fbdo.errorReason());
      }
    }

    if (abs((float)DHT11.temperature - temperatura_antiga) > 1) {
      if (Firebase.RTDB.setInt(&fbdo, "/temperaturaAr", (float)DHT11.temperature)) {

        temperatura_antiga = (float)DHT11.temperature;
      } else {
        Serial.println("FAILED: " + fbdo.errorReason());
      }
    }

    if (abs(umidadeTratado - umidadeTratado_antiga) > 2) {
      if (Firebase.RTDB.setInt(&fbdo, "/umidadeSolo", (int)umidadeTratado)) {

        umidadeTratado_antiga = umidadeTratado;
      } else {
        Serial.println("FAILED: " + fbdo.errorReason());
      }
    }

    if (abs(luminosidade - luminosidade_antiga) > 0) {
      if (Firebase.RTDB.setInt(&fbdo, "/luminosidade", (int)luminosidade)) {
        luminosidade_antiga = luminosidade;
      } else {
        Serial.println("FAILED: " + fbdo.errorReason());
      }
    }
    ////////////////////////////////////////////////////////////////////////

    //Funções para pegar dados
    if(!Firebase.RTDB.getString(&fbdo, "config/horaInicio", &horaInicio))
    {
        Serial.println(fbdo.errorReason().c_str());
    }

    //-------------------------------------------------------

    if(!Firebase.RTDB.getString(&fbdo, "config/horaFim", &horaFim))
    {
        Serial.println(fbdo.errorReason().c_str());

    }

    //-------------------------------------------------------

    if(!Firebase.RTDB.getInt(&fbdo, "config/umidadeMinima", &umidadeMinima))
    {
        Serial.println(fbdo.errorReason().c_str());
        umidadeMinima = umidadeMinimaAux;
    }

    //-------------------------------------------------------

    if(!Firebase.RTDB.getInt(&fbdo, "config/suspenderLampada", &suspenderLampada))
    {
        Serial.println(fbdo.errorReason().c_str());
        suspenderLampada = suspenderLampadaAux;
    } 

    //-------------------------------------------------------

    if(!Firebase.RTDB.getInt(&fbdo, "config/suspenderIrrigacao", &suspenderIrrigacao))
    {
        Serial.println(fbdo.errorReason().c_str());
        suspenderIrrigacao = suspenderIrrigacaoAux;
    }

    if(!Firebase.RTDB.getInt(&fbdo, "config/travaIrrigacao", &habilitarTravaIrrigacao))
    {
        Serial.println(fbdo.errorReason().c_str());
        habilitarTravaIrrigacao = habilitarTravaIrrigacaoAux;
    }
  }

  verificarHorario();

  if (Firebase.ready() && (millis() - sendDataPrevMillisHist > 1800000 || sendDataPrevMillisHist == 0)) {
    sendDataPrevMillisHist = millis();

    // Cria um caminho exclusivo para a medição
    String path = "/historico/";
    dataTratado = (dia < 10 ? "0" + String(dia) : String(dia)) + "/" + (mes < 10 ? "0" + String(mes) : String(mes)) + "/" + String(ano);
    horaTratado = (horaAtual < 10 ? "0" + String(horaAtual) : String(horaAtual)) + ":" + (minutoAtual < 10 ? "0" + String(minutoAtual) : String(minutoAtual));
    luminosidadeString = luminosidade;

    // Envia os dados ao Firebase
    FirebaseJson json;
    json.set("Umidade Solo", (int)umidadeTratado);
    json.set("Umidade Ar", (int)DHT11.humidity);
    json.set("Temperatura", (int)DHT11.temperature);
    json.set("Lâmpada", luminosidadeString);
    json.set("Horário", horaTratado);
    json.set("Data", dataTratado );

    if (Firebase.RTDB.pushJSON(&fbdo, path.c_str(), &json)) {
      Serial.println("Dados enviados com sucesso.");
    } else {
      Serial.println("Erro ao enviar dados: " + fbdo.errorReason());
    }
  }

  // Exibe o estado da luz no monitor serial
  Serial.print("Luminosidade: ");
  if (luminosidade == 1) {
    Serial.println("Luz detectada");
  } else {
    Serial.println("Escuro detectado");
  }

  Serial.print("Umidade do Solo(valor): ");
  Serial.println(umidade);

  Serial.print("Umidade minima: ");
  Serial.println(umidadeMinima);

  Serial.print("O valor da umidade do ar é: ");
  Serial.println((float)DHT11.humidity, 2);

  Serial.print("A temperatura do ar é: ");
  Serial.println((float)DHT11.temperature, 2);

  Serial.print("A umidade do solo: ");
  Serial.println(umidadeTratado);

  Serial.print("Trava pós irrigação: ");
  Serial.println(habilitarTravaIrrigacao);

  if(habilitarTravaIrrigacao == 0){
    travaIrrigacao = 0;
  }

  if (suspenderIrrigacao == 0) {
    if (travaIrrigacao == 0) { // Verifica se a trava está desativada
        if ((int)umidadeTratado < umidadeMinima) {
            digitalWrite(14, LOW);
            Serial.println("Estado da Torneira: Aberto");
            delay(10000); // Liga a irrigação por 10 segundos

            // Ativa a trava e registra o tempo
            if(habilitarTravaIrrigacao == 1){
              travaIrrigacao = 1;
              timerTravaIrrigacao = millis();
            }

            digitalWrite(14, HIGH);
        } else {
            digitalWrite(14, HIGH);
            Serial.println("Estado da Torneira: Fechado");
        }
    } else {
        if (millis() - timerTravaIrrigacao > 300000) { // 5 minutos em milissegundos
          travaIrrigacao = 0; // Libera a trava
          Serial.println("Trava pós irrigação liberada!");
          Serial.println("Estado da Torneira: Fechado");
        } else{
          Serial.println("Irrigação em espera devido à trava pós irrigação");
          digitalWrite(14, HIGH);
        }
    }

  } else {
      Serial.println("Irrigação em espera devido à suspensão.");
      digitalWrite(14, HIGH);
  }

  Serial.print("Acionamento da lâmpada: " + horaInicio);

  //Guarda as configurações caso a conexão seja interrompida
  umidadeMinimaAux = umidadeMinima;
  horaInicioAux = horaInicioTratado;
  minutoInicioAux = minutoInicioTratado;
  horaFimAux = horaFimTratado;
  minutoFimAux = minutoFimTratado;
  suspenderLampadaAux = suspenderLampada;
  suspenderIrrigacaoAux = suspenderIrrigacao;
  habilitarTravaIrrigacaoAux = habilitarTravaIrrigacao;

  Serial.println();
  Serial.println("+++++++++++++++++++++++++++++++++++++++++++++++");
  Serial.println();
  delay(5000);
}
