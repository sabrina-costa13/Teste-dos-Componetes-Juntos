//inclusao das bibliotecas
#include <Arduino.h>
#include <Adafruit_BMP280.h> //biblioteca BMP
#include <Wire.h> //biblioteca MPU
#include <TinyGPS++.h> //biblioteca GPS
#include <SoftwareSerial.h> //biblioteca GPS
#include <FS.h> //biblioteca SD
#include <SD.h> //biblioteca SD
#include <SPI.h> //biblioteca SD e LoRa
#include <LoRa.h> //biblioteca LoRa

//difinir pinos:
//BMP
#define BMP_SCK  (13) 
#define BMP_MISO (12)
#define BMP_MOSI (11)
#define BMP_CS   (10)
//SD
#define SCK 14
#define MISO 12
#define MOSI 13
#define CS 15
//LoRa
const int csPin = 5;      // Chip Select ( Slave Select do protocolo SPI ) do modulo Lora
const int resetPin = 2;    // Reset do modulo LoRa
const int irqPin = 4;      // Pino DI0
//GPS
int RXPin = 16;
int TXPin = 17;
// Endereco I2C do MPU6050
const int MPU = 0x68;

Adafruit_BMP280 bmp; // I2C
//Adafruit_BMP280 bmp(BMP_CS); // hardware SPI
//Adafruit_BMP280 bmp(BMP_CS, BMP_MOSI, BMP_MISO,  BMP_SCK);

// Variaveis para armazenar valores dos sensores MPU
int AcX, AcY, AcZ, Tmp, GyX, GyY, GyZ;

int GPSBaud = 9600;
// CRIANDO UM OBJETO PARA COMUNICAR COM A BIBLIOTECA
TinyGPSPlus gps;
// CRIANDO UMA PORTA SERIAL gpsSerial PARA CONVERSAR COM MÓDULO
SoftwareSerial gpsSerial(RXPin, TXPin);

String outgoing;           // outgoing message
byte localAddress = 0xBB; // Endereco deste dispositivo LoRa
byte msgCount = 0;         // Contador de mensagens enviadas
byte destination = 0xFF;  // Endereco do dispositivo para enviar a mensagem (0 xFF envia para todos devices )
long lastSendTime = 0; // TimeStamp da ultima mensagem enviada
int interval = 5000;   // Intervalo em ms no envio das mensagens ( inicial 5s)

void listDir(fs ::FS &fs, const char *dirname, uint8_t levels)
{
  Serial.printf(" Listing directory : %s\n", dirname);
  File root = fs.open(dirname);
  if (!root)
  {
    Serial.println(" Failed to open directory ");
    return;
  }
  if (!root.isDirectory())
  {
    Serial.println(" Not a directory ");
    return;
  }
  File file = root.openNextFile();
  while (file)
  {
    if (file.isDirectory())
    {
      Serial.print(" DIR : ");
      Serial.println(file.name());
      if (levels)
      {
        listDir(fs, file.name(), levels - 1);
      }
    }
    else
    {
      Serial.print(" FILE : ");
      Serial.print(file.name());
      Serial.print(" SIZE : ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}
void createDir(fs ::FS &fs, const char *path)
{
  Serial.printf(" Creating Dir : %s\n", path);
  if (fs.mkdir(path))
  {
    Serial.println(" Dir created ");
  }
  else
  {
    Serial.println(" mkdir failed ");
  }
}
void removeDir(fs ::FS &fs, const char *path)
{
  Serial.printf(" Removing Dir : %s\n", path);
  if (fs.rmdir(path))
  {
    Serial.println(" Dir removed ");
  }
  else
  {
    Serial.println(" rmdir failed ");
  }
}
void readFile(fs ::FS &fs, const char *path)
{
  Serial.printf(" Reading file : %s\n", path);
  File file = fs.open(path);
  if (!file)
  {
    Serial.println(" Failed to open file for reading ");
    return;
  }
  Serial.print(" Read from file : ");
  while (file.available())
  {
    Serial.write(file.read());
  }
  file.close();
}
void writeFile(fs ::FS &fs, const char *path, const char *message)
{
  Serial.printf(" Writing file : %s\n", path);
  File file = fs.open(path, FILE_WRITE);
  if (!file)
  {
    Serial.println(" Failed to open file for writing ");
    return;
  }
  if (file.print(message))
  {
    Serial.println(" File written ");
  }
  else
  {
    Serial.println(" Write failed ");
  }
  file.close();
}
void appendFile(fs ::FS &fs, const char *path, const char *message)
{
  Serial.printf(" Appending to file : %s\n", path);
  File file = fs.open(path, FILE_APPEND);
  if (!file)
  {
    Serial.println(" Failed to open file for appending ");
    return;
  }
  if (file.print(message))
  {
    Serial.println(" Message appended ");
  }
  else
  {
    Serial.println(" Append failed ");
  }
  file.close();
}
void renameFile(fs ::FS &fs, const char *path1, const char *path2)
{
  Serial.printf(" Renaming file %s to %s\n", path1, path2);
  if (fs.rename(path1, path2))
  {
    Serial.println(" File renamed ");
  }
  else
  {
    Serial.println(" Rename failed ");
  }
}
void deleteFile(fs ::FS &fs, const char *path)
{
  Serial.printf(" Deleting file : %s\n", path);
  if (fs.remove(path))
  {
    Serial.println(" File deleted ");
  }
  else
  {
    Serial.println(" Delete failed ");
  }
}
void testFileIO(fs ::FS &fs, const char *path)
{
  File file = fs.open(path);
  static uint8_t buf[512];
  size_t len = 0;
  uint32_t start = millis();
  uint32_t end = start;
  if (file)
  {
    len = file.size();
    size_t flen = len;
    start = millis();
    while (len)
    {
      size_t toRead = len;
      if (toRead > 512)
      {
        toRead = 512;
      }
      file.read(buf, toRead);
      len -= toRead;
    }
    end = millis() - start;
    Serial.printf("%u bytes read for %u ms\n", flen, end);
    file.close();
  }
  else
  {
    Serial.println(" Failed to open file for reading ");
  }
  file = fs.open(path, FILE_WRITE);
  if (!file)
  {
    Serial.println(" Failed to open file for writing ");
    return;
  }
  size_t i;
  start = millis();
  for (i = 0; i < 2048; i++)
  {
    file.write(buf, 512);
  }
  end = millis() - start;
  Serial.printf("%u bytes written for %u ms\n", 2048 * 512, end);
  file.close();
}

void setup() {
  Serial.begin(115200);
  
  //BMP
  Serial.println(F("BMP280 Forced Mode Test."));
  //if (!bmp.begin(BMP280_ADDRESS_ALT, BMP280_CHIPID)) {
  if (!bmp.begin(0x76)) {
    Serial.println(F("Could not find a valid BMP280 sensor, check wiring or "
                      "try a different address!"));
    while (1) delay(10);
  }
  bmp.setSampling(Adafruit_BMP280::MODE_FORCED,     /* Operating Mode. */
                  Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
                  Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
                  Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                  Adafruit_BMP280::STANDBY_MS_500); /* Standby time. */

  //MPU
  Wire.begin();
  Wire.beginTransmission(MPU);
  Wire.write(0x6B);
  // Inicializa o MPU -6050
  Wire.write(0);
  Wire.endTransmission(1);

  //GPS
  // INICIA A PORTA SERIAL DO SOFTWARE NO BAUD PADRÃO DO GPS, COMO DETERMINAMOS ACIMA:9600
  gpsSerial.begin(GPSBaud);

  //SD
   SPIClass spi = SPIClass(HSPI);
  spi.begin(SCK, MISO, MOSI, CS);
  if (!SD.begin(CS, spi, 80000000))
  {
    Serial.println(" Card Mount Failed ");
    return;
  }
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE)
  {
    Serial.println("No SD card attached ");
    return;
  }
  Serial.print("SD Card Type : ");
  if (cardType == CARD_MMC)
  {
    Serial.println(" MMC ");
  }
  else if (cardType == CARD_SD)
  {
    Serial.println(" SDSC ");
  }
  else if (cardType == CARD_SDHC)
  {
    Serial.println(" SDHC ");
  }
  else
  {
    Serial.println(" UNKNOWN ");
  }
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size : % lluMB \n", cardSize);
  listDir(SD, "/", 0);
  createDir(SD, "/ mydir ");
  listDir(SD, "/", 0);
  removeDir(SD, "/ mydir ");
  listDir(SD, "/", 2);
  writeFile(SD, "/ hello . txt ", " Hello ");
  appendFile(SD, "/ hello . txt ", " World !\n");
  readFile(SD, "/ hello . txt ");
  deleteFile(SD, "/foo .txt ");
  renameFile(SD, "/ hello . txt ", "/ foo . txt ");
  readFile(SD, "/ foo . txt ");
  testFileIO(SD, "/ test . txt ");
  Serial.printf(" Total space : % lluMB \n", SD.totalBytes() / (1024 * 1024));
  Serial.printf(" Used space : % lluMB \n", SD.usedBytes() / (1024 * 1024));

  //LoRa
   while (!Serial)
    ;
  Serial.println(" Comunicacao LoRa Duplex - Ping & Pong ");
  // override the default CS , reset , and IRQ pins ( optional )
  LoRa.setPins(csPin, resetPin, irqPin); // set CS , reset , IRQ pin
  // Inicializa o radio LoRa em 915 MHz e checa se esta ok!
  if (!LoRa.begin(915E6))
  {
    Serial.println(" Erro ao iniciar modulo LoRa . Verifique a coenxao dos seus pinos !! ");
    while (true);
  }
  Serial.println(" Modulo LoRa iniciado com sucesso !!! :) ");
}

void displayInfo() // FUNÇÃO RESPONSAVEL PELA LEITURA DOS DADOS
{
  if (gps.location.isValid()) // SE A LOCALIZAÇÃO DO SINAL ENCONTRADO É VÁLIDA, ENTÃO
  {
    Serial.print("Latitude: ");
    Serial.println(gps.location.lat(), 6); // IMPRIME NA SERIAL O VALOR DA LATIDUE LIDA
    Serial.print("Longitude: ");
    Serial.println(gps.location.lng(), 6); // IMPRIME NA SERIAL O VALOR DA LONGITUDE LIDA
  }
  else
  {
    Serial.println("Não detectamos a localização"); // SE NÃO HOUVER NENHUMA LEITURA, IMPRIME A MENSAGEM DE ERRO NA SERIAL
  }

  Serial.print("Data: ");
  if (gps.date.isValid()) // IMPRIME A DATA NA SERIAL
  {
    Serial.print(gps.date.day()); // LEITURA DO DIA
    Serial.print("/");
    Serial.print(gps.date.month()); // LEITURA DO MêS
    Serial.print("/");
    Serial.println(gps.date.year()); // LEITURA DO ANO
  }
  else
  {
    Serial.println("Erro"); // SE NÃO HOUVER NENHUMA LEITURA, IMPRIME A MENSAGEM DE ERRO NA SERIAL
  }

  Serial.print("Time: "); // LEITURA DA HORA PARA SER IMPRESSA NA SERIAL
  if (gps.time.isValid())
  {
    if (gps.time.hour() < 10)
      Serial.print(F("0"));
    Serial.print(gps.time.hour() - 3); // AJUSTA O FUSO HORARIO PARA NOSSA REGIAO (FUSO DE SP 03:00, POR ISSO O -3 NO CÓDIGO) E IMPRIME NA SERIAL
    Serial.print(":");
    if (gps.time.minute() < 10)
      Serial.print(F("0"));
    Serial.print(gps.time.minute()); // IMPRIME A INFORMAÇÃO DOS MINUTOS NA SERIAL
    Serial.print(":");
    if (gps.time.second() < 10)
      Serial.print(F("0"));
    Serial.print(gps.time.second()); // IMPRIME A INFORMAÇÃO DOS SEGUNDOS NA SERIAL
  }
  else
  {
    Serial.println("Não detectamos o horário atual"); // SE NÃO HOUVER NENHUMA LEITURA, IMPRIME A MENSAGEM DE ERRO NA SERIAL
  }

  Serial.println();
  Serial.println();
  delay(1000);
}

// Funcao que envia uma mensagem LoRa
void sendMessage(String outgoing)
{
  LoRa.beginPacket();            // Inicia o pacote da mensagem
  LoRa.write(destination);       // Adiciona o endereco de destino
  LoRa.write(localAddress);      // Adiciona o endereco do remetente
  LoRa.write(msgCount);          // Contador da mensagem
  LoRa.write(outgoing.length()); // Tamanho da mensagem em bytes
  LoRa.print(outgoing);          // Vetor da mensagem
  LoRa.endPacket();              // Finaliza o pacote e envia
  msgCount++;                    // Contador do numero de mensagnes enviadas
}
// Funcao para receber mensagem
void onReceive(int packetSize)
{
  if (packetSize == 0)
    return; // Se nenhuma mesnagem foi recebida , retorna nada
  // Leu um pacote , vamos decodificar ?
  int recipient = LoRa.read();       // Endereco de quem ta recebendo
  byte sender = LoRa.read();         // Endereco do remetente
  byte incomingMsgId = LoRa.read();  // Mensagem
  byte incomingLength = LoRa.read(); // Tamanho da mensagem
  String incoming = "";
  while (LoRa.available())
  {
    incoming += (char)LoRa.read();
  }
  if (incomingLength != incoming.length())
  {
    // check length for error
    Serial.println(" erro !: o tamanho da mensagem nao condiz com o conteudo !");
    return;
  }
  // if the recipient isn ’t this device or broadcast ,
  if (recipient != localAddress && recipient != 0xFF)
  {
    Serial.println(" This message is not for me.");
    return; // skip rest of function
  }
  // Caso a mensagem seja para este dispositivo , imprime os detalhes
  Serial.println(" Recebido do dispositivo : 0x" + String(sender, HEX));
  Serial.println(" Enviado para : 0x" + String(recipient, HEX));
  Serial.println("ID da mensagem : " + String(incomingMsgId));
  Serial.println(" Tamanho da mensagem : " + String(incomingLength));
  Serial.println(" Mensagem : " + incoming);
  Serial.println(" RSSI : " + String(LoRa.packetRssi()));
  Serial.println(" Snr : " + String(LoRa.packetSnr()));
  Serial.println();
}

void loop() {
 //BMP
   if (bmp.takeForcedMeasurement()) {
    // can now print out the new measurements
    Serial.print(F("Temperature = "));
    Serial.print(bmp.readTemperature());
    Serial.println(" *C");

    Serial.print(F("Pressure = "));
    Serial.print(bmp.readPressure());
    Serial.println(" Pa");

    Serial.print(F("Approx altitude = "));
    Serial.print(bmp.readAltitude(1013.25)); /* Adjusted to local forecast! */
    Serial.println(" m");

    Serial.println();
    delay(2000);
  } else {
    Serial.println("Forced measurement failed!");
  }

  //MPU
   Wire.beginTransmission(MPU);
  Wire.write(0x3B); // starting with register 0 x3B ( ACCEL_XOUT_H )
  Wire.endTransmission(0);
  // Solicita os dados do sensor
  Wire.requestFrom(MPU, 14, 1);
  // Armazena o valor dos sensores nas variaveis correspondentes
  AcX = Wire.read() << 8 | Wire.read(); // 0 x3B ( ACCEL_XOUT_H ) & 0 x3C ( ACCEL_XOUT_L )
  AcY = Wire.read() << 8 | Wire.read(); // 0 x3D ( ACCEL_YOUT_H ) & 0 x3E ( ACCEL_YOUT_L )
  AcZ = Wire.read() << 8 | Wire.read(); // 0 x3F ( ACCEL_ZOUT_H ) & 0 x40 ( ACCEL_ZOUT_L )
  Tmp = Wire.read() << 8 | Wire.read(); // 0 x41 ( TEMP_OUT_H ) & 0x42 ( TEMP_OUT_L )
  GyX = Wire.read() << 8 | Wire.read(); // 0 x43 ( GYRO_XOUT_H ) & 0 x44 ( GYRO_XOUT_L )
  GyY = Wire.read() << 8 | Wire.read(); // 0 x45 ( GYRO_YOUT_H ) & 0 x46 ( GYRO_YOUT_L )
  GyZ = Wire.read() << 8 | Wire.read(); // 0 x47 ( GYRO_ZOUT_H ) & 0 x48 ( GYRO_ZOUT_L )
  Serial.print(" AcX = ");
  Serial.print(AcX);
  Serial.print(" | AcY = ");
  Serial.print(AcY);
  Serial.print(" | AcZ = ");
  Serial.print(AcZ);
  Serial.print(" | Tmp = ");
  Serial.print(Tmp / 340.00 + 36.53);
  Serial.print(" | GyX = ");
  Serial.print(GyX);
  Serial.print(" | GyY = ");
  Serial.print(GyY);
  Serial.print(" | GyZ = ");
  Serial.println(GyZ);
  // Aguarda 300 ms e reinicia o processo
  delay(300);

  //GPS
    while (gpsSerial.available() > 0)
    if (gps.encode(gpsSerial.read()))
      displayInfo();
  // SE EM 5 SEGUNDOS NÃO FOR DETECTADA NENHUMA NOVA LEITURA PELO MÓDULO,SERÁ MOSTRADO ESTA MENSGEM DE ERRO.
  if (millis() > 5000 && gps.charsProcessed() < 10)
  {
    Serial.println("Sinal GPS não detectado");
    while (true)
      ;
  }

  //LoRa
    // verifica se temos o intervalo de tempo para enviar uma mensagem
  if (millis() - lastSendTime > interval)
  {
    String mensagem = " Ola mundo ! :O "; // Definicao da mensagem
    sendMessage(mensagem);
    Serial.println(" Enviando " + mensagem);
    lastSendTime = millis(); // Timestamp da ultima mensagem
  }
  // parse for a packet , and call onReceive with the result :
  onReceive(LoRa.parsePacket());

}
