
/****************** wspolne definicje przerwan i aplikacji ******************/

/* bajty pisane na D7..D0 wyswietlacza LCD */
#define DX_FN_SET         0x30
#define DX_DISP_ON_OFF    0x0E
#define DX_ENTRY_MODE_SET 0x06
#define DX_CLEAR          0x01
#define DX_RETURN_HOME    0x02

/* maski bajtu CTRL */
#define CTRL_BLT  0x08     // podswietlenie
#define CTRL_RS   0x04     // wybór rejestru
#define CTRL_E    0x01     // strob transmisji danych
#define CTRL_NE   0xFE     


#define MASK_E 0x01
#define MASK_RS 0x02

 komendy LCD
