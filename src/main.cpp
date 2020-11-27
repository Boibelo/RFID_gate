/**
 * ----------------------------------------------------------------------------
 * Frank Sobel - 01 Oct 2020 
 * program to read card and switch on relay based on expected card data
 * card data defined by validblk
 * sector under check : 2
 * block under check : 9
 * Serial open with debug TRUE
 * BASED ON MFRC522 library example; see https://github.com/miguelbalboa/rfid
 * 
 */


#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
//#include <LowPower.h> for Arduino


#define RST_PIN     4           // Configurable, see typical pin layout above
#define SS_PIN      5           // Configurable, see typical pin layout above
#define LED_PIN     26          // 34 is not convenient
#define RELAY_PIN   25          // 35 is not convenient
#define REL_VCC_PIN 27          // for powering the relay     

MFRC522 mfrc522(SS_PIN, RST_PIN);   // Create MFRC522 instance.
MFRC522::MIFARE_Key key;

byte sector         = 2;
byte blockAddr      = 9;
byte trailerBlock   = 7;
MFRC522::StatusCode status;
byte buffer[18];
byte size = sizeof(buffer);
bool Relay_state; 
bool Debug=true;
bool Tagwrite; // if true, the tag will be written to become a good tag

// Sleep mode parameters and setting for no wireless comms 
RTC_DATA_ATTR int gate_entries = 0;

/**
 * Helper routine to dump a byte array as hex values to Serial.
 */
void dump_byte_array(byte *buffer, byte bufferSize) {
    for (byte i = 0; i < bufferSize; i++) {
        Serial.print(buffer[i] < 0x10 ? " 0" : " ");
        Serial.print(buffer[i], HEX);
    }
}

void setup() {
    /* arduino nano power consumption reduction
    CLKPR = 0x80; // (1000 0000) enable change in clock frequency
    CLKPR = 0x01; // (0000 0001) use clock division factor 2 to reduce the frequency from 16 MHz to 8 MHz
    */
    esp_sleep_enable_timer_wakeup(3000000); // ESP wakes up every 3 sec 
    if (Debug) Serial.begin(115200); // Initialize serial communications with the PC
    SPI.begin();        // Init SPI bus
    mfrc522.PCD_Init(); // Init MFRC522 card
    pinMode(LED_PIN, OUTPUT);
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN,Relay_state);

    // Prepare the key (used both as key A and as key B)
    // using FFFFFFFFFFFFh which is the default at chip delivery from the factory
    for (byte i = 0; i < 6; i++) {
        key.keyByte[i] = 0xFF;
    }

}

void loop() {
    // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
    //  LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF); for Arduino
    
    if ( ! mfrc522.PICC_IsNewCardPresent() || ! mfrc522.PICC_ReadCardSerial() ) {
        Serial.println("ESP32 going to Deep Sleep");
        esp_deep_sleep_start();
        return;
    }

    // Show some details of the PICC (that is: the tag/card)
    
    if (Debug) {
        Serial.print(F("Card UID:"));
        dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size);
        Serial.println();
        Serial.print(F("PICC type: "));
        MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
        Serial.println(mfrc522.PICC_GetTypeName(piccType));
        // Check for compatibility
        if (    piccType != MFRC522::PICC_TYPE_MIFARE_MINI
            &&  piccType != MFRC522::PICC_TYPE_MIFARE_1K
            &&  piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
            Serial.println(F("This sample only works with MIFARE Classic cards."));
            return;
        } 
        // Authenticate using key A
        Serial.println(F("Authenticating using key A..."));
        status = (MFRC522::StatusCode) mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(mfrc522.uid));
        if (status != MFRC522::STATUS_OK) {
            Serial.print(F("PCD_Authenticate() failed: "));
            Serial.println(mfrc522.GetStatusCodeName(status));
            return;
        }
        // Show the whole sector as it currently is
        Serial.println(F("Current data in sector:"));
        mfrc522.PICC_DumpMifareClassicSectorToSerial(&(mfrc522.uid), &key, sector);
        Serial.println();

        // Read data from the block
        Serial.print(F("Reading data from block ")); Serial.print(blockAddr);
        Serial.println(F(" ..."));
        status = (MFRC522::StatusCode) mfrc522.MIFARE_Read(blockAddr, buffer, &size);
        if (status != MFRC522::STATUS_OK) {
            Serial.print(F("MIFARE_Read() failed: "));
            Serial.println(mfrc522.GetStatusCodeName(status));
        }
    }
    // activate relay pin if reading of block blockAddr is correct
    mfrc522.PICC_DumpMifareClassicSectorToSerial(&(mfrc522.uid), &key, sector);
    status = (MFRC522::StatusCode) mfrc522.MIFARE_Read(blockAddr, buffer, &size);
    byte validblk[]    = {
    0x01, 0x02, 0x03, 0x04, 
    0x05, 0x06, 0x07, 0x08, 
    0x09, 0x0a, 0xff, 0x0b, 
    0x0c, 0x0d, 0x0e, 0xcd  
    };
    if ( buffer[0]==validblk[0] 
        && buffer[5]==validblk[5]
        && buffer[6]==validblk[6]
        && buffer[11]==validblk[11] ) {
        pinMode(REL_VCC_PIN, OUTPUT);
        delay(1);
        digitalWrite(REL_VCC_PIN,HIGH);
        delay(5);
        digitalWrite(RELAY_PIN,!Relay_state);
        delay(100);
        if (Debug) {
            Serial.print("Relay has been switched on with buffer 0 : ");
            Serial.println(buffer[0]);
            Serial.print("and validblk 0 : ");
            Serial.println(validblk[0]);
        }
        digitalWrite(RELAY_PIN,Relay_state);
        digitalWrite(REL_VCC_PIN,LOW);
    }
    else {
        digitalWrite(LED_PIN,HIGH);
        delay(50);
        if (Debug) {
            Serial.println("Relay has not been switched on");
            Serial.print(F("buffer : ")); 
            dump_byte_array(buffer, 16); Serial.println();          
            Serial.print(F("valiblk : ")); 
            dump_byte_array(validblk, 16); Serial.println();
        }
        digitalWrite(LED_PIN,LOW);
    }
    if (Tagwrite) {
        // Write data to the block
        Serial.print(F("Writing data into block ")); Serial.print(blockAddr);
        Serial.println(F(" ..."));
        dump_byte_array(validblk, 16); Serial.println();
        status = (MFRC522::StatusCode) mfrc522.MIFARE_Write(blockAddr, validblk, 16);
        if (status != MFRC522::STATUS_OK) {
            Serial.print(F("MIFARE_Write() failed: "));
            Serial.println(mfrc522.GetStatusCodeName(status));
        }
        Serial.println();
        // Read data from the block (again, should now be what we have written)
        Serial.print(F("Reading data from block ")); Serial.print(blockAddr);
        Serial.println(F(" ..."));
        status = (MFRC522::StatusCode) mfrc522.MIFARE_Read(blockAddr, buffer, &size);
        if (status != MFRC522::STATUS_OK) {
            Serial.print(F("MIFARE_Read() failed: "));
            Serial.println(mfrc522.GetStatusCodeName(status));
        }
        Serial.print(F("Data in block ")); Serial.print(blockAddr); Serial.println(F(":"));
        dump_byte_array(buffer, 16); Serial.println();

        // Check that data in block is what we have written
        // by counting the number of bytes that are equal
        Serial.println(F("Checking result..."));
        byte count = 0;
        for (byte i = 0; i < 16; i++) {
            // Compare buffer (= what we've read) with validblk (= what we've written)
            if (buffer[i] == validblk[i])
                count++;
        }
        Serial.print(F("Number of bytes that match = ")); Serial.println(count);
        if (count == 16) {
            Serial.println(F("Success :-)"));
        } else {
            Serial.println(F("Failure, no match :-("));
            Serial.println(F("  perhaps the write didn't work properly..."));
        }
        Serial.println();
        // Dump the sector data
        Serial.println(F("Current data in sector:"));
        mfrc522.PICC_DumpMifareClassicSectorToSerial(&(mfrc522.uid), &key, sector);
        Serial.println();
    }
    // Halt PICC
    mfrc522.PICC_HaltA();
    // Stop encryption on PCD
    mfrc522.PCD_StopCrypto1();
}

