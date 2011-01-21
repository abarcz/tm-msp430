#include "msp430x14x.h"
#include "intrinsics.h"
#include "math.h"
#include "string.h"

// sygnaly sterujace LCD
#define CTRL_E            0x01
#define CTRL_RS           0x02

// komendy sterujace LCD
/*
#define LCD_FN_SET        0x38
#define LCD_DISP_ON_OFF   0x0C
#define LCD_ENTRY_MD_SET  0x06
#define LCD_CLEAR         0x01
*/

#define DCO_FQ 1000000      // czestotliwosc DCO (glowny zegar uk³adu)
#define ELIM_TIME_DIV 500   // 1/ELIM_TIME_DIV = czas eliminacji drgan
#define TIMER_FQ 32768      // ACLK podlaczone do timer A

// ile razy nalezy sprawdzic wartosc WE podczas eliminacji drgan
const int ELIM_CHECKS = DCO_FQ / ELIM_TIME_DIV;

// tablica stacji metra warszawskiego
char *stations[] = {"Kabaty", "Natolin", "Imielin", "Stoklosy", 
    "Ursynow", "Sluzew", "Wilanowska", "Wierzbno", "Raclawicka", 
    "Pola Mokotowskie", "Politechnika", "Centrum", "Swietokrzyska", 
    "Ratusz Arsenal", "Dworzec Gdanski", "Plac Wilsona",
    "Marymont", "Slodowiec", "Stare Bielany", "Wawrzyszew", 
    "Mlociny"};

const int g_stations_num = 21;  // ilosc stacji        
int g_stations_index = 0;       // indeks aktualnie wybranej stacji
int g_curr_input = 0;           // stan portu P2 (WE, przyciski na b0 i b1)

/* znaczniki przekazywane miedzy ISR a petla glowna
b0 = przycisk dolny zglasza przerwanie (cofnij sie o stacjê)
b1 = przycisk górny zglasza przerwanie (naprzód o jedn¹ stacjê)
*/
int g_flags = 0;

//Obsluga wyswietlacza
void strobe_e();            // puszcza impuls e na wyswietlacz

// :TODO: LEGACY FUNCTIONS
inline void delayUs(unsigned int num);
inline void delayMs(unsigned int num);

inline void delay_us(int us_num); // :TODO: ma zastapic powyzsze
void display_string(char *str);   // wyswietla napis na LCD

int main( void )
{
  // Stop watchdog timer to prevent time out reset
  WDTCTL = WDTPW + WDTHOLD;
  
  // przyciski
  P2IES = 0xFF;             // przerwanie wyzwalane 1->0
  P2IE = BIT0 + BIT1;       // przyciski wyboru stacji
  
  // timer A
  TACTL |= TASSEL_1;
  TACCTL0 |= CCIE;
  TACCR0 = 5;
  
  int i = 0;
  int elim_state = 0;       // stan eliminacji drgan : 
                            //    0 - czekamy na 0, 1 - czekamy na 1
  
  // Wyjscie danych na LCD
  P1DIR = 0xFF;
  P1OUT = 0x00;
  
  // Wyjscie sterujace LCD
  // pin 1 = wyjscie strobujace E
  // pin 2 = wyjscie RS     // :TODO: przelaczenie pinow tak zeby sobie odpowiadaly
  P3DIR = 0xFF;
  P3OUT = 0x00;
  
  // Uruchomienie wyswietlacza
  P1OUT = 0x38;           //Function set 
  strobe_e();
  P1OUT = 0x0C;           //Display on/off control 
  strobe_e();
  P1OUT = 0x06;           //Entry mode set 
  strobe_e();
  //delay_us(1000);
  delayUs(40);
  P1OUT = 0x01;           //Clear 
  strobe_e();
  //delay_us(3000);
  delayMs(3);
  
  display_string(stations[g_stations_index]);
  
  //petla glowna programu
  while(1)
  {
    //zasypiamy
    _BIS_SR(LPM0_bits + GIE);
    __disable_interrupt();
    
    // eliminacja drgan - wcisniecie przycisku
    if ((elim_state == 0) && ((g_flags & BIT0)|| (g_flags & BIT1)))
    {
      i = 0;
      while ((i < ELIM_CHECKS) && (g_curr_input == P2IN))
      {
        i++;  
      }
      if (i == ELIM_CHECKS) // jesli rzeczywiscie wcisniety
      {
        if (g_flags & BIT1)
          g_stations_index = (g_stations_index + 1) % g_stations_num;  
        else if (g_flags & BIT0)
          g_stations_index = (g_stations_index == 0) ? (g_stations_num - 1): g_stations_index - 1;
        display_string(stations[g_stations_index]);
        // SEKCJA KRYTYCZNA!
        P2IES = 0;          // przelacz zbocze, ktorym wyzwalane jest INT
        P2IFG = 0;          // wyczysc ew. smieci
        // koniec SK
        elim_state = 1;
      }
      g_flags = 0;
      P2IE = BIT0 + BIT1;   // przyciski wyboru stacji
    }
    else if ((elim_state == 1) && ((g_flags & BIT0)|| (g_flags & BIT1)))
    {
      i = 0;
      while ((i < ELIM_CHECKS) && (g_curr_input == P2IN))
      {
        i++;  
      }
      if (i == ELIM_CHECKS) // jesli rzeczywiscie wycisniety
      {
        // SEKCJA KRYTYCZNA!
        P2IES = 0xFF;       // przelacz zbocze, ktorym wyzwalane jest INT
        P2IFG = 0;          // wyczysc ew. smieci
        // koniec SK
        elim_state = 0;
      }
      g_flags = 0;
      P2IE = BIT0 + BIT1;   // przyciski wyboru stacji
    }
    __enable_interrupt();
  }
  return 0;
}

// fcja wysylajaca impuls e na wyswietlacz
inline void strobe_e()
{
  P3OUT |= CTRL_E;
  P3OUT &= ~CTRL_E;
}

inline void delayUs(unsigned int num)
{
  while(num--);
}

inline void delayMs(unsigned int num)
{
  while(num--)
    delayUs(80);
}

// czeka us_num mikrosekund (min 31 dla ACLK)
inline void delay_us (int us_num)
{
  int ticks = (int)ceil((double)(us_num * TIMER_FQ) / 1000000);
  TACCR0 = ticks;
  TAR = 0;
  TACTL |= MC_1;            // wlacza timer. przerwania musza byc wl.
  _BIS_SR(LPM0_bits + GIE); // zasnij w oczekiwaniu na timer A
}

// wyswietla napis str na ekranie LCD
void display_string(char *str)
{
  int len = strlen(str);
  int i;
  P3OUT = 0x00;      
  P1OUT = 0x01;             // clear
  strobe_e();
  delayMs(3);
  //delay_us(3000);
  for (i = 0; i < len; i++)
  {
    P1OUT = str[i];
    P3OUT = CTRL_RS;
    strobe_e();
  }
}

// przerwanie przyciskow - samoblokujace
#pragma vector=PORT2_VECTOR
__interrupt void Port1 (void)
{
  g_curr_input = P2IN;      // przepisz wartosc wejsc
  g_flags |= P2IFG & 0x0003;  // czy przyciski 0 lub 1 zglaszaja przerwanie?
  P2IFG = 0;                // czysc znaczniki przerwan (:TODO: potrzebne?)
  P2IE = 0;                 // zablokuj wlasne przerwania
  _BIS_SR(LPM0_EXIT + GIE);
}

// przerwanie zegara - samowylaczajace
#pragma vector=TIMERA0_VECTOR
__interrupt void TimerA (void)
{
  TACTL &= 0xFFCF;          // wylacz timer
  _BIS_SR(LPM0_EXIT + GIE);
}
