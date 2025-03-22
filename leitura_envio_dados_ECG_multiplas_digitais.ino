#include <Adafruit_Fingerprint.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <HTTPClient.h>

// Configuração do sensor biométrico (R307) e UART para o ESP32
HardwareSerial mySerial(1);  // Usando UART1 (GPIO16, GPIO17)
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

// Configuração do WiFi e HTTP para envio dos dados do ECG
const char* ssid = "CIT_Alunos";          // Substitua pelo seu SSID
const char* password = "alunos@2024";       // Substitua pela sua senha
String servidorURL = "http://172.22.68.154:5000/dados";  // URL do servidor

// Configuração do sensor ECG AD8232
int ecgPin = 34;  // Pino analógico conectado ao sinal do ECG
int valorECG = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);  // Aguarda estabilização da comunicação Serial

  // Inicializa o sensor biométrico
  mySerial.begin(57600, SERIAL_8N1, 16, 17);
  finger.begin(57600);
  if (!finger.verifyPassword()) {
    Serial.println("Erro: Sensor de biometria não detectado!");
    while (1);
  }
  
  // Inicializa a conexão WiFi
  WiFi.begin(ssid, password);
  Serial.println("Conectando ao WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Tentando conexão...");
  }
  Serial.println("Conectado ao WiFi!");
  Serial.print("IP_Address: ");
  Serial.println(WiFi.localIP());

  Serial.println("\nDigite 'cadastro' para cadastrar digitais ou 'exame' para iniciar a coleta do ECG.");
}

void loop() {
  if (Serial.available() > 0) {
    String comando = Serial.readStringUntil('\n');
    comando.trim();
    
    if (comando.equalsIgnoreCase("cadastro")) {
      modoCadastro();
    } 
    else if (comando.equalsIgnoreCase("exame")) {
      modoExame();
    } 
    else {
      Serial.println("Comando inválido. Digite 'cadastro' ou 'exame'.");
    }
  }
  // Pequeno delay para evitar laços muito rápidos
  delay(100);
}

void modoCadastro() {
  Serial.println("\n=== MODO CADASTRO ===");
  Serial.println("Digite o ID para cadastrar uma digital ou 'fim' para encerrar o cadastro.");
  
  while (true) {
    // Aguarda comando via serial
    while (Serial.available() == 0) {
      delay(100);
    }
    
    String entrada = Serial.readStringUntil('\n');
    entrada.trim();
    
    if (entrada.equalsIgnoreCase("fim")) {
      Serial.println("Encerrando modo cadastro.\n");
      break;
    }
    
    int id = entrada.toInt();
    if (id <= 0) {
      Serial.println("ID inválido. Por favor, digite um número maior que zero.");
      continue;
    }
    
    Serial.print("Iniciando cadastro para ID ");
    Serial.println(id);
    cadastrarDigital(id);
    Serial.println("Para cadastrar outra digital, informe outro ID ou digite 'fim' para encerrar.");
  }
}

void modoExame() {
  Serial.println("\n=== MODO EXAME ===");
  Serial.println("Por favor, mantenha o dedo pressionado durante a autenticação...");
  
  int resultado = verificarDigital();
  if (resultado > 0) {
    Serial.print("Usuário autenticado! ID: ");
    Serial.println(resultado);
    Serial.println("Iniciando sessão de coleta de ECG...");
    
    while (true) {
      // Leitura e envio do sinal do ECG
      valorECG = analogRead(ecgPin);
      Serial.print("Valor ECG: ");
      Serial.println(valorECG);
      
      // Cria payload JSON com os dados do ECG
      String jsonPayload = "{\"ecg\": " + String(valorECG) + "}";
      
      // Envia os dados para o servidor via HTTP POST
      if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(servidorURL);
        http.addHeader("Content-Type", "application/json");
        int httpResponseCode = http.POST(jsonPayload);
        if (httpResponseCode > 0) {
          Serial.println("Dados enviados com sucesso!");
        } else {
          Serial.println("Erro ao enviar dados: " + String(httpResponseCode));
        }
        http.end();
      } else {
        Serial.println("WiFi desconectado!");
      }
      
      // Aguarda um curto intervalo para estabilizar a leitura do ECG
      delay(1000);
      
      // Verifica continuamente se um dedo foi posicionado para encerrar a sessão
      int imageStatus = finger.getImage();
      if (imageStatus == FINGERPRINT_OK) {
        // Pequeno delay para garantir estabilidade na captura da imagem
        delay(200);
        int novaValidacao = verificarDigital();
        // Se a digital validada for a mesma que iniciou a sessão, encerra o exame
        if (novaValidacao == resultado) {
          Serial.println("Digital de encerramento detectada. Finalizando sessão de ECG...");
          // Aguarda a remoção do dedo para evitar múltiplos disparos
          while (finger.getImage() != FINGERPRINT_NOFINGER) {
            delay(100);
          }
          break;
        }
      }
    }
    Serial.println("Sessão de ECG finalizada.\n");
  } 
  else {
    Serial.println("Falha na autenticação! Digite 'exame' para tentar novamente.\n");
  }
}

// Função para cadastrar uma digital com o ID informado
void cadastrarDigital(uint8_t id) {
  Serial.println("Aguardando posicionamento do dedo para cadastro...");
  
  // Primeira leitura da digital
  while (finger.getImage() != FINGERPRINT_OK);
  Serial.println("Dedo detectado, processando a primeira leitura...");
  if (finger.image2Tz(1) != FINGERPRINT_OK) {
    Serial.println("Erro ao processar imagem (primeira leitura). Tente novamente.");
    return;
  }
  
  // Solicita que o dedo seja removido e reposicionado para a segunda leitura
  Serial.println("Remova o dedo e posicione novamente...");
  delay(2000);
  while (finger.getImage() != FINGERPRINT_NOFINGER);
  while (finger.getImage() != FINGERPRINT_OK);
  Serial.println("Segunda leitura capturada!");
  if (finger.image2Tz(2) != FINGERPRINT_OK) {
    Serial.println("Erro na segunda leitura. Tente novamente.");
    return;
  }
  
  // Cria o modelo da digital e o armazena no sensor
  if (finger.createModel() != FINGERPRINT_OK) {
    Serial.println("Erro ao gerar modelo. Cadastro não realizado.");
    return;
  }
  if (finger.storeModel(id) != FINGERPRINT_OK) {
    Serial.println("Erro ao salvar no banco de dados do sensor.");
    return;
  }
  
  Serial.println("Digital cadastrada com sucesso!");
}

// Função para verificar a digital do usuário
int verificarDigital() {
  int resultado = finger.getImage();
  if (resultado != FINGERPRINT_OK) return -1;
  
  resultado = finger.image2Tz();
  if (resultado != FINGERPRINT_OK) return -1;
  
  resultado = finger.fingerFastSearch();
  if (resultado == FINGERPRINT_OK) {
    return finger.fingerID;  // Retorna o ID do usuário autenticado
  }
  
  return -1;  // Autenticação falhou
}
