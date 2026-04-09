#include <WiFi.h>//Libreria modulo WIFi
#include <esp_now.h>//Libreria protocollo ESP_NOW
#include <NewPing.h> //Libreria sensore ad ultrasuoni

// PIN SENSORE ULTRASUONI
#define trigPin 12
#define echoPin 14

//ho bisogno del MAC del destinatario
uint8_t receiverMAC[]={0x5C, 0x01, 0x3B, 0x8A, 0x17, 0xB4};
//questa funzione verifica che il messaggio è stato spedito con successo
void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  Serial.print("Invio: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}
//mi definisco la struct che contiene le variabili da inviare
typedef struct {
  int32_t velocita;
  int32_t distanza;
} Dati;

Dati dati;

// --- PARAMETRI PID ---
double Kp = 2.5;
double Ki = 0.1;
double Kd = 0.05;

// --- VARIABILI ---
double distanza_desiderata = 10.0;
double distanza_misurata = 0.0;
double errore_proporzionale = 0.0; // distanza_desiderata - distanza_misurata
double errore_integrale = 0.0; // somma dell'errore nel tempo
double errore_derivativo = 0.0;
double ultimo_errore = 0.0;
const unsigned long Tempo_di_campionamento = 500;
int campionamento_corretto = 0; // 0 -> il campionamento è corretto e si puo procedere, 1 il campionamento è errato, si passa al prossimo passo temporale
double valore_di_saturazione = 50.0;
double uscitaPWM = 0.0; 
int input_utente = 200; // valore di velocità massima di cruise control impostato dall'utente

// --- CONTROLLI TEMPORALI ---
unsigned long lastTime = 0;
double dt = 0.0;


//SETUP PRECODE
NewPing sonar(trigPin,echoPin,500);


// --- FUNZIONI AUSILIARIE ---
double calcoloPID(){
  errore_proporzionale = distanza_misurata -distanza_desiderata ;
  // CALCOLO POSSIBILE ERRORE INTEGRALE
  double errore_integrale_temp = errore_integrale + errore_proporzionale * dt;

  errore_derivativo = (errore_proporzionale-ultimo_errore)/dt;
  ultimo_errore = errore_proporzionale;

  double uscita_non_saturata = Kp*errore_proporzionale + Ki*errore_integrale_temp + Kd*errore_derivativo;

  // APPLICO SATURAZIONE
  double uscita_saturata = constrain(uscita_non_saturata,0,255);

  // APPLICO ANDIWINDUP
  if(uscita_saturata == uscita_non_saturata){
      errore_integrale = errore_integrale_temp;
  }
  if(errore_integrale >= valore_di_saturazione){
    errore_integrale -= errore_proporzionale*dt;
  }
  if(distanza_misurata == 0){
    uscita_saturata = input_utente;
  }
  
  return uscita_saturata;
}

void misura_distanze_e_tempi(){
  unsigned long istante_attuale = millis();
  if(istante_attuale - lastTime < Tempo_di_campionamento){
    campionamento_corretto = 1;
    return;
  }
  campionamento_corretto = 0;
  dt = (istante_attuale - lastTime)/1000.0;
  lastTime = istante_attuale;

  distanza_misurata = sonar.ping_cm();
}


void setup() {
  Serial.begin(115200);
  //imposto il modulo wifi in station in modo da prepararlo per la comunicazione ESP_NOW
  WiFi.mode(WIFI_STA);
  //sconnetto l esp da tutte le connessioni precedenti
  WiFi.disconnect();

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  lastTime = millis();

  if(esp_now_init() != ESP_OK){
    Serial.println("Connessione ESP-NOW fallita");
    return;
  }
  esp_now_register_send_cb(OnDataSent);
  //struct contentente le info del receiver
  esp_now_peer_info_t peerInfo={};
  //inserisco il mac del destinatario nella struct
  memcpy(peerInfo.peer_addr,receiverMAC,6);
  //scelgo un canale per la comunicazione
  peerInfo.channel=1;
  //disabilito la crittografia
  peerInfo.encrypt=false;
  //registro il receiver
  if(esp_now_add_peer(&peerInfo)!=ESP_OK){
    Serial.println("Collegamento fallito");
    return;
  }
}

void loop() {

  misura_distanze_e_tempi();
  if(campionamento_corretto == 1){
    return;
  }
  uscitaPWM = calcoloPID();
  int PWM_EFFETTIVI = constrain((int)uscitaPWM,0,input_utente);

  Serial.print("PWM = ");
  Serial.println(PWM_EFFETTIVI);
  Serial.print("Errore integrale = ");
  Serial.println(errore_integrale);
  Serial.println(distanza_misurata);

  dati.velocita = PWM_EFFETTIVI;
  dati.distanza = distanza_misurata;

  //invio il dato con parametri (destinatario,indirizzo variabile, dimensione)
  esp_now_send(receiverMAC, (uint8_t *) &dati, sizeof(dati));

  Serial.print("Inviato -> velocita: ");
  Serial.print(dati.velocita);
  Serial.print(" distanza: ");
  Serial.println(dati.distanza);

  delay(500);
}