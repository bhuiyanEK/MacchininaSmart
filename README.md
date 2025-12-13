# Macchinina Smart

### Descrizione 
Il progetto ha l'obiettivo di simulare un macchina smart. La macchina è progettato per operare in tre modalità distinte: navigazione autonoma con evitamento ostacoli, una modalità "Gara" basata sul riconoscimento dei colori (tramite cartellini), e una modalità di controllo remoto manuale tramite interfaccia Web (Wi-Fi).

### Componenti
* ESP32-S3
* Modulo L298N
* 4x Motori DC 
* Schermo OLED 128x32   
* HC-SR04 (Ultrasuoni)    
* Servo Motore SG90 da 180°
* TCS3200 (Sensore di colore)
* 3 Bottoni (Rosso, Bianco, Blu) per selezionare le modalità.
* 4x Batterie 18650
* Buck Converter

### Modalità di Funzionamento

Il robot dispone di tre pulsanti fisici che attivano tre logiche differenti:

#### A. Modalità GARA (Bottone Rosso)
La particolarità di questa macchina è che il sensore di colore non guarda a terra, ma di lato (sulla fiancata)
1.  **Attesa/Pronto** La macchina rimane immobile (attesa) e aspetta il colore **rosso** per prepararsi.
2.  **Partenza:** Se il sensore rileva il verde per almeno 1 secondo, la macchina parte.
3.  **Corsa:** La macchina naviga autonomamente evitando gli ostacoli (sfruttando sempre il sensore ultrasuoni) per 60 secondi.
4.  **Fine:** Se durante la corsa il sensore legge il colore **blu**, la macchina si ferma immediatamente. Altrimenti, si ferma allo scadere del tempo.

#### B. Modalità AUTOMATICA (Bottone Bianco)
Navigazione autonoma classica.
1.  **Avvio:** Parte un conto alla rovescia.
2.  **Navigazione:** La macchina si muove in avanti. Se incontra un ostacolo (distanza < 30cm):
    * Si ferma.
    * Il servo ruota il sensore a destra e sinistra.
    * La macchina sceglie la direzione con più spazio libero e *sterza*.
3.  **Durata:** La modalità dura 30 secondi, poi si ferma.

#### C. Modalità WI-FI (Pulsante Blu)
Trasforma La macchina in un veicolo radiocomandato.
1.  **Utilizzo** L'ESP32 crea una rete Wi-Fi con tutte le informazioni per collegarsi sullo schermo.
2.  **Controllo:** Collegandosi al sito web, appare un'interfaccia con frecce direzionali.

### Considerazioni e limiti del progetto
Inizialmente, avevamo posizionato il sensore di colore sulla parte anteriore per fargli riconoscere gli oggetti e agire di conseguenza. Tuttavia, dai test è emerso un problema pratico: il sensore leggeva correttamente il colore
solo a 1 o 2 centimetri di distanza. Per evitare che il robot, dovendosi avvicinare così tanto, finisse per schiantarsi contro l'ostacolo, ho deciso di introdurre il sensore di ultrasuoni e cambiare gli obiettivi principali.

Inoltre, è importante notare che non sterza come una macchina vera (girando le ruote anteriori). Invece per girare a destra o a sinistra, le ruote di un lato vanno avanti mentre quelle dell'altro lato si fermano o vanno indietro.

Infine, un problema è stato riscontrato durante il conto alla rovescia: in questa fase il sistema non può aggiornare i sensori o i motori in tempo reale, pertanto il processo non risulta pienamente ottimizzato.
