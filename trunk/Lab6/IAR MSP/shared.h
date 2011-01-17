
/****************** wspolne definicje przerwan i aplikacji ******************/

/* bajty pisane na D7..D0 wyswietlacza LCD */
#define DX_FN_SET         0x038
#define DX_DISP_ON_OFF    0x00C
#define DX_ENTRY_MODE_SET 0x006
#define DX_CLEAR          0x001
#define DX_RETURN_HOME    0x002

/* maski bajtu CTRL */
#define CTRL_BLT  0x080     // podswietlenie
#define CTRL_RS   0x040     // wybór rejestru
#define CTRL_E    0x010     // strob transmisji danych