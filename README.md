# Irrigatore
Progetto Arduino per pilotare 8 relais con varie combinazioni temporali e/o temperatura e umidità

Configurazioni possibili (#define nei sorgenti)
- Numero dei relais: max 8
- Modalità di pilotaggio (I2C o canali di output)
- Se I2C, indirizzo 
- Se normale, serie di indirizzi delle porte
- Presenza di sensore umidità/temperatura DHT11 (in questo caso saranno accettate altre 4 modalità
  di accensione dei relais


# Modalità relais:
- 0: Non configurato
- 1: Acceso dalle ore xx:yy alle ore zz:ww
- 2: Partendo dalle ore xx:yy, acceso per zz minuti (max 120), poi si ripete ogni kk minuti (max 120)
- 3: Partendo dalle ore xx:yy, acceso per zz minuti (max 120), poi si ripete ogni k ore (max 18)
- 4: Acceso dalle ore xx:yy alle ore zz:ww, ma solo nei giorni della settimana configurati
- 5: Partendo dalle ore xx:yy, acceso per zz minuti (max 120), poi si ripete ogni kk minuti (max 120),
   ma solo nei giorni della settimana configurati.
- 6: Partendo dalle ore xx:yy, acceso per zz minuti (max 120), poi si ripete ogni k ore (max 18),
   ma solo nei giorni della settimana configurati.

SE DHT11 PRESENTE:
- 7: Acceso se Temperatura < x, spento se > y
- 8: Acceso se Temperatura > x, spento se < y
- 9: Acceso se Umidità < x, spento se > y
- 10:Acceso se Umidità > x, spento se < y

# Possibilità di configurazione da keypad:
- Un tasto qualsiasi per riaccendere il display che si spegne dopo un minuto
- Tasto 'A' : switch Automatico/Manuale. Se in Manuale, allora i tasti da 1 a 8 fanno cambiare stato al relais relativo.
- Tasto 'D' : Menu: 
  - A: Vedi configurazione relais
  - B: Configura relais
  - C: Configura orologio interno
  - D: Esci dal Menu
 # Librerie usate:
 - Keypad
 - LiquidCrystal_PCF8574
 - Wire
 - RTClib
 - EEPROM

 Se DHT11 presente, allora 
 - SimpleDHT
 # Note:
 Ad ogni configurazione effettuata, essa viene salvata nella EEPROM
   
