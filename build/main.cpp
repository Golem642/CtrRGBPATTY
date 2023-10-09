#include <stdio.h>  // Basic functions
#include <stdlib.h> // malloc and free
#include <string.h> // String manipulation
#include <dirent.h> // For opendir() and readdir()
#include <unistd.h> // rmdir()
#include <fstream>
#include <sstream>
#include <3ds.h>    // Main 3ds lib (libctru)
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>

typedef struct {
    uint8_t r[32];
    uint8_t g[32];
    uint8_t b[32];
} LED;

typedef struct {
    uint8_t ani[4]; // animation settings : https://www.3dbrew.org/wiki/MCURTC:SetInfoLEDPattern

    // colors
    uint8_t r[32];
    uint8_t g[32];
    uint8_t b[32];
} LED_MCU;

std::string menu[10]={
    "Set notification RGB hex color",
    "Change pattern for LED",
    "Change animation speed (delay)",
    "Change animation smoothness",
    "Change loop delay",
    "Change static ending",
    "Test LED with pattern",
    "Install IPS Patch",
    "Toggle enabled state",
    "Shutdown 3DS"
};

std::string installMenu[5]={
    "Unknown",
    "SpotPass",
    "StreetPass",
    "Friends ???",
    "Friends"
};

int currMenu = 0;

int selected, nbMain = 10;
int nbInstall = 5;
int nbCmd = nbMain;
int nbCustom = 32;

std::string patterns[5]={
    "Blink  ",
    "Explode",
    "Rainbow",
    "Static ",
    "Custom "
};

std::string staticEnds[4]={
    "None       ",
    "If sleeping",
    "If awake   ",
    "Both       "
};

int selectedpat, PATS = 5;

char keybInput[7] = "";

// defaults
char color_HEX[] = "2200ff"; // Originally 910b0b
char color_copy[10] = "000000"; // 10 to avoid gcc compiling warnings

char anim_speed[] = "2f";
char anim_smooth[] = "5f";
char anim_loop_delay[] = "ff";
char anim_blink_speed[] = "00";

int staticend = 1;
// DEFAULT 3DS SETTINGS :
// - ????? : 0x50, 0x50, 0xFF, 0x00 (there are no color settings after wtf is this)
// - SPOTPASS : 0x50, 0x3C, 0xFF, 0x00
// - STREETPASS : 0x50, 0x50, 0xFF, 0x00
// - FRIENDS ??? : 0x68, 0x68, 0xFF, 0x00 (strange friend notification type but different)
// - FRIENDS : 0x50, 0x3C, 0xFF, 0x00
// - LOW BATTERY : not found for now :(
uint8_t ANIMDELAY = 0x2F; // 0x50
uint8_t ANIMSMOOTH = 0x5F; // 0x3c
uint8_t LOOPBYTE = 0xFF; // no loop
uint8_t BLINKSPEED = 0x00; // https://www.3dbrew.org/wiki/MCURTC:SetInfoLEDPattern
bool enabled;

bool debugMode = false;

LED customLed;

static SwkbdState swkbd;
static SwkbdStatusData swkbdStatus;
static SwkbdLearningData swkbdLearning;

static SwkbdCallbackResult hexadecimalCheck(void* user, const char** ppMessage, const char* text, size_t textlen) {
    //char asciiNum[100];
    for (int i = 0; i < (int)textlen; i++) {
        //printf("Char %d is %d\n", i, (int)text[i]);
        if (!((text[i] >= '0' && text[i] <= '9') || (text[i] >= 'A' && text[i] <= 'F') || (text[i] >= 'a' && text[i] <= 'f'))) {
            *ppMessage = "The text must be in\nhexadecimal";
            return SWKBD_CALLBACK_CONTINUE;
        }
    }
    return SWKBD_CALLBACK_OK;
}

void hexaInput(char* hexaText, int hexaLen, const char* hintText) {
    swkbdInit(&swkbd, SWKBD_TYPE_QWERTY, 2, hexaLen);
    swkbdSetValidation(&swkbd, SWKBD_FIXEDLEN, 0, 0);
    swkbdSetFilterCallback(&swkbd, hexadecimalCheck, NULL);
    swkbdSetFeatures(&swkbd, SWKBD_FIXED_WIDTH);
    swkbdSetInitialText(&swkbd, hexaText);
    swkbdSetHintText(&swkbd, hintText);
    swkbdSetStatusData(&swkbd, &swkbdStatus, false, true);
    swkbdSetLearningData(&swkbd, &swkbdLearning, false, true);
    swkbdInputText(&swkbd, (char*)keybInput, hexaLen + 1);

    if (strcmp(keybInput, "") != 0) {
        strcpy(hexaText, keybInput);
    }
}

int fcopy(const char* source, const char* dest) {
    FILE *f1 = fopen(source, "rb");
    if (f1 == NULL) {
        printf("Cannot open source file\n");
        return 1;
    }
    FILE *f2 = fopen(dest, "wb+");
    if (f2 == NULL) {
        printf("Cannot open destination file\n");
        return 1;
    }
    size_t n, m;
    char buff;
    m = 0;
    do {
        n = fread(&buff, 1, sizeof(buff), f1);
        if (!feof(f1)) m = fwrite(&buff, 1, n, f2);
        else m = 0;
    } while (!feof(f1));
    if (m) {
        perror("Error while copying");
        return 1;
    }
    if (fclose(f2)) {
        perror("Error while closing output file");
        return 1;
    }
    if (fclose(f1)) {
        perror("Error while closing input file");
        return 1;
    }
    return 0;
}

void intRGB(std::string hexCode, int *r, int *g, int *b)
{
    // Remove the hashtag ...
    if(hexCode.at(0) == '#') 
    {
        hexCode = hexCode.erase(0, 1);
    }

    // ... and extract the rgb values.
    std::istringstream(hexCode.substr(0,2)) >> std::hex >> *r;
    std::istringstream(hexCode.substr(2,2)) >> std::hex >> *g;
    std::istringstream(hexCode.substr(4,2)) >> std::hex >> *b;
}

/* Documentation:
    LED struct has RGB patterns for 32 itterations. With this you can make an animation with the LED. (like the one MCU BRICKER does).
*/

void createLED(LED* patern, std::string hexCode, bool staticEND, int selcpat)
{
    // LED datastruct we will be returning
    int r, g, b;

    // Remove the hashtag ...
    if(hexCode.at(0) == '#') 
    {
        hexCode = hexCode.erase(0, 1);
    }

    // ... and extract the rgb values.
    std::istringstream(hexCode.substr(0,2)) >> std::hex >> r;
    std::istringstream(hexCode.substr(2,2)) >> std::hex >> g;
    std::istringstream(hexCode.substr(4,2)) >> std::hex >> b;
    
    if (debugMode) {
        printf("(%d, %d, %d)\n", r, g ,b);
    }
    printf("Writing to RGB struct...\n");

    memset(&patern->r[0], 0, 32);
    memset(&patern->g[0], 0, 32);
    memset(&patern->b[0], 0, 32);

    switch(selcpat)
    {
        case 0: // Blink
            for (int i = 1; i<31; i+=10)
            {
                memset(&patern->r[i], r, 5); 
                memset(&patern->g[i], g, 5); 
                memset(&patern->b[i], b, 5);
            }
            patern->r[31] = r;
            patern->g[31] = g;
            patern->b[31] = b;
        break;
        case 1: // Explode
            for (int i = 1; i<31; i+=10)
            {
                patern->r[i] = r/i;
                patern->g[i] = g/i;
                patern->b[i] = b/i;
            }
            patern->r[31] = r;
            patern->g[31] = g;
            patern->b[31] = b;
        break;
        case 2: // Rainbow (AKA MCU bricker lol)
            patern->r[0] = 128;
            patern->r[1] = 103;
            patern->r[2] = 79;
            patern->r[3] = 57;
            patern->r[4] = 38;
            patern->r[5] = 22;
            patern->r[6] = 11;
            patern->r[7] = 3;
            patern->r[8] = 1;
            patern->r[9] = 3;
            patern->r[10] = 11;
            patern->r[11] = 22;
            patern->r[12] = 38;
            patern->r[13] = 57;
            patern->r[14] = 79;
            patern->r[15] = 103;
            patern->r[16] = 128;
            patern->r[17] = 153;
            patern->r[18] = 177;
            patern->r[19] = 199;
            patern->r[20] = 218;
            patern->r[21] = 234;
            patern->r[22] = 245;
            patern->r[23] = 253;
            patern->r[24] = 255;
            patern->r[25] = 253;
            patern->r[26] = 245;
            patern->r[27] = 234;
            patern->r[28] = 218;
            patern->r[29] = 199;
            patern->r[30] = 177;
            patern->r[31] = r;
            patern->g[0] = 238;
            patern->g[1] = 248;
            patern->g[2] = 254;
            patern->g[3] = 255;
            patern->g[4] = 251;
            patern->g[5] = 242;
            patern->g[6] = 229;
            patern->g[7] = 212;
            patern->g[8] = 192;
            patern->g[9] = 169;
            patern->g[10] = 145;
            patern->g[11] = 120;
            patern->g[12] = 95;
            patern->g[13] = 72;
            patern->g[14] = 51;
            patern->g[15] = 33;
            patern->g[16] = 18;
            patern->g[17] = 8;
            patern->g[18] = 2;
            patern->g[19] = 1;
            patern->g[20] = 5;
            patern->g[21] = 14;
            patern->g[22] = 27;
            patern->g[23] = 44;
            patern->g[24] = 65;
            patern->g[25] = 87;
            patern->g[26] = 111;
            patern->g[27] = 136;
            patern->g[28] = 161;
            patern->g[29] = 184;
            patern->g[30] = 205;
            patern->g[31] = g;
            patern->b[0] = 18;
            patern->b[1] = 33;
            patern->b[2] = 51;
            patern->b[3] = 72;
            patern->b[4] = 95;
            patern->b[5] = 120;
            patern->b[6] = 145;
            patern->b[7] = 169;
            patern->b[8] = 192;
            patern->b[9] = 212;
            patern->b[10] = 229;
            patern->b[11] = 242;
            patern->b[12] = 251;
            patern->b[13] = 255;
            patern->b[14] = 254;
            patern->b[15] = 248;
            patern->b[16] = 238;
            patern->b[17] = 223;
            patern->b[18] = 205;
            patern->b[19] = 184;
            patern->b[20] = 161;
            patern->b[21] = 136;
            patern->b[22] = 111;
            patern->b[23] = 87;
            patern->b[24] = 64;
            patern->b[25] = 44;
            patern->b[26] = 27;
            patern->b[27] = 14;
            patern->b[28] = 5;
            patern->b[29] = 1;
            patern->b[30] = 2;
            patern->b[31] = b;
        break;
        case 3:
            memset(&patern->r[0], r, 32); 
            memset(&patern->g[0], g, 32); 
            memset(&patern->b[0], b, 32);
        break;
        case 4:
            for (int i = 0; i < 32; i++) {
                patern->r[i] = customLed.r[i];
                patern->g[i] = customLed.g[i];
                patern->b[i] = customLed.b[i];
            }
        break;
    }

    if (!staticEND)
    {
        patern->r[31] = 0;
        patern->g[31] = 0;
        patern->b[31] = 0;
    }
}

/*
void restoreConfig(LED_MCU* led) {
    // Select from the installMenu list
}
*/

void writeDefault(FILE* file) {
    
    printf("Writing default configuration...\n");

    LED_MCU defaultNotifs[nbInstall];

    LED_MCU temp;

    // Unknown LED
    temp.ani[0] = 0x50;
    temp.ani[1] = 0x50;
    temp.ani[2] = 0xFF;
    temp.ani[3] = 0x00;
    for (int i = 0; i<32; i++)
    {
        temp.r[i] = 0x00;
        temp.g[i] = 0x00;
        temp.b[i] = 0x00;
    }
    defaultNotifs[0] = temp;

    // SpotPass LED
    temp.ani[0] = 0x50;
    temp.ani[1] = 0x3C;
    temp.ani[2] = 0xFF;
    temp.ani[3] = 0x00;
    for (int i = 0; i<30; i = i + 6)
    {
        temp.r[i] = 0x00;
        temp.r[i+1] = 0x00;
        temp.r[i+2] = 0x00;
        temp.r[i+3] = 0x00;
        temp.r[i+4] = 0x00;
        temp.r[i+5] = 0x00;

        temp.g[i] = 0x00;
        temp.g[i+1] = 0x00;
        temp.g[i+2] = 0x00;
        temp.g[i+3] = 0x00;
        temp.g[i+4] = 0x00;
        temp.g[i+5] = 0x00;

        temp.b[i] = 0x00;
        temp.b[i+1] = 0xFF;
        temp.b[i+2] = 0xF0;
        temp.b[i+3] = 0xDE;
        temp.b[i+4] = 0xBF;
        temp.b[i+5] = 0x8C;
    }
    temp.r[30] = 0x00;
    temp.g[30] = 0x00;
    temp.b[30] = 0x00;

    temp.r[31] = 0x00;
    temp.g[31] = 0x00;
    temp.b[31] = 0x66;

    defaultNotifs[1] = temp;

    // StreetPass LED
    temp.ani[0] = 0x50;
    temp.ani[1] = 0x50;
    temp.ani[2] = 0xFF;
    temp.ani[3] = 0x00;
    for (int i = 0; i<30; i = i + 6)
    {
        for (int j = 0; j < 4; j = j+2) {
            temp.r[i+j] = 0x00;
            temp.g[i+j] = 0x00;
            temp.b[i+j] = 0x00;

            temp.r[i+j+1] = 0x00;
            temp.g[i+j+1] = 0x80;
            temp.b[i+j+1] = 0x00;
        }
        
        for (int j = 4; j < 6; j++) {
            temp.r[i+j] = 0x00;
            temp.g[i+j] = 0x00;
            temp.b[i+j] = 0x00;
        }
    }
    temp.r[30] = 0x00;
    temp.g[30] = 0x00;
    temp.b[30] = 0x00;

    temp.r[31] = 0x00;
    temp.g[31] = 0x33;
    temp.b[31] = 0x00;

    defaultNotifs[2] = temp;

    // Friends ??? LED
    temp.ani[0] = 0x68;
    temp.ani[1] = 0x68;
    temp.ani[2] = 0xFF;
    temp.ani[3] = 0x00;
    for (int i = 0; i<24; i = i + 8)
    {
        for (int j = 0; j < 6; j = j + 2) {
            temp.r[i+j] = 0x00;
            temp.g[i+j] = 0x00;
            temp.b[i+j] = 0x00;

            temp.r[i+j+1] = 0xA9;
            temp.g[i+j+1] = 0x24;
            temp.b[i+j+1] = 0x00;
        }

        for (int j = 6; j<8; j++) {
            temp.r[i+j] = 0x00;
            temp.g[i+j] = 0x00;
            temp.b[i+j] = 0x00;
        }
    }
    for (int i = 24; i < 32; i++) {
        temp.r[i] = 0x00;
        temp.g[i] = 0x00;
        temp.b[i] = 0x00;
    }

    defaultNotifs[3] = temp;
    
    // Friends LED
    temp.ani[0] = 0x50;
    temp.ani[1] = 0x3C;
    temp.ani[2] = 0xFF;
    temp.ani[3] = 0x00;
    
    for (int i = 0; i < 30; i = i + 3) {
        temp.r[i] = 0x00;
        temp.g[i] = 0x00;
        temp.b[i] = 0x00;

        temp.r[i+1] = 0xA9;
        temp.g[i+1] = 0x24;
        temp.b[i+1] = 0x00;

        temp.r[i+2] = 0x7E;
        temp.g[i+2] = 0x1B;
        temp.b[i+2] = 0x00;
    }

    for (int i = 30; i<32; i++) {
        temp.r[i] = 0x00;
        temp.g[i] = 0x00;
        temp.b[i] = 0x00;
    }

    defaultNotifs[4] = temp;


    // https://zerosoft.zophar.net/ips.php for documentation of the IPS file format
    // HEADER (5 BYTES)
    fwrite("PATCH", 5, 1, file);

    // OFFSET (3 BYTES)
    fputc(0x00, file);
    fputc(0xA0, file);
    fputc(0xC8, file);

    // SIZE (2 BYTES)
    fputc(0x03, file);
    fputc(0xE8, file); //  1000 BYTES

    // PATCH
    for (int i = 0; i < nbInstall; i++) {

        // DATA (200 BYTES)

        // This part applies for when the console is in sleep mode (to have the LED holding the color)
        // 100 BYTES
        fwrite(&defaultNotifs[i], sizeof(defaultNotifs[i]), 1, file);

        // This part applies for when the console is awake (to avoid having the LED holding the color)
        defaultNotifs[i].r[31] = 0x00;
        defaultNotifs[i].g[31] = 0x00;
        defaultNotifs[i].b[31] = 0x00;

        // 100 BYTES
        fwrite(&defaultNotifs[i], sizeof(defaultNotifs[i]), 1, file);

    }
    // END OF PATCH
    // EOF MARKER (3 BYTES)
    fwrite("EOF", 3, 1, file);
}

void writepatch(LED note, int selectedType = 0)
{
    printf("Making directory...\n");
    mkdir("/CtrRGBPAT2", 0777);
    mkdir("/luma", 0777);
    mkdir("/luma/titles", 0777);
    mkdir("/luma/titles/0004013000003502", 0777);
    mkdir("/luma/sysmodules", 0777);

    DIR* dir = opendir("/luma/titles/0004013000003502"); // ! CHANGED IN LAST VERSION TO GO TO /luma/sysmodules !
    DIR* dir2 = opendir("/luma/sysmodules");
    DIR* dir3 = opendir("/CtrRGBPAT2");
    if (dir && dir2 && dir3)
    {
        // was copied/pasted from https://github.com/Pirater12/CustomRGBPattern/blob/master/main.c and then edited
        printf("Writing IPS patch file...\n");

        FILE *file;
        if (!(access("/CtrRGBPAT2/0004013000003502.ips", F_OK) != -1)) {
            if (debugMode) printf("No file detected. New file\n");
            file = fopen("/CtrRGBPAT2/0004013000003502.ips", "wb+");
            writeDefault(file);
        } else {
            struct stat stats;
            if (stat("/CtrRGBPAT2/0004013000003502.ips", &stats) == 0) {
                if (stats.st_size == 5 + (3 + 2) + 200*nbInstall + 3) { // PATCH + (OFFSET + SIZE) + DATA*NOTIFTYPES + EOF
                    if (debugMode) printf("Opening file normally\n");
                    file = fopen("/CtrRGBPAT2/0004013000003502.ips", "rb+");
                } else {
                    if (debugMode) printf("Old file detected. New file\n");
                    file = fopen("/CtrRGBPAT2/0004013000003502.ips", "wb+");
                    writeDefault(file);
                }
            } else {
                if (debugMode) printf("Cannot get file size somehow. New file\n");
                file = fopen("/CtrRGBPAT2/0004013000003502.ips", "wb+");
                writeDefault(file);
            }
        }
        
        LED notifAWAKE;
        LED notifSLEEP;
        createLED(&notifAWAKE, std::string(color_HEX), false, selectedpat);
        createLED(&notifSLEEP, std::string(color_HEX), true, selectedpat);
        
        // https://zerosoft.zophar.net/ips.php for documentation of the IPS file format

        // PATCH

        // ????? : 0x00A0C8 real address is 0x10A0C8 (it's literally just zeros)
        // SPOTPASS : 0x00A190 real address is 0x10A190
        // STREETPASS : 0x00A258 real address is 0x10A258
        // FRIENDS ??? : 0x00A320 real address is 0x10A320
        // FRIENDS : 0x00A3E8 real address is 0x10A3E8
        // LOW BATTERY : not found for now :(

        // Seek to the correct position in the patch
        fseek(file, 5 + (3 + 2) + 200*selectedType, 0);

        // DATA (200 BYTES)

        // This part applies for when the console is in sleep mode (to have the LED holding the color)
        // 4 BYTES
        fputc(ANIMDELAY, file);
        fputc(ANIMSMOOTH, file);
        fputc(LOOPBYTE, file);
        fputc(BLINKSPEED, file);
        // 96 BYTES
        if (staticend == 1 || staticend == 3) {
            fwrite(&notifSLEEP, sizeof(notifSLEEP), 1, file);
        } else {
            fwrite(&notifAWAKE, sizeof(notifAWAKE), 1, file);
        }

        // This part applies for when the console is awake (to avoid having the LED holding the color)
        // 4 BYTES
        fputc(ANIMDELAY, file);
        fputc(ANIMSMOOTH, file);
        fputc(LOOPBYTE, file);
        fputc(BLINKSPEED, file);
        // 96 BYTES
        if (staticend == 2 || staticend == 3) {
            fwrite(&notifSLEEP, sizeof(notifSLEEP), 1, file);
        } else {
            fwrite(&notifAWAKE, sizeof(notifAWAKE), 1, file);
        }

        // END OF PATCH

        // close file
        fclose(file);

        // Copy files to activate the patch
        fcopy("/CtrRGBPAT2/0004013000003502.ips", "/luma/titles/0004013000003502/code.ips");
        fcopy("/CtrRGBPAT2/0004013000003502.ips", "/luma/sysmodules/0004013000003502.ips");

        // Check if our files were written
        if( access("/luma/sysmodules/0004013000003502.ips", F_OK) != -1 && access("/luma/titles/0004013000003502/code.ips", F_OK) != -1) 
        {
            printf("Success !\n");
        } 
        else 
        {
            printf("Failed !\n");
        }

        closedir(dir);
        closedir(dir2);
        closedir(dir3);
    }
    else if (ENOENT == errno)
    {
        printf("Directory failed...\n");
    }
}

// Interesting fact : the low battery pattern overwrites everything
void ptmsysmSetInfoLedPattern(LED_MCU pattern)
{
    Handle serviceHandle = 0;
    Result result = srvGetServiceHandle(&serviceHandle, "ptm:sysm");
    if (result != 0) 
    {
        printf("Failed to get service ptm:sysm :(\n");
        return;
    }

    u32* ipc = getThreadCommandBuffer();
    ipc[0] = 0x8010640;
    memcpy(&ipc[1], &pattern, 0x64);
    svcSendSyncRequest(serviceHandle);
    svcCloseHandle(serviceHandle);
}

void test_LED(LED patern)
{
    LED_MCU MCU_PAT;

    for (int i = 0; i<32; i++)
    {
        MCU_PAT.r[i] = patern.r[i];
        MCU_PAT.g[i] = patern.g[i];
        MCU_PAT.b[i] = patern.b[i];
        if (debugMode) {
            printf("(%u, %u, %u)\n", patern.r[i], patern.g[i], patern.b[i]);
        }
    }

    if (debugMode) {
        printf("First color = (%u, %u, %u)\n", patern.r[0], patern.g[0], patern.b[0]);
        printf("Last color = (%u, %u, %u)\n", patern.r[31], patern.g[31], patern.b[31]);
    }

    MCU_PAT.ani[0] = ANIMDELAY;
    MCU_PAT.ani[1] = ANIMSMOOTH;
    MCU_PAT.ani[2] = LOOPBYTE;
    MCU_PAT.ani[3] = BLINKSPEED;

    printf("Testing custom pattern...\n");

    ptmsysmSetInfoLedPattern(MCU_PAT);
}

// when done we want LUMA to reload so it can patch with our ips patches
// https://www.3dbrew.org/wiki/PTMSYSM:LaunchFIRMRebootSystem
void PTM_RebootAsync() 
{
    ptmSysmInit();

    Handle serviceHandle = 0;
    Result result = srvGetServiceHandle(&serviceHandle, "ptm:sysm");
    if (result != 0) {
        return;
    }

    u32 *commandBuffer = getThreadCommandBuffer();
    commandBuffer[0] = 0x04090080;
    commandBuffer[1] = 0x00000000;
    commandBuffer[2] = 0x00000000;

    svcSendSyncRequest(serviceHandle);
    svcCloseHandle(serviceHandle);

    ptmSysmExit();
}

void listMenu(int dispOffset)
{
    int colr; //= (int)strtol(color_HEX.substr(0, 2), NULL, 16);
    int colg; //= (int)strtol(color_HEX.substr(2, 2), NULL, 16);
    int colb; //= (int)strtol(color_HEX.substr(4, 2), NULL, 16);
    intRGB(std::string(color_HEX), &colr, &colg, &colb);
    iprintf("\x1b[2J");
    printf("\x1b[0;0H\x1b[30;0m");
    printf("===== Ctr\e[31mR\e[32mG\e[34mB\e[0mPAT2 ===== %s\n", debugMode ? "[DEBUG MODE ACTIVATED]" : " ");
    switch (currMenu) {
        case 0:
            for (int i = 0; i < nbMain; i++) 
            {
                if (i == 0 && selectedpat == 4) {
                    if (selected == 0)
                        printf("\x1b[47;30m* Set custom notification pattern\x1b[30;0m\n");
                    else
                        printf("\x1b[40;33m* Set custom notification pattern\x1b[30;0m\n");
                } else {
                    if (i == selected)
                        printf("\x1b[47;30m* %s\x1b[30;0m\n", menu[i].c_str());
                    else
                        printf("\x1b[30;0m* %s\n", menu[i].c_str());
                }
            }
            printf("======================\n");
            printf("COLOR  : %s \e[48;2;%d;%d;%dm  \e[0m\n", color_HEX, colr, colg, colb);
            printf("PATTERN : %s\n", patterns[selectedpat].c_str());
            printf("ANIMATION DELAY : %X\n", ANIMDELAY);
            printf("ANIMATION SMOOTHNESS : %X\n", ANIMSMOOTH);
            printf("LOOP DELAY : %X\n", LOOPBYTE);
            printf("STATIC END : %s\n", staticEnds[staticend].c_str());
            printf("ENABLED : %s\e[0m\n", (enabled ? "\e[32mYES" : "\e[31mNO"));
            printf("======================\n");
        break;
        case 1:
            printf("Select where to install the custom pattern :\n");
            for (int i = 0; i < nbInstall; i++) 
            {
                if (i == selected)
                    printf("\x1b[47;30m* %s\x1b[30;0m\n", installMenu[i].c_str());
                else
                    printf("\x1b[30;0m* %s\n", installMenu[i].c_str());
            }
            printf("======================\n");
            printf("(B) to cancel\n");
        break;
        case 2:
            printf("\e[40;36m(L) to copy, (R) to paste : \e[0m%s\n", color_copy);
            for (int i = dispOffset; i < dispOffset+27; i++) {
                if (i == selected)
                    printf("\x1b[47;30m%d: %s%X%s%X%s%X \e[48;2;%d;%d;%dm \e[0m%s\x1b[30;0m\n", i+1, (customLed.r[i] < 16 ? "0" : ""), customLed.r[i], (customLed.g[i] < 16 ? "0" : ""), customLed.g[i], (customLed.b[i] < 16 ? "0" : ""), customLed.b[i], customLed.r[i], customLed.g[i], customLed.b[i], (i == 31 ? "(may be 0 depending on static ending)" : (i == 0 ? "(seems to not do anything)" : "")));
                else
                    printf("\x1b[37;40m%d: %s%X%s%X%s%X \e[48;2;%d;%d;%dm \e[0m%s\n", i+1, (customLed.r[i] < 16 ? "0" : ""), customLed.r[i], (customLed.g[i] < 16 ? "0" : ""), customLed.g[i], (customLed.b[i] < 16 ? "0" : ""), customLed.b[i], customLed.r[i], customLed.g[i], customLed.b[i], (i == 31 ? "(may be 0 depending on static ending)" : (i == 0 ? "(seems to not do anything)" : "")));
            }
        break;
        default:
            printf("err\n");
        break;
    }
}

// 32: 000000   (may be 0 depending on static ending)
// This is not the final version, i will include more

int main(int argc, char **argv) 
{
    gfxInitDefault();
    aptInit();
    aptSetHomeAllowed(false);
    srvInit();
	
    // Init console for text output
    consoleInit(GFX_TOP, NULL);
    
    int r, g, b;

    for (int i = 0; i < 32; i++) {
        customLed.r[i] = 0x00;
        customLed.g[i] = 0x00;
        customLed.b[i] = 0x00;
    }

    enabled = (access("/luma/sysmodules/0004013000003502.ips", F_OK) != -1 && access("/luma/titles/0004013000003502/code.ips", F_OK) != -1);

    selected = 0;
    int selOffset = 0;

    bool infoRead = false;  
    printf("Welcome to Ctr\e[31mR\e[32mG\e[34mB\e[0mPAT2 !\n\nThis is a tool allowing you to change the color\nand pattern of the LED when you receive\na notification.\n\nYou can select for which type of notifications\nyou want it to apply from the install menu.\n\nThis is not the final version, i will include more\nthings in future updates.\n\n\nPress (A) to continue\n");
    //listMenu();

    while (aptMainLoop()) 
    {
        gfxSwapBuffers();
        gfxFlushBuffers();
		gspWaitForVBlank();

        hidScanInput();

        u32 kDown = hidKeysDown();

        if (aptCheckHomePressRejected()) {
            infoRead = true;
            listMenu(selOffset);
            printf("Cannot return to the HOME Menu. START to reboot\n");
        }

        if (kDown & KEY_START)
        {
            printf("Rebooting the console...\n");
            PTM_RebootAsync();
            //break; // needs changes, crashes the console
        }

        if (kDown & KEY_Y) {
            debugMode = !debugMode;
            infoRead = true;
            listMenu(selOffset);
        }

        if ((kDown & KEY_X) && debugMode) {
            if (access("/CtrRGBPAT2/0004013000003502.ips", F_OK) != -1) {
                FILE *file = fopen("/CtrRGBPAT2/0004013000003502.ips", "wb+");
                writeDefault(file);
                fclose(file);
                listMenu(selOffset);
                printf("Configuration restored to default\n");
            } else {
                listMenu(selOffset);
                printf("No previous configuration detected\n");
            }
        }

        if (kDown & KEY_DDOWN)
        {
            selected=selected+1;
            if (selected-selOffset > 26)
                selOffset = selOffset+1;
            if (selected>nbCmd-1) {
                selected = 0;
                selOffset = 0;
            }
            infoRead = true;
            listMenu(selOffset);
        }

        if (kDown & KEY_DUP)
        {
            selected=selected-1;
            if (selected-selOffset < 0)
                selOffset = selOffset-1;
            if (selected<0) {
                selected = nbCmd-1;
                selOffset = nbCmd-27;
            }
            infoRead = true;
            listMenu(selOffset);
        }

        if ((kDown & KEY_L) && currMenu == 2) {
            sprintf((char *)color_copy, "%s%X%s%X%s%X", (customLed.r[selected] < 16 ? "0" : ""), customLed.r[selected], (customLed.g[selected] < 16 ? "0" : ""), customLed.g[selected], (customLed.b[selected] < 16 ? "0" : ""), customLed.b[selected]);
            listMenu(selOffset);
        }

        if ((kDown & KEY_R) && currMenu == 2) {
            intRGB(std::string(color_copy), &r, &g, &b);
            customLed.r[selected] = r;
            customLed.g[selected] = g;
            customLed.b[selected] = b;
            listMenu(selOffset);
        }

        if (kDown & KEY_B) {
            currMenu = 0;
            nbCmd = nbMain;
            selected = 0;
            selOffset = 0;
            infoRead = true;
            listMenu(selOffset);
        }

        if (kDown & KEY_A)
        {
            if (infoRead) {
                switch(currMenu) {
                    case 0:
                        switch(selected)
                        {
                            case 0:
                                if (selectedpat == 4) {
                                    currMenu = 2;
                                    nbCmd = nbCustom;
                                    selected = 0;
                                    selOffset = 0;
                                    listMenu(selOffset);
                                } else {
                                    hexaInput((char *)color_HEX, 6, "LED RGB color (in HEX)");
                                    listMenu(selOffset);
                                    if (debugMode) {
                                        for (int i = 0; i < 6; i++) {
                                            printf("Char %d is %d\n", i, (int)keybInput[i]);
                                        }
                                    }
                                }
                            break;
                            case 1:
                                selectedpat=selectedpat+1;
                                if (selectedpat>PATS-1)
                                    selectedpat = 0;
                                listMenu(selOffset);
                            break;
                            case 2:
                                hexaInput((char *)anim_speed, 2, "Animation speed (delay)");
                                ANIMDELAY = (uint8_t)strtol(anim_speed, NULL, 16);
                                listMenu(selOffset);
                            break;
                            case 3:
                                hexaInput((char *)anim_smooth, 2, "Animation smoothness");
                                ANIMSMOOTH = (uint8_t)strtol(anim_smooth, NULL, 16);
                                listMenu(selOffset);
                            break;
                            case 4:
                                hexaInput((char *)anim_loop_delay, 2, "Animation loop delay");
                                LOOPBYTE = (uint8_t)strtol(anim_loop_delay, NULL, 16);
                                listMenu(selOffset);
                            break;
                            case 5:
                                staticend = (staticend + 1) % 4;
                                listMenu(selOffset);
                            break;
                            case 6:
                                LED test_notification;
                                listMenu(selOffset);
                                createLED(&test_notification, std::string(color_HEX), (staticend > 1), selectedpat);
                                test_LED(test_notification);
                            break;
                            case 7:
                                //LED notification;
                                //createLED(&notification, std::string(color_HEX), staticend, selectedpat);
                                //enabled = true;
                                currMenu = 1;
                                nbCmd = nbInstall;
                                selected = 0;
                                selOffset = 0;
                                listMenu(selOffset);
                                //writepatch(notification);
                            break;
                            case 8:
                                printf("Toggling state, please wait...\n");
                                // copyFile code and stuff
                                mkdir("/CtrRGBPAT2", 0777);
                                if (enabled) {
                                    remove("/luma/titles/0004013000003502/code.ips");
                                    remove("/luma/sysmodules/0004013000003502.ips");
                                } else if (access("/CtrRGBPAT2/0004013000003502.ips", F_OK) != -1) {
                                    fcopy("/CtrRGBPAT2/0004013000003502.ips", "/luma/titles/0004013000003502/code.ips");
                                    fcopy("/CtrRGBPAT2/0004013000003502.ips", "/luma/sysmodules/0004013000003502.ips");
                                } else {
                                    selected = 7;
                                    listMenu(selOffset);
                                    printf("Unable to toggle state. Write IPS patch first\n");
                                    break;
                                }
                                enabled = !enabled;
                                listMenu(selOffset);
                                printf("Patch is now %s\n", (enabled ? "enabled" : "disabled"));
                            break;
                            case 9:
                                ptmSysmInit();
                                PTMSYSM_ShutdownAsync(0);
                                ptmSysmExit();
                            break;
                            default:
                                printf("err\n");
                            break;
                        }
                    break;
                    case 1:
                        LED notification;
                        //createLED(&notification, std::string(color_HEX), staticend, selectedpat);
                        enabled = true;
                        currMenu = 0;
                        nbCmd = nbMain;
                        listMenu(selOffset);
                        writepatch(notification, selected);
                    break;
                    case 2:
                    {
                        char hexCustom[10]; // 10 to avoid gcc compiling warnings
                        r = customLed.r[selected];
                        g = customLed.g[selected];
                        b = customLed.b[selected];
                        sprintf((char *)hexCustom, "%s%X%s%X%s%X", (customLed.r[selected] < 16 ? "0" : ""), customLed.r[selected], (customLed.g[selected] < 16 ? "0" : ""), customLed.g[selected], (customLed.b[selected] < 16 ? "0" : ""), customLed.b[selected]);
                        hexaInput((char *)hexCustom, 6, "LED RGB color (in HEX)");
                        intRGB(std::string(hexCustom), &r, &g, &b);
                        customLed.r[selected] = r;
                        customLed.g[selected] = g;
                        customLed.b[selected] = b;
                        listMenu(selOffset);
                    break;
                    }
                    default:
                        printf("err\n");
                    break;
                }
                
            } else {
                infoRead = true;
                listMenu(selOffset);
            }
        }
    }

    srvExit();
    aptExit();
    gfxExit();
    return 0;
}