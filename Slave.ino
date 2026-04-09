#include <WiFi.h>//libreria modulo WiFi
#include <esp_now.h>//libreria protocollo ESP-NOW
#include <SPI.h>//libreria per comunicazione tra esp e schermo
#include <TFT_eSPI.h>//libreria per controllare schermo
#include "esp_wifi.h"//libreria moduli WiFi avanzati

//creo un oggetto di tipo display
TFT_eSPI tft = TFT_eSPI();

//mi definisco le dimensioni dello schermo
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

// Area grafico
#define GRAPH_X 40
#define GRAPH_Y 40
#define GRAPH_WIDTH 240
#define GRAPH_HEIGHT 140

#define MAX_VELOCITA 200

//memorizzo i dati del grafico
int storico[GRAPH_WIDTH];

//mi inizializzo la struct che contiene i dati

typedef struct {
  int32_t velocita;
  int32_t distanza;
} Dati;

Dati datiRicevuti;

//funzione chiamata quando arriva un dato
void OnDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {

  if (len == sizeof(Dati)) {
    memcpy(&datiRicevuti, incomingData, sizeof(datiRicevuti));
  }

  Serial.print("Ricevuto -> Velocita: ");
  Serial.print(datiRicevuti.velocita);
  Serial.print(" Distanza: ");
  Serial.println(datiRicevuti.distanza);
}

//Funzione Disegno Griglia
void disegnaGriglia() {
  //creo il rettangolo
  tft.drawRect(GRAPH_X, GRAPH_Y, GRAPH_WIDTH, GRAPH_HEIGHT, TFT_BLACK);
  //imposto 6 sezioni

  for (int i = 0; i <= 8; i++) {
    int valore = i * (MAX_VELOCITA / 8);  // divide la scala in 6 parti
    //converto in valore le coordinate in cui va disegnata la riga e la disegno
    int y = map(valore, 0, MAX_VELOCITA, GRAPH_Y + GRAPH_HEIGHT, GRAPH_Y);
    tft.drawLine(GRAPH_X, y, GRAPH_X + GRAPH_WIDTH, y, TFT_LIGHTGREY);

    tft.setTextColor(TFT_BLACK, TFT_WHITE);
    //mi scrivo il valore a sinistra della riga
    tft.drawRightString(String(valore), GRAPH_X - 5, y - 4, 1);
  }
  //stessa cosa per le linee verticali
  for (int i = 0; i <= 8; i++) {
    int x = GRAPH_X + (i * GRAPH_WIDTH / 8);
    tft.drawLine(x, GRAPH_Y, x, GRAPH_Y + GRAPH_HEIGHT, TFT_LIGHTGREY);
  }
}


void disegnaGrafico() {
  //ripulisco l area del grafico
  tft.fillRect(GRAPH_X + 1, GRAPH_Y + 1, GRAPH_WIDTH - 2, GRAPH_HEIGHT - 2, TFT_WHITE);

  //ridisegno la griglia
  disegnaGriglia();
  //scorro i dati mappando i punti come coordinate per poi disegnare tra questi una retta
  for (int i = 1; i < GRAPH_WIDTH; i++) {
    int y1 = map(storico[i - 1], 0, MAX_VELOCITA, GRAPH_Y + GRAPH_HEIGHT, GRAPH_Y);
    int y2 = map(storico[i], 0, MAX_VELOCITA, GRAPH_Y + GRAPH_HEIGHT, GRAPH_Y);

    int x1 = GRAPH_X + i - 1;
    int x2 = GRAPH_X + i;

    tft.drawLine(x1, y1, x2, y2, TFT_RED);
    tft.drawLine(x1, y1 + 1, x2, y2 + 1, TFT_RED);
  }
}

void setup() {
  Serial.begin(115200);

  //Inizializzo il display
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_WHITE);

  //Imposto la modalita di WiFi e imposto il canale per ESP_NOW
  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
  //avvio il protocollo e verifico che si sia avviato correttamente
  if (esp_now_init() != ESP_OK) {
    Serial.println("Errore inizializzazione ESP-NOW");
    return;
  }
  //mi salvo i dati che mi arrivano
  esp_now_register_recv_cb(OnDataRecv);

  // inizializza storico
  for (int i = 0; i < GRAPH_WIDTH; i++) {
    storico[i] = 0;
  }
  //disegno laprima griglia
  disegnaGriglia();

  //configuro le parole velocita e testo
  String testo = "Velocita";

  int x = GRAPH_X - 30;
  int y = GRAPH_Y + 30;

  for (int i = 0; i < testo.length(); i++) {
    tft.drawChar(testo[i], x, y + i * 8, 1);
  }
  tft.drawCentreString("Tempo",GRAPH_X + GRAPH_WIDTH / 2,GRAPH_Y + GRAPH_HEIGHT + 10,1);
}

void loop() {
  // shift a sinistra del grafico
  for (int i = 0; i < GRAPH_WIDTH - 1; i++) {
    storico[i] = storico[i + 1];
  }

  // usa valore ricevuto
  storico[GRAPH_WIDTH - 1] = datiRicevuti.velocita;

  // aggiorna grafico
  disegnaGrafico();

  // testo sotto
  tft.fillRect(0, 200, SCREEN_WIDTH, 40, TFT_WHITE);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.drawCentreString("Distanza Rilevata : " + String(datiRicevuti.distanza) + " cm", SCREEN_WIDTH / 2, 205, 2);

  delay(100);
}