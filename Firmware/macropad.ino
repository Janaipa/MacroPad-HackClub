/*
 ============================================================================
 XIAO RP2040 MACROPAD con OLED e ENCODER
 Autore: [Janaipa]
 
 Descrizione: 
 Macropad 4x3 con encoder rotativo e display OLED 128x32.
 Controlla volume, media e shortcuts tramite emulazione HID.
 ============================================================================
*/

// ================= INCLUDES LIBRERIE =================
#include <Keyboard.h>      // Per emulazione tastiera
#include <Keypad.h>        // Per gestione matrice tasti
#include <Encoder.h>       // Per encoder rotativo
#include <Wire.h>          // Per comunicazione I2C (OLED)
#include <Adafruit_GFX.h>  // Per grafica OLED
#include <Adafruit_SSD1306.h> // Per driver OLED SSD1306

// ================= CONFIGURAZIONE OLED =================
#define SCREEN_WIDTH 128    // Larghezza display in pixel
#define SCREEN_HEIGHT 32    // Altezza display in pixel
#define OLED_RESET -1       // Pin reset (-1 se non usato)
#define SCREEN_ADDRESS 0x3C // Indirizzo I2C OLED (0x3C o 0x3D)

// Crea oggetto display OLED
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ================= CONFIGURAZIONE ENCODER =================
// Encoder collegato ai pin GP3 (A) e GP4 (B)
Encoder myEncoder(3, 4);

// Variabili per gestione encoder
long oldPosition = 0;     // Posizione precedente encoder
int volume = 50;          // Livello volume corrente (0-100%)
bool mute = false;        // Stato mute (true = mutato)

// ================= CONFIGURAZIONE MATRICE TASTI =================
// Definizione dimensioni matrice: 4 righe x 3 colonne
const byte ROWS = 4;  // Numero righe
const byte COLS = 3;  // Numero colonne

// Mappatura caratteri per ogni tasto della matrice
// Ogni carattere identifica univocamente un tasto fisico
char hexaKeys[ROWS][COLS] = {
  {'A', 'B', 'C'},  // Riga 1: Tasti 1, 2, 3
  {'D', 'E', 'F'},  // Riga 2: Tasti 4, 5, 6
  {'G', 'H', 'I'},  // Riga 3: Tasti 7, 8, 9
  {'J', 'K', 'L'}   // Riga 4: Tasti 10, 11, 12
};

// Pin GPIO collegati alle RIGHE della matrice (GP11, GP10, GP5, GP6)
byte rowPins[ROWS] = {11, 10, 5, 6};

// Pin GPIO collegati alle COLONNE della matrice (GP7, GP8, GP9)
byte colPins[COLS] = {7, 8, 9};

// Crea oggetto keypad con la configurazione sopra
Keypad customKeypad = Keypad(
  makeKeymap(hexaKeys),  // Mappa caratteri
  rowPins,               // Pin righe
  colPins,               // Pin colonne
  ROWS,                  // Numero righe
  COLS                   // Numero colonne
);

// ================= VARIABILI GLOBALI =================
unsigned long lastOLEDUpdate = 0;  // Timestamp ultimo aggiornamento OLED
bool oledInitialized = false;      // Flag inizializzazione OLED riuscita
const unsigned long OLED_UPDATE_INTERVAL = 500; // Aggiorna OLED ogni 500ms

// ================= FUNZIONI OLED =================

/**
 * Inizializza il display OLED via I2C
 * Configura i pin I2C della XIAO RP2040
 */
void initOLED() {
  // Configura pin I2C della XIAO: GP1 = SDA, GP2 = SCL
  Wire.setSDA(1);  // Imposta GP1 come SDA
  Wire.setSCL(2);  // Imposta GP2 come SCL
  Wire.begin();    // Inizia comunicazione I2C
  
  // Prova a inizializzare display OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("ERRORE: Impossibile inizializzare OLED SSD1306!"));
    Serial.println(F("Controlla:"));
    Serial.println(F("1. Cablaggio I2C (SDA=GP1, SCL=GP2)"));
    Serial.println(F("2. Alimentazione 3.3V"));
    Serial.println(F("3. Indirizzo I2C (prova 0x3D)"));
    return;  // Esci se inizializzazione fallisce
  }
  
  // Display inizializzato con successo
  display.clearDisplay();           // Pulisce schermo
  display.setTextSize(1);           // Imposta dimensione testo
  display.setTextColor(SSD1306_WHITE); // Testo bianco
  display.setCursor(0, 0);          // Posizione cursore in alto a sinistra
  
  // Mostra messaggio di avvio
  display.println(F("XIAO Macropad"));
  display.println(F("Inizializzazione..."));
  display.display();                // Mostra su schermo
  
  delay(2000);  // Mostra messaggio per 2 secondi
  oledInitialized = true;  // Imposta flag a true
}

/**
 * Aggiorna il contenuto del display OLED
 * Mostra volume, stato mute e posizione encoder
 */
void updateOLED() {
  // Controlla se OLED è inizializzato e se è passato abbastanza tempo
  if (!oledInitialized || (millis() - lastOLEDUpdate < OLED_UPDATE_INTERVAL)) {
    return;  // Esci se condizioni non soddisfatte
  }
  
  display.clearDisplay();  // Pulisce il buffer dello schermo
  display.setCursor(0, 0); // Posizione iniziale testo
  
  // ===== SEZIONE 1: INFORMAZIONI VOLUME =====
  display.print(F("Volume: "));  // Testo fisso
  display.print(volume);         // Valore volume corrente
  display.print(F("%"));         // Simbolo percentuale
  
  // Mostra [MUTE] se il volume è mutato
  if (mute) {
    display.print(F(" [MUTE]"));
  }
  
  // ===== SEZIONE 2: BARRA DEL VOLUME =====
  // Disegna rettangolo vuoto per barra volume
  display.drawRect(0, 16, 128, 8, SSD1306_WHITE);
  
  // Calcola larghezza riempimento in base al volume (0-100% → 0-126 pixel)
  int barWidth = map(volume, 0, 100, 0, 126);
  
  // Riempie la barra in base al volume corrente
  display.fillRect(1, 17, barWidth, 6, SSD1306_WHITE);
  
  // ===== SEZIONE 3: STATO ENCODER =====
  display.setCursor(0, 24);  // Posizione terza riga
  
  // Mostra direzione ultima rotazione encoder
  display.print(F("Encoder: "));
  if (oldPosition > 0) {
    display.print(F("+ (UP)"));    // Rotazione in senso orario
  } else if (oldPosition < 0) {
    display.print(F("- (DOWN)"));  // Rotazione anti-oraria
  } else {
    display.print(F("OK"));        // Nessuna rotazione recente
  }
  
  display.display();        // Invia buffer allo schermo
  lastOLEDUpdate = millis(); // Aggiorna timestamp
}

// ================= FUNZIONI GESTIONE VOLUME =================

/**
 * Aumenta il volume del 5%
 * Disattiva mute se attivo
 */
void increaseVolume() {
  if (volume < 100) {
    volume += 5;  // Incrementa di 5%
  }
  
  // Assicura che volume non superi 100%
  if (volume > 100) {
    volume = 100;
  }
  
  mute = false;  // Disattiva mute quando si regola volume
  
  // Invia comando volume UP al computer
  // NOTA: Sostituire con comandi HID appropriati se disponibili
  Keyboard.press(KEY_F3);  // Tasto F3 per volume UP
  Keyboard.releaseAll();
  
  Serial.print("Volume aumentato: ");
  Serial.println(volume);
}

/**
 * Diminuisce il volume del 5%
 * Disattiva mute se attivo
 */
void decreaseVolume() {
  if (volume > 0) {
    volume -= 5;  // Decrementa di 5%
  }
  
  // Assicura che volume non scenda sotto 0%
  if (volume < 0) {
    volume = 0;
  }
  
  mute = false;  // Disattiva mute quando si regola volume
  
  // Invia comando volume DOWN al computer
  Keyboard.press(KEY_F2);  // Tasto F2 per volume DOWN
  Keyboard.releaseAll();
  
  Serial.print("Volume diminuito: ");
  Serial.println(volume);
}

/**
 * Attiva/disattiva il mute
 * Inverte lo stato mute corrente
 */
void toggleMute() {
  mute = !mute;  // Inverte stato mute
  
  // Invia comando MUTE al computer
  Keyboard.press(KEY_F1);  // Tasto F1 per mute
  Keyboard.releaseAll();
  
  // Feedback su Serial Monitor
  if (mute) {
    Serial.println("Mute ATTIVATO");
  } else {
    Serial.println("Mute DISATTIVATO");
  }
}

// ================= FUNZIONI GESTIONE TASTI =================

/**
 * Gestisce la pressione di un tasto
 * @param key Carattere identificativo del tasto premuto
 */
void handleKeyPress(char key) {
  // Stampa su Serial Monitor per debug
  Serial.print("Tasto premuto: ");
  Serial.println(key);
  
  // Gestione in base al tasto premuto
  switch(key) {
    // ===== RIGA 1 =====
    case 'A':  // Tasto 1: Pulsante Encoder (Mute)
      Serial.println("Funzione: Toggle Mute");
      toggleMute();
      break;
      
    case 'B':  // Tasto 2: Ctrl + C (Copia)
      Serial.println("Funzione: Ctrl + C (Copia)");
      Keyboard.press(KEY_LEFT_CTRL);
      Keyboard.press('c');
      delay(10);  // Piccolo delay per stabilità
      Keyboard.releaseAll();
      break;
      
    case 'C':  // Tasto 3: Ctrl + V (Incolla)
      Serial.println("Funzione: Ctrl + V (Incolla)");
      Keyboard.press(KEY_LEFT_CTRL);
      Keyboard.press('v');
      delay(10);
      Keyboard.releaseAll();
      break;
    
    // ===== RIGA 2 =====
    case 'D':  // Tasto 4: Play/Pause Media
      Serial.println("Funzione: Play/Pause Media");
      Keyboard.write(KEY_MEDIA_PLAY_PAUSE);
      break;
      
    case 'E':  // Tasto 5: Prossima Traccia
      Serial.println("Funzione: Prossima Traccia");
      Keyboard.write(KEY_MEDIA_NEXT_TRACK);
      break;
      
    case 'F':  // Tasto 6: Traccia Precedente
      Serial.println("Funzione: Traccia Precedente");
      Keyboard.write(KEY_MEDIA_PREVIOUS_TRACK);
      break;
    
    // ===== RIGA 3 =====
    case 'G':  // Tasto 7: Lettera W
      Serial.println("Funzione: Lettera W");
      Keyboard.write('w');
      break;
      
    case 'H':  // Tasto 8: Lettera R
      Serial.println("Funzione: Lettera R");
      Keyboard.write('r');
      break;
      
    case 'I':  // Tasto 9: Shift + V (V maiuscola)
      Serial.println("Funzione: Shift + V");
      Keyboard.press(KEY_LEFT_SHIFT);
      Keyboard.write('v');
      Keyboard.release(KEY_LEFT_SHIFT);
      break;
    
    // ===== RIGA 4 =====
    case 'J':  // Tasto 10: Alt + Tab (Cambio finestra)
      Serial.println("Funzione: Alt + Tab");
      Keyboard.press(KEY_LEFT_ALT);
      Keyboard.press(KEY_TAB);
      delay(10);
      Keyboard.releaseAll();
      break;
      
    case 'K':  // Tasto 11: Ctrl + Alt + Esc
      Serial.println("Funzione: Ctrl + Alt + Esc");
      Keyboard.press(KEY_LEFT_CTRL);
      Keyboard.press(KEY_LEFT_ALT);
      Keyboard.press(KEY_ESC);
      delay(10);
      Keyboard.releaseAll();
      break;
      
    case 'L':  // Tasto 12: Ctrl + D
      Serial.println("Funzione: Ctrl + D");
      Keyboard.press(KEY_LEFT_CTRL);
      Keyboard.press('d');
      delay(10);
      Keyboard.releaseAll();
      break;
    
    // Default per sicurezza
    default:
      Serial.println("Tasto non configurato!");
      break;
  }
  
  // Aggiorna OLED dopo pressione tasto
  updateOLED();
}

// ================= SETUP (ESEGUITO UNA VOLTA) =================

/**
 * Funzione setup: eseguita una volta all'avvio
 */
void setup() {
  // Inizializza comunicazione seriale per debug
  Serial.begin(115200);
  Serial.println("========================================");
  Serial.println("XIAO RP2040 MACROPAD - Inizializzazione");
  Serial.println("========================================");
  
  // Inizializza emulazione tastiera HID
  Keyboard.begin();
  Serial.println("Emulazione tastiera: ATTIVA");
  
  // Inizializza display OLED
  initOLED();
  if (oledInitialized) {
    Serial.println("Display OLED: INIZIALIZZATO");
  } else {
    Serial.println("Display OLED: NON INIZIALIZZATO");
  }
  
  // Messaggio iniziale su OLED
  if (oledInitialized) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(F("Macropad Pronto"));
    display.println(F("Volume: 50%"));
    display.display();
    delay(1000);
  }
  
  Serial.println("Sistema pronto. Monitora i tasti premuti.");
  Serial.println("========================================");
}

// ================= LOOP (ESEGUITO IN CONTINUAZIONE) =================

/**
 * Funzione loop: eseguita continuamente dopo il setup
 */
void loop() {
  // ===== 1. GESTIONE ENCODER ROTATIVO =====
  // Legge posizione encoder (divisa per 4 per ridurre sensibilità)
  long newPosition = myEncoder.read() / 4;
  
  // Controlla se posizione è cambiata
  if (newPosition != oldPosition) {
    // Determinazione direzione rotazione
    if (newPosition > oldPosition) {
      // Rotazione in senso ORARIO (Aumenta volume)
      increaseVolume();
    } else {
      // Rotazione in senso ANTI-ORARIO (Diminuisce volume)
      decreaseVolume();
    }
    
    // Aggiorna posizione precedente
    oldPosition = newPosition;
    
    // Aggiorna display OLED con nuovo volume
    updateOLED();
  }
  
  // ===== 2. GESTIONE MATRICE TASTI =====
  // Legge se un tasto è stato premuto
  char customKey = customKeypad.getKey();
  
  // Se un tasto è stato premuto, gestiscilo
  if (customKey) {
    handleKeyPress(customKey);
  }
  
  // ===== 3. AGGIORNAMENTO PERIODICO OLED =====
  // Aggiorna display a intervalli regolari
  updateOLED();
  
  // ===== 4. PICCOLO DELAY PER STABILITÀ =====
  // Delay di 10ms per ridurre carico CPU e stabilizzare letture
  delay(10);
}