/*
UWAGA:
Funkcja delay_us() moze byc uzywana tylko gdy przerwanie timera A jest wlaczone,
a przerwania innych ukladow sa wylaczone - inaczej mozemy stracic informacje
o innych przerwaniach.
Rozwiazaniem jest informowanie petli glownej o wystapieniu innych przerwan
poprzez ustawianie odpowiednich bitow w zmiennej globalnej (tu: g_flags).
Timer A nie moze byc uzywany w zadnym innym celu (mozna do innych celow uzywac 
Timera B).
Funkcja delay_us() gwarantuje, ze odczeka czas trwania >= zadany czas
w mikrosekundach.
Rozwiazanie nie jest doskonale, tworcy nie ponosza odpowiedzialnosci za moralne
ani jakiekolwiek inne skutki dzialania oprogramowania :)
*/

#include "msp430x16x.h"
#include "math.h"
#include "string.h"

// sygnaly sterujace LCD
#define CTRL_E            0x01
#define CTRL_RS           0x04

// komendy sterujace LCD
#define LCD_CLEAR         0x01

#define DCO_FQ 1000000            // czestotliwosc DCO (glowny zegar uk³adu)
#define ELIM_TIME_DIV 500         // 1/ELIM_TIME_DIV = czas eliminacji drgan
#define TIMER_FQ 32768            // ACLK podlaczone do timer A

// Zmienna przechowujaca informacje o wystapieniu przerwan
// bit0 - wystapilo przerwanie ...
// bit1 - wystapilo przerwanie ...
// itd.
int g_flags;

inline void strobe_e();           // wysyla impuls E na wyswietlacz
inline void delay_us(int us_num); // czeka us_num mikrosekund (min 31 dla ACLK)
void display_string(char *str);   // wyswietla napis na LCD

int main( void )
{
  // Stop watchdog timer to prevent time out reset
  WDTCTL = WDTPW + WDTHOLD;     // wylacz watchdoga

  // timer A
  TACTL |= TASSEL_1;
  TACCTL0 |= CCIE;
  
  // Wyjscie danych na LCD
  P1DIR = 0xFF;
  P1OUT = 0x00;
  
  // Wyjscie sterujace LCD
  // pin 1 = wyjscie strobujace E
  // pin 2 = wyjscie RS     
  P3DIR = 0xFF;
  P3OUT = 0x00;
  
  // Wyzerowanie zmiennej przechowujacej znaczniki przychodzacych przerwan
  g_flags = 0;
  
  // Uruchomienie wyswietlacza
  delay_us(30000);            // Bez tego po resecie moze nie dzialac  
  P1OUT = 0x38;               // Function set (8bit, 1 line)
  strobe_e();
  P1OUT = 0x0C;               // Display on/off control 
  strobe_e();
  P1OUT = 0x06;               // Entry mode set (increments DDRAM address)
  strobe_e();
  delay_us(1000);
  P1OUT = LCD_CLEAR;         
  strobe_e();
  delay_us(3000);
  // koniec uruchamiania wyswietlacza
  
  display_string("Hello TM Lab6!"); 
  
  while(1)
  {
    /**** spr, czy nie przyszlo kolejne przerwanie w trakcie petli glownej ****/
    __disable_interrupt();
      if (g_flags != 0) // jesli ustawiony dowolny bit, to bylo przerwanie
      {
        __enable_interrupt();
        goto mainloop_internal;
      }
    //else : zasypiajac odblokujemy przerwania
    //zasypiamy
    _BIS_SR(LPM0_bits + GIE);
    __no_operation();
    
mainloop_internal:
    // ciag dalszy kodu, mogacy zawierac np. display_string();
    display_string("...");
  }
  return 0;
}

// fcja wysylajaca impuls e na wyswietlacz
inline void strobe_e()
{
  P3OUT |= CTRL_E;
  P3OUT &= ~CTRL_E;
}

// czeka us_num mikrosekund (min 31 dla ACLK)
inline void delay_us(int us_num)
{
  istate_t istate;
  TACCR0 = (int)ceil((double)(us_num * TIMER_FQ) / 1000000);
  TAR = 0;
  TACTL |= MC_1;              // wlacza timer. przerwania timera musza byc wl.
  istate = __get_interrupt_state(); // zachowanie stanu przerwan globalnych
  do
  {
    _BIS_SR(LPM0_bits + GIE); // zasnij w oczekiwaniu na timer A
    __disable_interrupt();    // zablokuj na czas sprawdzenia warunku while
  }
  while (TACTL & MC_1);       // dopiero gdy timer A jest wylaczony,
                              // wiemy ze nastapilo przerwanie timera A.
  __set_interrupt_state(istate);  // przywrocenie poprzedniego stanu przerwan
}

// wyswietla napis str na ekranie LCD
void display_string(char *str)
{
  int len = strlen(str);
  int i;
  P3OUT = 0x00;      
  P1OUT = LCD_CLEAR;         
  strobe_e();
  delay_us(3000);
  for (i = 0; i < len; i++)
  {
    P1OUT = str[i];
    P3OUT = CTRL_RS;
    strobe_e();
  }
}

// przerwanie zegara - samowylaczajace
#pragma vector=TIMERA0_VECTOR
__interrupt void TimerA (void)
{
  TACTL &= 0xFFCF;            // wylacz timer
  _BIS_SR(LPM0_EXIT);
}
