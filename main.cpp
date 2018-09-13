#include "mbed.h"
#include "TextLCD.h"
#include "MODSERIAL.h"
#include "tsi_sensor.h"

// Host PC Communication channels
MODSERIAL pc(USBTX, USBRX); // tx, rx
MODSERIAL blue(D14, D15);

//LDR input
DigitalIn ldr(D9);

//Timer interrupt
Ticker interrupt;

// I2C Communication
I2C i2c_lcd(D7,D6); // SDA, SCL

//Touch Sensor
TSIAnalogSlider tsi(9, 10, 40);

//Speaker
PwmOut speaker(D4);

//Pin assignments
DigitalOut led_red( D11);
DigitalOut led_green( D10);
AnalogIn knock_sensor( A0);
Timer t;

//LCD Instantiations
TextLCD_I2C lcd(&i2c_lcd, 0x7E, TextLCD::LCD20x4);                  // I2C exp: I2C bus, PCF8574AT Slaveaddress, LCD Type

//Some Note constants
#define NOTE_B0  31
#define NOTE_C1  33
#define NOTE_CS1 35
#define NOTE_D1  37
#define NOTE_DS1 39
#define NOTE_E1  41
#define NOTE_F1  44
#define NOTE_FS1 46
#define NOTE_G1  49
#define NOTE_GS1 52
#define NOTE_A1  55
#define NOTE_AS1 58
#define NOTE_B1  62
#define NOTE_C2  65
#define NOTE_CS2 69
#define NOTE_D2  73
#define NOTE_DS2 78
#define NOTE_E2  82
#define NOTE_F2  87
#define NOTE_FS2 93
#define NOTE_G2  98
#define NOTE_GS2 104
#define NOTE_A2  110
#define NOTE_AS2 117
#define NOTE_B2  123
#define NOTE_C3  131
#define NOTE_CS3 139
#define NOTE_D3  147
#define NOTE_DS3 156
#define NOTE_E3  165
#define NOTE_F3  175
#define NOTE_FS3 185
#define NOTE_G3  196
#define NOTE_GS3 208
#define NOTE_A3  220
#define NOTE_AS3 233
#define NOTE_B3  247
#define NOTE_C4  262
#define NOTE_CS4 277
#define NOTE_D4  294
#define NOTE_DS4 311
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_FS4 370
#define NOTE_G4  392
#define NOTE_GS4 415
#define NOTE_A4  440
#define NOTE_AS4 466
#define NOTE_B4  494
#define NOTE_C5  523
#define NOTE_CS5 554
#define NOTE_D5  587
#define NOTE_DS5 622
#define NOTE_E5  659
#define NOTE_F5  698
#define NOTE_FS5 740
#define NOTE_G5  784
#define NOTE_GS5 831
#define NOTE_A5  880
#define NOTE_AS5 932
#define NOTE_B5  988
#define NOTE_C6  1047
#define NOTE_CS6 1109
#define NOTE_D6  1175
#define NOTE_DS6 1245
#define NOTE_E6  1319
#define NOTE_F6  1397
#define NOTE_FS6 1480
#define NOTE_G6  1568
#define NOTE_GS6 1661
#define NOTE_A6  1760
#define NOTE_AS6 1865
#define NOTE_B6  1976
#define NOTE_C7  2093
#define NOTE_CS7 2217
#define NOTE_D7  2349
#define NOTE_DS7 2489
#define NOTE_E7  2637
#define NOTE_F7  2794
#define NOTE_FS7 2960
#define NOTE_G7  3136
#define NOTE_GS7 3322
#define NOTE_A7  3520
#define NOTE_AS7 3729
#define NOTE_B7  3951
#define NOTE_C8  4186
#define NOTE_CS8 4435
#define NOTE_D8  4699
#define NOTE_DS8 4978


//Some other Note constants
const int c = 261;
const int d = 294;
const int e = 329;
const int f = 349;
const int g = 391;
const int gS = 415;
const int a = 440;
const int aS = 466;
const int b = 494;
const int cH = 523;
const int cSH = 554;
const int dH = 587;
const int dSH = 622;
const int eH = 659;
const int fH = 698;
const int fSH = 740;
const int gH = 784;
const int gSH = 830;
const int aH = 880;

//Tuning constants. Changing the values below changes the behavior of the device./
const int threshold = 2000;                 // Minimum signal from the piezo to register as a knock. Higher = less sensitive. Typical values 1 - 10
const int rejectValue = 25;        // If an individual knock is off by this percentage of a knock we don't unlock. Typical values 10-30
const int averageRejectValue = 15; // If the average timing of all the knocks is off by this percent we don't unlock. Typical values 5-20
const int knockFadeTime = 150;     // Milliseconds we allow a knock to fade before we listen for another one. (Debounce timer.)
const int lockOperateTime = 2500;  // Milliseconds that we operate the lock solenoid latch before releasing it.
const int maximumKnocks = 20;      // Maximum number of knocks to listen for.
const int maximumUser = 10;
const int knockComplete = 1200;    // Longest time to wait for a knock before we assume that it's finished. (milliseconds)
const int maxNameLength = 30;
const int maximumPasswordLength = 10;
const int adminIndex = 0;

//Variables
char blueName[30] = {0};
char authListName[maximumUser][maxNameLength] = {0};
bool doorOpen = false;
bool breachFlag = false;
int toneSelectionList[maximumUser] = {0};
int secretCodes[ maximumUser ][ maximumKnocks ] = { 50, 25, 25, 50, 100, 50, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };  // Initial setup: "Shave and a Hair Cut, two bits."
int secretCodeIndex = 0;
int knockReadings[ maximumKnocks ] = { 0 };    // When someone knocks this array fills with the delays between knocks.
int knockSensorValue = 0;            // Last reading of the knock sensor.
bool programModeActive = false;   // True if we're trying to program a new knock.

//Admin Default Settings:
const char adminDefPassword[10] = {'0','1','2','3','4','5','6','7','8','9'};
const char adminDefName[30] = {'A','d','m','i','n'};

//Password
char passwordList[maximumUser][maximumPasswordLength] = {};

//ForgetKnockPattern
int forget_editUser = 0;
bool forget_editFlag = false;
bool adminSetting = false;

//Functions

//Function to play a tone for a certain duration
void play(int note, int duration)
{
    //Play tone on buzzerPin
    speaker = 0.1; //adjust volume
    speaker.period(1.0/note);
    wait_ms(duration);
    speaker = 0.0;
}

//function for star wars tone
void firstSection()
{
    play(a, 500);
    play(a, 500);
    play(a, 500);
    play(f, 350);
    play(cH, 150);
    play(a, 500);
    play(f, 350);
    play(cH, 150);
    play(a, 650);

    wait_ms(500);

    play(eH, 500);
    play(eH, 500);
    play(eH, 500);
    play(fH, 350);
    play(cH, 150);
    play(gS, 500);
    play(f, 350);
    play(cH, 150);
    play(a, 650);

    wait_ms(500);
}

//function for star wars tone 2
void secondSection()
{
    play(aH, 500);
    play(a, 300);
    play(a, 150);
    play(aH, 500);
    play(gSH, 325);
    play(gH, 175);
    play(fSH, 125);
    play(fH, 125);
    play(fSH, 250);

    wait_ms(325);

    play(aS, 250);
    play(dSH, 500);
    play(dH, 325);
    play(cSH, 175);
    play(cH, 125);
    play(b, 125);
    play(cH, 250);

    wait_ms(350);
}

//Plays the ith pre-saved tone as door open song. 0th tone is used for alarm
void playTone( int index)
{
    if ( index == 0 ) { //plays siren sound, used as breach tone
        int i = 0;
        for (i=10; i>=0; i=i-2) {
            speaker.period(1.0/969.0);
            speaker = float(i)/50.0;
            wait(.5);
            speaker.period(1.0/800.0);
            wait(.5);
            speaker =0.0;
        }
    } else if ( index == 1 ) {
        //Play first section
        firstSection();

        //Play second section //too long to play as tone
        /*secondSection();

        //Variant 1
        play(f, 250);
        play(gS, 500);
        play(f, 350);
        play(a, 125);
        play(cH, 500);
        play(a, 375);
        play(cH, 125);
        play(eH, 650);

        wait_ms(500);

        //Repeat second section
        secondSection();

        //Variant 2
        play(f, 250);
        play(gS, 500);
        play(f, 375);
        play(cH, 125);
        play(a, 500);
        play(f, 375);
        play(cH, 125);
        play(a, 650);

        wait_ms(650);*/

    } else if ( index == 2 ) { //Game of Thrones

        for(int i=0; i<2; i++) {
            play(NOTE_G4, 500);
            play(NOTE_C4, 500);
            play(NOTE_DS4, 250);
            play(NOTE_F4, 250);
        }

        for(int i=0; i<2; i++) {
            play(NOTE_G4, 500);
            play(NOTE_C4, 500);
            play(NOTE_E4, 250);
            play(NOTE_F4, 250);
        }

    } else if ( index == 3 ) { //Pirates of the Caribbean
        play(NOTE_E4, 125);
        play(NOTE_G4, 125);
        play(NOTE_A4, 250);
        play(NOTE_A4, 125);
        wait_ms(125);
        play(NOTE_A4, 125);
        play(NOTE_B4, 125);
        play(NOTE_C5, 250);
        play(NOTE_C5, 125);
        wait_ms(125);
        play(NOTE_C5, 125);
        play(NOTE_D5, 125);
        play(NOTE_B4, 250);
        play(NOTE_B4, 125);
        wait_ms(125);
        play(NOTE_A4, 125);
        play(NOTE_G4, 125);
        play(NOTE_A4, 375);

        wait_ms(125);
        play(NOTE_A4, 125);
        play(NOTE_B4, 125);
        play(NOTE_C5, 250);
        play(NOTE_C5, 125);
        wait_ms(125);
        play(NOTE_D5, 250);
        play(NOTE_E5, 125);
        play(NOTE_A4, 250);
        wait_ms(125);

        play(NOTE_A4, 125);
        play(NOTE_C5, 125);
        play(NOTE_B4, 250);
        play(NOTE_B4, 125);
        wait_ms(125);
        play(NOTE_C5, 125);
        play(NOTE_A4, 125);
        play(NOTE_B4, 375);

    } else if ( index == 4 ) { //Turkish March
        play(NOTE_B4, 125);
        play(NOTE_A4, 125);
        play(NOTE_GS4, 125);
        play(NOTE_A4, 125);
        play(NOTE_C5, 500);
        play(NOTE_D5, 125);
        play(NOTE_C5, 125);
        play(NOTE_B4, 125);
        play(NOTE_C5, 125);
        play(NOTE_E5, 500);
        play(NOTE_F5, 125);
        play(NOTE_E5, 125);
        play(NOTE_DS5, 125);
        play(NOTE_E5, 125);
        play(NOTE_B5, 125);
        play(NOTE_A5, 125);
        play(NOTE_GS5, 125);
        play(NOTE_A5, 125);
        play(NOTE_B5, 125);
        play(NOTE_A5, 125);
        play(NOTE_GS5, 125);
        play(NOTE_A5, 125);
        play(NOTE_C6, 500);

        play(NOTE_A5, 250);
        play(NOTE_B5, 250);
        play(NOTE_C6, 250);
        play(NOTE_B5, 250);
        play(NOTE_A5, 250);
        play(NOTE_GS5, 250);
        play(NOTE_A5, 250);
        play(NOTE_E5, 250);
        play(NOTE_F5, 250);
        play(NOTE_D5, 250);
        play(NOTE_C5, 500);
        play(NOTE_B4, 250);
        play(NOTE_A4, 125);
        play(NOTE_B4, 125);
        play(NOTE_A4, 500);
    }
}

// Unlocks the door and plays the tone that user selected
void doorUnlock( int delayTime, int index)
{
    lcd.locate(0,3);
    lcd.printf("Access Granted");
    doorOpen = true;
    led_green = 0;
    playTone( toneSelectionList[index] );
    while ( ldr == 1 ) { //hang around until door closes
        wait_ms(10);
    }
    led_green = 1;
    lcd.cls();
    lcd.printf("Locking door...");
    doorOpen = false;
    wait_ms(500);
    lcd.cls();
    lcd.printf("Enter your pattern");
}

// Plays back the pattern of the knock from speaker for user to remember
void playbackKnock(int maxKnockInterval)
{
    play(c, 100);
    for (int i = 0; i < maximumKnocks ; i++) {
        // only turn it on if there's a delay
        if (secretCodes[secretCodeIndex][i] > 0) {
            wait_ms(secretCodes[secretCodeIndex][i] * maxKnockInterval / 100); // Expand the time back out to what it was. Roughly.
            play(c, 100);
        }
    }
}

//Check if there is a breach
void checkBreach()
{
    if ( ldr == 1 && !doorOpen) { //there is a breach if true
        pc.printf("%d ", ldr.read());
        lcd.cls();
        breachFlag = 1;
        lcd.printf("UNAUTHORIZED BREACH ATTEMPT");
        interrupt.detach();
    }
}

// Check if recorded pattern matches with the argument pattern
// Returns true if knock matches with one of previously saved ones
// Can change tone/pattern/name information
bool validateKnock( int code[] )
{
    int i = 0;

    int currentKnockCount = 0;
    int secretKnockCount = 0;
    int maxKnockInterval = 0;               // We use this later to normalize the times.

    for ( i = 0; i<maximumKnocks; i++) {
        if (knockReadings[i] > 0) {
            currentKnockCount++;
        }
        if (code[i] > 0) {
            secretKnockCount++;
        }

        if (knockReadings[i] > maxKnockInterval) {  // Collect normalization data while we're looping.
            maxKnockInterval = knockReadings[i];
        }
    }

    // If we are in programming more check for which option was selected
    if (programModeActive == true) {

        if ( !adminSetting && !forget_editFlag && secretCodeIndex == maximumUser - 1 ) {
            lcd.cls();
            lcd.printf("Pattern List is Full");
            return false;
        }

        int tempIndex = 0;

        if( forget_editFlag) {
            tempIndex = secretCodeIndex;
            secretCodeIndex = forget_editUser;
        }

        else if( adminSetting) {
            tempIndex = secretCodeIndex;
            secretCodeIndex = adminIndex;
        } else
            secretCodeIndex++;

        for (i=0; i < maximumKnocks; i++) { // Normalize the time between knocks. (the longest time = 100)
            secretCodes[secretCodeIndex][i] = knockReadings[i] * 100 / maxKnockInterval;
        }

        lcd.cls();
        lcd.printf("Pattern Recorded");
        wait(1.5);

        //New User
        if( !forget_editFlag && !adminSetting) {
            lcd.cls();
            lcd.printf("To Continue, Please");
            lcd.locate(0,1);
            lcd.printf("Enter Admin Password");
            lcd.locate(0,2);
            lcd.printf("For Authorization:");

askAdminPassword:
            blue.printf("To Continue, Please Enter Admin Password For Authorization:");
            blue.printf("\r\n");

            blue.rxBufferFlush();
            while( blue.readable() == false)
                wait_ms(100);

            int iP = 0;
            char tempAdminPass[10] = {0};

            while(blue.readable()) {
                tempAdminPass[iP] = blue.getc();
                iP++;
            }

            for( int j = 0; j < maximumPasswordLength; j++) {
                if(passwordList[adminIndex][j] != 0 && passwordList[adminIndex][j] != tempAdminPass[j]) {
                    blue.printf("Error: Invalid Admin Password");
                    blue.printf("\r\n");
                    wait_ms(50);
                    goto askAdminPassword;
                }
            }

askUserName:
            lcd.cls();
            lcd.printf("Please Enter Your");
            lcd.locate(0,1);
            lcd.printf("User Name:");
            wait(1);

            //name insertion phase
            lcd.cls();
            lcd.printf("User Name: ");
            wait(0.5);

            blue.printf("Please Enter Your Name (MAX 30 char):");
            blue.printf("\r\n");

            blue.rxBufferFlush();
            while(blue.readable()== false) {
                wait_ms(100);
            }

            int j = 0;
            while(blue.readable()) {
                blueName[j] = blue.getc();
                j++;
            }

            pc.printf("Name of the New User: %s", blueName);

            wait_ms(100);

            for( i = 0; i < maxNameLength; i++) {
                authListName[secretCodeIndex][i] = blueName[i];
            }
            memset(blueName, 0, sizeof(blueName));
            blue.printf("User \"%s\" Saved", authListName[secretCodeIndex]);
            blue.printf("\r\n");

            lcd.locate(0,1);
            lcd.printf("%s", authListName[secretCodeIndex]);
            wait(2);

            lcd.cls();
            lcd.printf("User Name Saved");
            wait(1);

            lcd.cls();
            lcd.printf("Please Enter Your");
            lcd.locate(0,1);
            lcd.printf("Password");
            wait(0.5);

            blue.printf("Please Enter Your Password (MAX 10 char):");
            blue.printf("\r\n");

            blue.rxBufferFlush();
            while(blue.readable()== false) {
                wait_ms(100);
            }

            int iPas = 0;
            char tP[10] = {0};

            while(blue.readable()) {
                tP[iPas] = blue.getc();
                pc.printf("%c ", tP[iPas]);
                iPas++;
            }

            for( i = 0; i < maximumPasswordLength; i++) {
                passwordList[secretCodeIndex][i] = tP[i];
            }

            lcd.cls();
            lcd.printf("Password Saved");
            wait(1);

            blue.printf("Password Saved");
            blue.printf("\r\n");

//Forget/Change
        } else if( forget_editFlag) {
            lcd.cls();
            lcd.printf("To change your Tone");
            lcd.locate(0,1);
            lcd.printf("Enter 'y' or 'Y',");
            lcd.locate(0,2);
            lcd.printf("otherwise 'n' or 'N'");
            wait_ms(100);

            blue.printf("Do You Like to change your selection of Tone? Enter 'y' for Yes and 'n' for No.");
            blue.printf("\r\n");

askTone:
            blue.rxBufferFlush();
            while(blue.readable()== false) {
                wait_ms(100);
            }

            char tempAns = '0';

            tempAns = blue.getc();

            if( tempAns == 'y' || tempAns == 'Y')
                goto selectTone;

            else if( tempAns == 'n' || tempAns == 'N') {
                goto exitProg;
            }

            else {
                blue.printf("Error: Invalid Character. If you want to change your selection of Tone, Enter 'y' for Yes and 'n' for No.");
                blue.printf("\r\n");
                goto askTone;
            }
        } else if( adminSetting) {
            lcd.cls();
            lcd.printf("To change your Name");
            lcd.locate(0,1);
            lcd.printf("Enter 'y' or 'Y',");
            lcd.locate(0,2);
            lcd.printf("otherwise 'n' or 'N'");
            wait_ms(100);

            blue.printf("Do you like to change your Name? Enter 'y' for Yes and 'n' for No.");
            blue.printf("\r\n");

askAdName:
            blue.rxBufferFlush();
            while(blue.readable()== false) {
                wait_ms(100);
            }

            char tempAns = '0';

            tempAns = blue.getc();

            if( tempAns == 'y' || tempAns == 'Y')
                goto askAName;

            else if( tempAns == 'n' || tempAns == 'N') {
                goto askForATone;
            }

            else {
                blue.printf("Error: Invalid Character. If you want to change your name, Enter 'y' for Yes and 'n' for No.");
                blue.printf("\r\n");
                goto askAdName;
            }
askAName:
            lcd.cls();
            lcd.printf("Please Enter Your");
            lcd.locate(0,1);
            lcd.printf("Admin Name:");
            wait(1);

            //name insertion phase
            lcd.cls();
            lcd.printf("Admin Name: ");
            wait(0.5);

            blue.printf("Please Enter Your Admin Name (MAX 30 char):");
            blue.printf("\r\n");

            blue.rxBufferFlush();
            while(blue.readable()== false) {
                wait_ms(100);
            }

            int j = 0;
            while(blue.readable()) {
                blueName[j] = blue.getc();
                j++;
            }

            pc.printf("Name of the Admin: %s", blueName);

            wait_ms(100);

            for( i = 0; i < maxNameLength; i++) {
                authListName[secretCodeIndex][i] = blueName[i];
            }
            
            memset(blueName, 0, sizeof(blueName)); //empty the array
            
            blue.printf("Admin \"%s\" Saved", authListName[secretCodeIndex]);
            blue.printf("\r\n");

            lcd.locate(0,1);
            lcd.printf("%s", authListName[secretCodeIndex]);
            wait(2);

            lcd.cls();
            lcd.printf("Admin Name Saved");
            wait(1);

askForATone:
            lcd.cls();
            lcd.printf("To change your Tone"); //check until here
            lcd.locate(0,1);
            lcd.printf("Enter 'y' or 'Y',");
            lcd.locate(0,2);
            lcd.printf("otherwise 'n' or 'N'");
            wait_ms(100);

            blue.printf("Do you like to change your selection of Tone? Enter 'y' for Yes and 'n' for No.");
            blue.printf("\r\n");

askATone:
            blue.rxBufferFlush();
            while(blue.readable()== false) {
                wait_ms(100);
            }

            char tempAnswer1 = '0';

            tempAnswer1 = blue.getc();

            if( tempAnswer1 == 'y' || tempAnswer1 == 'Y')
                goto selectTone;

            else if( tempAnswer1 == 'n' || tempAnswer1 == 'N') {
                goto exitProg;
            }

            else {
                blue.printf("Error: Invalid Character. If You Do Like to Change Your Selection of Tone, Enter 'y' For Yes and 'n' For No.");
                blue.printf("\r\n");
                goto askATone;
            }
        }

selectTone:
        //tone selection phase
        lcd.cls();
        lcd.printf("Please Select Tone: ");
        wait(1);

        blue.printf("Please Enter Your Tone Selection: ( 1 - Star Wars, 2 - Game of Thrones, 3 - Pirates of the Caribbean, 4- Turkish March )");
        blue.printf("\r\n");

        blue.rxBufferFlush();
        while(blue.readable()== false) {
            wait_ms(100);
        }

        int j = 0;
        while(blue.readable()) {
            blueName[j] = blue.getc();
            j++;
        }
        pc.printf("Selected tone is: %s", blueName);
        wait_ms(100);

        toneSelectionList[secretCodeIndex] = ( ( (int) blueName[0] ) - 48); //get the number value 

        memset(blueName, 0, sizeof(blueName)); //empty the array
        
        blue.printf("Tone \"%d\" Saved", toneSelectionList[secretCodeIndex]);
        blue.printf("\r\n");

        lcd.cls();
        lcd.printf("Selected Tone: ");
        lcd.locate(0,1);
        lcd.printf("%d", toneSelectionList[secretCodeIndex]);
        wait(2);

        lcd.cls();
        lcd.printf("Tone Saved");
        wait(1);

        programModeActive = false;

        lcd.cls();
        lcd.printf("Replaying the knock");
        lcd.locate(0,1);
        lcd.printf("pattern for you to");
        lcd.locate(0,2);
        lcd.printf("remember...");
        wait(1);
        
        playbackKnock(maxKnockInterval); //playback the recorded knock pattern

exitProg:
        blue.printf("Exiting from the programming mode");
        blue.printf("\r\n");
        wait_ms(50);

        lcd.cls();
        lcd.printf("Exiting from the");
        lcd.locate(0,1);
        lcd.printf("programming mode");
        wait(1.5);


        lcd.cls();
        lcd.printf("Enter your pattern");
        led_red=1;

        if( forget_editFlag || adminSetting)
            secretCodeIndex = tempIndex;

        forget_editFlag = false;
        adminSetting = false;
        programModeActive = false;
        
        interrupt.attach(&checkBreach, 1); //attach breach interrupt before exiting program mode

        return false;
    }

    if (currentKnockCount != secretKnockCount) { //number of knocks do not match
        pc.printf("Error 1: knock amount does not match");
        return false;
    }

// compare relative intervals between recorded and previously saved patterns
//allows user to enter same pattern slower/faster
    int totaltimeDifferences = 0;
    int timeDiff = 0;
    for (i=0; i < maximumKnocks; i++) {   // Normalize the times
        timeDiff = abs( ( knockReadings[i] * 100 / maxKnockInterval ) - code[i]);
        if (timeDiff > rejectValue) {       // Timing is too off
            pc.printf("Error 2: Timing too off");
            return false;
        }
        totaltimeDifferences += timeDiff;
    }
    if (totaltimeDifferences / secretKnockCount > averageRejectValue) { // Average difference is too much
        pc.printf("Error 3: average is off");
        return false;
    }

    return true;
}



//waits between knocks
void knockDelay()
{
    int iterations = ( knockFadeTime / 20 );      //wait before listening to next one.

    for ( int i = 0; i < iterations; i++ ) {
        wait_ms( 10 );
        knock_sensor.read_u16();                  //defuse the piezo's capacitor
        wait_ms( 10 );
    }
}

// Records the timing between knocks.
void listenToSecretKnock()
{
    int i = 0;
    t.reset();
    
    //reset the knock timing array.
    for ( i = 0; i < maximumKnocks; i++ ) {
        knockReadings[i] = 0;
    }

    int currentKnockNumber = 0;    
    t.start();
    int startTime = t.read_ms();      // Reference for the first knock time.
    int now = t.read_ms();

    do {   // Listen for the next knock or wait for it to timeout.

        if ( breachFlag == 1 ) { //if interrupt occurred in this function exit to main
            return;
        }

        knockSensorValue = knock_sensor.read_u16();
        if ( knockSensorValue >= threshold ) { // other knock detected. record the time between knocks
            now = t.read_ms();
            knockReadings[currentKnockNumber] = now - startTime;
            currentKnockNumber++;
            startTime = now;

            if ( programModeActive == true ) { // Blink led when knock is sensed
                led_red = 1;
            } else {
                led_red = 0;
            }

            knockDelay();

            if ( programModeActive == true ) { // Un-blink the led
                led_red = 0;
            } else {
                led_red = 1;
            }
        }

        now = t.read_ms();

        // Stop listening if there are too many knocks or there is too much time between knocks.
    } while ( ( now - startTime < knockComplete ) && ( currentKnockNumber < maximumKnocks ) );

    //knocks are recorded, check if is valid
    if ( programModeActive == false ) { // Only do this if we're not recording a new knock.

        bool flag = 0; //flag to check if pattern matches

        for ( int j = 0; j <= secretCodeIndex; j++ ) {
            for ( int k = 0; k < maximumKnocks; k++ ) {
                pc.printf("%d ", secretCodes[j][k]);
            }
            if ( validateKnock( secretCodes[j] ) == true ) {
                flag = 1;

                blue.printf("%s opened the door lock", authListName[j]);
                blue.printf("\r\n");
                wait_ms(50);

                lcd.cls();
                lcd.printf("Welcome: ");
                lcd.locate(0,1);
                lcd.printf("%s", authListName[j]);
                doorUnlock( lockOperateTime, j );

                pc.printf("Index: %d ", j); //debug

                break;
            }
        }
        if ( flag == 0 ) { // invalid knock. Blink the LED to warn others
            lcd.cls();
            lcd.printf("Access Denied");
            for ( i=0; i < 4; i++ ) {
                led_red = 0;
                wait_ms( 50 );
                led_red = 1;
                wait_ms( 50 );
            }
            wait(1);

            if ( breachFlag == 1 ) { //if interrupt occurred in this function exit to main
                return;
            }

            lcd.cls();
            lcd.printf("Enter your pattern");
        }
    } else { //save the recorded knock
        validateKnock( secretCodes[0] ); //initial code is passed but it does not matter
    }
}

//Main
int main()
{
    for( int i = 0; i < maximumPasswordLength; i++) {
        passwordList[adminIndex][i] = adminDefPassword[i];
    }

    for( int i = 0; i < maxNameLength; i++) {
        authListName[adminIndex][i] = adminDefName[i];
    }
    
    //Default Admin Tone
    toneSelectionList[adminIndex] = 1;

    //instantiate baud rates
    pc.baud(38400);
    blue.baud(38400);

    //initial values of Leds
    led_red = 1;
    led_green = 1;

    //instantiate LCD
    lcd.setBacklight(TextLCD::LightOn);
    lcd.setCursor(TextLCD::CurOff_BlkOn);
    lcd.printf("Welcome!");
    wait(2);
    lcd.cls();
    lcd.printf("Enter your pattern");

    //Test BLT
    blue.printf("Connected");
    blue.printf("\r\n");
    wait_ms(50);

    //attach the interrupt
    interrupt.attach(&checkBreach, 1.0); //each second check for a breach

    while (1) { // Listen for knocks continuously.
        knockSensorValue = knock_sensor.read_u16();

        if ( breachFlag == 1 ) {

            blue.printf("Entered Programming Mode");
            blue.printf("\r\n");
            wait_ms(50);
            lcd.locate(0,2);
            lcd.printf("Enter admin password");
            lcd.locate(0,3);
            lcd.printf("to close alarm");
            wait(1);

askAdPassword:
            pc.printf("%d ", ldr.read());

            blue.printf("Unauthorized breach attempt! Enter your admin password to shut alarm off");
            blue.printf("\r\n");
            wait(1);

            blue.rxBufferFlush();
            while ( blue.readable() == false ) {
                playTone(0); //siren
            }

            int j = 0;
            char blueAdPass[maximumPasswordLength] = {0};
            while(blue.readable()) {
                blueAdPass[j] = blue.getc();
                j++;
            }

            for( int k = 0; k < maximumPasswordLength; k++) {
                if(passwordList[adminIndex][k] != 0 && passwordList[adminIndex][k] != blueAdPass[k]) {
                    blue.printf("Error: Invalid Password");
                    blue.printf("\r\n");
                    wait_ms(200);
                    goto askAdPassword;
                }
            }

            blue.printf("Password verified");
            blue.printf("\r\n");
            wait_ms(50);

            lcd.cls();
            lcd.printf("Password verified");
            wait_ms(500);

            while ( ldr == 1 ) {
                lcd.locate(0,1);
                lcd.printf("Please close the");
                lcd.locate(0,2);
                lcd.printf("door to reactivate");
                lcd.locate(0,3);
                lcd.printf("lock");
            }

            lcd.cls();
            lcd.printf("Enter your pattern");
            breachFlag = 0;
            interrupt.attach(&checkBreach, 1);
        }

        if ( tsi.readPercentage() >= 0.7 ) {  // is the touch slider touched?
            wait_ms( 100 );   //debounce

            if ( tsi.readPercentage() >= 0.7 ) {

                if ( programModeActive == false ) { // If we're not in programming mode, turn it on.
                    programModeActive = true;      

                    blue.printf("Entered Programming Mode");
                    blue.printf("\r\n");
                    wait_ms(50);

                    lcd.cls();
                    lcd.printf("Entered Programming");
                    lcd.locate(0,1);
                    lcd.printf("Mode");
                    wait_ms(500);

                    interrupt.detach(); //dont check for breaches in programming mode

                    led_red = 0;     // Turn on red led to inform user that we're programming
                    wait_ms(500);

                    //Programming Mode: New User/Forget Knock Pattern/Admin Settings
                    lcd.cls();
                    lcd.printf("Please select:");
                    lcd.locate(0,1);
                    lcd.printf("1) New User");
                    lcd.locate(0,2);
                    lcd.printf("2) Change Pattern");
                    lcd.locate(0,3);
                    lcd.printf("3) Admin Settings");
                    wait(1);

backToSelect:       //Bluetooth for Programming Mode
                    blue.printf("Please Select:");
                    blue.printf("\r\n");
                    wait_ms(50);
                    blue.printf("1) New User");
                    blue.printf("\r\n");
                    wait_ms(50);
                    blue.printf("2) Forget Knock Pattern");
                    blue.printf("\r\n");
                    wait_ms(50);
                    blue.printf("3) Admin Settings");
                    blue.printf("\r\n");
                    wait_ms(50);

                    blue.rxBufferFlush();
                    while(blue.readable() == false) {
                        wait_ms(10);
                    }

                    char programSelect = '0';

                    programSelect = blue.getc();

                    //New User
                    if( programSelect == '1') {
                        goto newUser;
                    }

                    //Forget/Edit Knock Pattern
                    else if (programSelect == '2') {
                        forget_editFlag = true;
askName:
                        blue.printf("Please Enter Your Name:");
                        blue.printf("\r\n");
                        wait_ms(50);

                        blue.rxBufferFlush();
                        while(blue.readable() == false) {
                            wait_ms(100);
                        }

                        int iName = 0;
                        char tempName[30] = {0};

                        while(blue.readable()) {
                            tempName[iName] = blue.getc();
                            iName++;
                        }

                        bool foundName = false;

                        for( int i = 0; i < maximumUser; i++) {
                            for( int j = 0; j < maxNameLength; j++) {
                                if(authListName[i][j] != 0 && authListName[i][j] != tempName[j]) {
                                    foundName = false;
                                    break;
                                }
                                foundName = true;
                            }
                            if(foundName ) {
                                forget_editUser = i;
                                break;
                            }
                        }
                        if (!foundName) {
                            blue.printf("Error: Name Cannot Be Found");
                            blue.printf("\r\n");
                            wait_ms(50);
                            goto askName;
                        }

                        lcd.cls();
                        lcd.printf("User Found:");
                        lcd.locate(0,1);
                        lcd.printf("%s", authListName[forget_editUser]);
                        lcd.locate(0,2);
                        lcd.printf("Please enter password:");
                        wait(0.2);

                        blue.printf("User Found:");
                        blue.printf("\r\n");
                        wait_ms(50);
                        blue.printf("%s", authListName[forget_editUser]);
                        blue.printf("\r\n");
                        wait_ms(50);
askPassword:
                        blue.printf("Please enter password:");
                        blue.printf("\r\n");
                        wait_ms(50);

                        blue.rxBufferFlush();
                        while(blue.readable() == false) {
                            wait_ms(100);
                        }

                        int iPass = 0;
                        char tempPass[10] = {0};

                        while(blue.readable()) {
                            tempPass[iPass] = blue.getc();
                            pc.printf("%c ", tempPass[iPass]);
                            iPass++;

                        }

                        for( int j = 0; j < maximumPasswordLength; j++) {
                            if(passwordList[forget_editUser][j] != 0 && passwordList[forget_editUser][j] != tempPass[j]) {
                                blue.printf("Error: Invalid Password");
                                blue.printf("\r\n");
                                wait_ms(50);
                                goto askPassword;
                            }
                        }
                        forget_editFlag = true;
                    }

                    //Admin Settings
                    else if(programSelect == '3') {
                        adminSetting = true;
                        lcd.cls();
                        lcd.printf("To Continue, Please");
                        lcd.locate(0,1);
                        lcd.printf("Enter Admin Password");
                        lcd.locate(0,2);
                        lcd.printf("For Authorization:");

askAdP:
                        blue.printf("To Continue, Please Enter Admin Password For Authorization:");
                        blue.printf("\r\n");

                        blue.rxBufferFlush();
                        while( blue.readable() == false)
                            wait_ms(100);

                        int iAP = 0;
                        char tempAP2[10] = {0};

                        while(blue.readable()) {
                            tempAP2[iAP] = blue.getc();
                            iAP++;
                        }

                        for( int m = 0; m < maximumPasswordLength; m++) {
                            if(passwordList[adminIndex][m] != 0 && passwordList[adminIndex][m] != tempAP2[m]) {
                                blue.printf("Error: Invalid Admin Password");
                                blue.printf("\r\n");
                                wait_ms(50);
                                goto askAdP;
                            }
                        }

                        lcd.cls();
                        lcd.printf("To change Admin");
                        lcd.locate(0,1);
                        lcd.printf("Password, enter 'y'");
                        lcd.locate(0,2);
                        lcd.printf("otherwise 'n'");

                        blue.printf("Do you like to change Admin password? Enter 'y' for YES and 'n' for NO");
                        blue.printf("\r\n");
                        wait_ms(50);

askAdminPass:
                        blue.rxBufferFlush();
                        while( blue.readable() == false)
                            wait_ms(100);

                        char tempAns = '0';

                        tempAns = blue.getc();

                        if( tempAns == 'y' || tempAns == 'Y') {
                            lcd.cls();
                            lcd.printf("Please Enter Your");
                            lcd.locate(0,1);
                            lcd.printf("Password");
                            wait(0.5);

                            blue.printf("Please Enter Your Password (MAX 10 char):");
                            blue.printf("\r\n");

                            blue.rxBufferFlush();

                            while(blue.readable()== false) {
                                wait_ms(100);
                            }

                            int iP = 0;
                            char tP[10] = {0};

                            while(blue.readable()) {
                                tP[iP] = blue.getc();
                                iP++;
                            }

                            for( int i = 0; i < maximumPasswordLength; i++) {
                                passwordList[adminIndex][i] = tP[i];
                            }

                            lcd.cls();
                            lcd.printf("Admin Password Saved");
                            wait(1);

                            blue.printf("Admin Password Saved");
                            blue.printf("\r\n");

                            adminSetting = true;
                        }

                        else if( tempAns == 'n' || tempAns == 'N') {
                            adminSetting = true;
                            goto newUser;
                        }

                        else {
                            blue.printf("Error: Invalid Character. If you want to change Your Password, Enter 'y' For Yes and 'n' For No.");
                            blue.printf("\r\n");
                            goto askAdminPass;
                        }

                    }

                    else {
                        blue.printf("Error: Invalid Character");
                        blue.printf("\r\n");
                        wait_ms(500);
                        blue.printf("Please Select Again:");
                        blue.printf("\r\n");
                        wait_ms(50);
                        goto backToSelect;
                    }

newUser:
                    wait_ms(100);
                    lcd.cls();
                    lcd.printf("Enter your pattern");
                    lcd.locate(0,1);
                    lcd.printf("to be saved");
                }
                while ( tsi.readPercentage() >= 0.7 ) {
                    wait_ms( 10 );                         // stay here until touch senser is released
                }
            }
            wait_ms( 250 );   //debounce
        }

        if ( knockSensorValue >= threshold ) {
            if (programModeActive == true) { // Blink led when knock is sensed
                led_red = 1;
            } else {
                led_red = 0;
            }

            knockDelay();

            pc.printf("Sensor Value: %d ", knockSensorValue);

            if (programModeActive == true) { // Un-blink led
                led_red = 0;
            } else {
                led_red = 1;
            }

            listenToSecretKnock();           // First knock detected, check for others and save
        }
    }
}