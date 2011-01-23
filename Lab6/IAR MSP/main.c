#include "msp430x16x.h"
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
*/
#define LCD_CLEAR         0x01

// adresy segmentu zapisu licznika
#define SEGMENT_START   0xFE00
#define SEGMENT_END     0xFFFF

// adresy segmentu zapisu sumy kontrolnej
#define CS_SEG_START    0xFC00
#define CS_SEG_END      0xFDFF

/*//dualBIOS
// adresy segmentu zapisu licznika2
#define SEGMENT_START2  0xFA00
#define SEGMENT_END2    0xFBFF
// adresy segmentu zapisu sumy kontrolnej2
#define CS_SEG_START2   0xF800
#define CS_SEG_END2     0xF9FF
*/

// stany ukladu
#define S_NORMAL 0                // stan normalny
#define S_WDOG_RESET 1            // uklad zostal zresetowany przez wdoga
#define S_CTRL_SUM_ERROR 2        // wystapil blad sumy kontrolnej

#define DCO_FQ 1000000            // czestotliwosc DCO (glowny zegar uk³adu)
#define ELIM_TIME_DIV 500         // 1/ELIM_TIME_DIV = czas eliminacji drgan
#define TIMER_FQ 32768            // ACLK podlaczone do timer A

int g_curr_input = 0;             // stan portu P2 (WE, przyciski na b0 i b1)

/* znaczniki przekazywane miedzy ISR a petla glowna
b0 = przycisk dolny zglasza przerwanie (cofnij sie o stacjê)
b1 = przycisk górny zglasza przerwanie (naprzód o jedn¹ stacjê)
*/
int g_flags = 0;

//Obsluga wyswietlacza
void strobe_e();                  // puszcza impuls e na wyswietlacz

// :TODO: LEGACY FUNCTIONS
void delayUs(unsigned int num);
void delayMs(unsigned int num);

inline void delay_us(int us_num); // :TODO: ma zastapic powyzsze
void display_string(char *str);   // wyswietla napis na LCD

// zapis do pamieci pod adres mem_ptr + 2, w segmencie ograniczonym przez 
// segment_end, przesuwa wskaznik mem_ptr
void save_to_memory(unsigned int int_to_save, unsigned int **mem_ptr,
                    unsigned int segment_start, unsigned int segment_end);

// czyszczenie segmentu wskazanego przez mem_ptr
void clear_memory();

// wyszukiwanie adresu do zapisu w pamieci w segmencie seg_start - seg_end
// zwraca adres ostatniej zapisanej wartosci
// lub adres seg_start - 2, jesli nic nie znaleziono
unsigned int* find_address(unsigned int seg_start, unsigned int seg_end);

// zwraca sume kontrolna dla wskazanego segmentu = zwykla suma
// miesci sie w zakresie uint dla naszej ilosci stacji
unsigned int calculate_ctrl_sum(unsigned int seg_start, unsigned int seg_end);

int main( void )
{
  // Stop watchdog timer to prevent time out reset
  WDTCTL = WDTPW + WDTHOLD;     // wylacz watchdoga
  //WDTCTL = WDTPW;         //:TODO: wlacz watchdoga
  
  // ile razy nalezy sprawdzic wartosc WE podczas eliminacji drgan
  const int ELIM_CHECKS = DCO_FQ / ELIM_TIME_DIV;
  
  // tablica stacji metra warszawskiego
  char *stations[] = {"Kabaty", "Natolin", "Imielin", "Stoklosy", 
      "Ursynow", "Sluzew", "Wilanowska", "Wierzbno", "Raclawicka", 
      "Pola Mokotowskie", "Politechnika", "Centrum", "Swietokrzyska", 
      "Ratusz Arsenal", "Dworzec Gdanski", "Plac Wilsona",
      "Marymont", "Slodowiec", "Stare Bielany", "Wawrzyszew", 
      "Mlociny"};
  
  const int stations_num = 21;// ilosc stacji   
  
  // komunikat oznajmiajacy ze watchdog nas zresetowal
  char *wdog_error = "RST by watchdog";
  // komunikat bledu sumy kontrolnej
  char *ctrl_sum_error = "Ctrl sum error";
  
  int i = 0;
  int elim_state = 0;         // stan eliminacji drgan : 
                              //    0 - czekamy na 0, 1 - czekamy na 1
  
  int state = S_NORMAL;       // stan ukladu - normalny
  int stations_index = 0;     // indeks aktualnie wybranej stacji
  unsigned int control_sum = 0; // suma kontrolna segmentu licznikow
  
  // wskaznik do pamieci - adres licznika stacji
  unsigned int *mem_ptr = 0;
  // adres sumy kontrolnej segmentu licznika stacji
  unsigned int *cs_mem_ptr = 0;
  // czy powiodlo sie policzenie sumy kontrolnej?
  int ctrl_sum_check1_succeeded = 0;
  
  /*//dualBIOS
   // wskaznik do pamieci - adres licznika stacji2
  unsigned int *mem_ptr2 = 0;
  // adres sumy kontrolnej segmentu licznika stacji2
  unsigned int *cs_mem_ptr2 = 0;
  // czy powiodlo sie policzenie sumy kontrolnej2?
  int ctrl_sum_check2_succeeded = 0;
  */
  
  // przyciski
  P2IES = 0xFF;               // przerwanie wyzwalane 1->0
  P2IE = BIT0 + BIT1;         // przyciski wyboru stacji
  
  // timer A
  TACTL |= TASSEL_1;
  TACCTL0 |= CCIE;
  TACCR0 = 5;
  
  // Wyjscie danych na LCD
  P1DIR = 0xFF;
  P1OUT = 0x00;
  
  // Wyjscie sterujace LCD
  // pin 1 = wyjscie strobujace E
  // pin 2 = wyjscie RS       // :TODO: przelaczenie pinow tak zeby sobie odpowiadaly
  P3DIR = 0xFF;
  P3OUT = 0x00;
  
  // Uruchomienie wyswietlacza
  P1OUT = 0x38;               //Function set 
  strobe_e();
  P1OUT = 0x0C;               //Display on/off control 
  strobe_e();
  P1OUT = 0x06;               //Entry mode set 
  strobe_e();
  //delay_us(1000);
  delayUs(40);
  P1OUT = LCD_CLEAR;         
  strobe_e();
  //delay_us(3000);
  delayMs(3);
  
  // znajdz ostatnia legalna wartosc licznika w pamieci
  mem_ptr = find_address(SEGMENT_START, SEGMENT_END);  
  if (mem_ptr >= (unsigned int*)SEGMENT_START)
  {
    stations_index = *mem_ptr;  // pobierz z pamieci wartosc licznika,
                                // o ile istnieje (wpp 0)
  }
  // znajdz ostatnia legalna wartosc sumy kontrolnej w pamieci
  cs_mem_ptr = find_address(CS_SEG_START, CS_SEG_END);
  if (cs_mem_ptr >= (unsigned int*)CS_SEG_START)
  {
    control_sum = *cs_mem_ptr;  // pobierz z pamieci wartosc sumy kontrolnej,
                                // o ile istnieje (wpp 0)
  }
  // sprawdz sume kontrolna1
  if ((CS_SEG_END - (unsigned int)cs_mem_ptr) ==
        (SEGMENT_END - (unsigned int)mem_ptr))
  {
    if (control_sum == calculate_ctrl_sum(SEGMENT_START, SEGMENT_END))
    {
      ctrl_sum_check1_succeeded = 1;  // spr sumy kontrolnej - sukces!
    }
  }
  
  // przy braku dualBIOSa jesli suma kontrolna sie nie zgadza, zresetuj licznik
  if (ctrl_sum_check1_succeeded == 0)
  {
    state = S_CTRL_SUM_ERROR;
    clear_memory(SEGMENT_START);
    clear_memory(CS_SEG_START);
    mem_ptr = (unsigned int*)SEGMENT_START;
    cs_mem_ptr = (unsigned int*)CS_SEG_START;
    stations_index = 0;
    control_sum = 0;
  }

    
  /*//dualBIOS
  mem_ptr2 = find_address(SEGMENT_START2, SEGMENT_END2);  
  cs_mem_ptr2 = find_address(CS_SEG_START2, CS_SEG_END2);
  if (cs_mem_ptr2 >= (unsigned int*)CS_SEG_START2)
  {
    control_sum = *cs_mem_ptr2; // pobierz z pamieci wartosc sumy kontrolnej,
                                // o ile istnieje (wpp 0)
  }
  else
  {
    control_sum = 0;
  }
  // sprawdz sume kontrolna 2
  if ((CS_SEG_END2 - (unsigned int)cs_mem_ptr2) ==
      (SEGMENT_END2 - (unsigned int)mem_ptr2))
  {
    if (control_sum == calculate_ctrl_sum(SEGMENT_START2, SEGMENT_END2))
    {
      ctrl_sum_check2_succeeded = 1;
    }
  }  
  // uruchom procedury dualBIOSu
  if ((!ctrl_sum_check1_succeeded) || (!ctrl_sum_check2_succeeded))
  {
    // zakladamy, ze jedna z sum musiala byc poprawna
    if (ctrl_sum_check1_succeeded)
      control_sum = *mem_ptr;
    if (ctrl_sum_check2_succeeded)
      control_sum = *mem_ptr2;
    
    clear_memory(SEGMENT_START);
    clear_memory(SEGMENT_START2);
    clear_memory(CS_SEG_START);
    clear_memory(CS_SEG_START2);
    
    mem_ptr = (unsigned int*)SEGMENT_START;
    cs_mem_ptr = (unsigned int*)CS_SEG_START;
    mem_ptr2 = (unsigned int*)SEGMENT_START2;
    cs_mem_ptr2 = (unsigned int*)CS_SEG_START2;
    
    save_to_memory(stations_index, &mem_ptr, SEGMENT_START, SEGMENT_END);   
    save_to_memory(stations_index, &mem_ptr2, SEGMENT_START2, SEGMENT_END2); 
    save_to_memory(control_sum, &cs_mem_ptr, CS_SEG_START, CS_SEG_END);
    save_to_memory(control_sum, &cs_mem_ptr2, CS_SEG_START2, CS_SEG_END2);
    
    stations_index = control_sum;
    
    state = S_CTRL_SUM_ERROR;
  }*/
 
  // sprawdz stan ukladu i wyswietl odpowiedni komunikat
  if (IFG1 & WDTIFG)          // uklad zostal zresetowany przez watchdoga
  {
    IFG1 &= 0xFE;             // wyczyszczenie znacznika przerwania
    state = S_WDOG_RESET;     // zostalismy zresetowani przez wdoga
    display_string(wdog_error); 
  }
  else if (state == S_CTRL_SUM_ERROR) // wystapil blad sumy kontrolnej
  {
    display_string(ctrl_sum_error);
  }
  else                        // uklad dziala poprawnie
  {
    display_string(stations[stations_index]);
  }
  
  WDTCTL = WDTPW + WDTHOLD;     // wylacz watchdoga przed wejsciem do petli
  //petla glowna programu
  while(1)
  {
    //zasypiamy
    _BIS_SR(LPM0_bits + GIE);
    
mainloop_internal:
  
    /******* eliminacja drgan - wcisniecie przycisku (1->0)  *******/
    if ((elim_state == 0) && ((g_flags & BIT0)|| (g_flags & BIT1)))
    {
      i = 0;
      while ((i < ELIM_CHECKS) && (g_curr_input == P2IN))
      {
        i++;  
      }
      if (i == ELIM_CHECKS)   // jesli rzeczywiscie wcisniety
      { 
        if (state != S_NORMAL)// ustawiamy normalny stan poczatkowy ukladu
        {
          state = S_NORMAL;
          display_string(stations[stations_index]);
        }                     // nic wiecej nie robimy
        else
        {
          if (g_flags & BIT1)
            stations_index = (stations_index + 1) % stations_num;  
          else if (g_flags & BIT0)
            stations_index = (stations_index == 0) ? (stations_num - 1): stations_index - 1;
          display_string(stations[stations_index]);
          control_sum += stations_index;
          
          save_to_memory(stations_index, &mem_ptr, SEGMENT_START, SEGMENT_END);   
          save_to_memory(control_sum, &cs_mem_ptr, CS_SEG_START, CS_SEG_END);
          //  w funkcji blokowane sa przerwanie a pozniej odblokowywane
          // wiec jezeli tu potrzebujesz zablokowanych to trzeba cos zmienic
          
          /*//dualBIOS
          save_to_memory(stations_index, &mem_ptr2, SEGMENT_START2, SEGMENT_END2); 
          save_to_memory(control_sum, &cs_mem_ptr2, CS_SEG_START2, CS_SEG_END2);
          */
          
          
        }
        __disable_interrupt();
        // SEKCJA KRYTYCZNA!
        P2IES = 0;            // przelacz zbocze, ktorym wyzwalane jest INT
        P2IFG = 0;            // wyczysc ew. smieci
        // koniec SK
        __enable_interrupt();
        elim_state = 1;
      }
      g_flags = 0;
      //WDTCTL = WDTPW + WDTHOLD;  //:TODO: wylacz watchdoga
      P2IE = BIT0 + BIT1;     // przyciski wyboru stacji
    }
    
    /******* eliminacja drgan - zwolnienie przycisku (0->1)  *******/
    else if ((elim_state == 1) && ((g_flags & BIT0)|| (g_flags & BIT1)))
    {
      i = 0;
      while ((i < ELIM_CHECKS) && (g_curr_input == P2IN))
      {
        i++;  
      }
      if (i == ELIM_CHECKS)   // jesli rzeczywiscie wycisniety
      {
        __disable_interrupt();
        // SEKCJA KRYTYCZNA!
        P2IES = 0xFF;         // przelacz zbocze, ktorym wyzwalane jest INT
        P2IFG = 0;            // wyczysc ew. smieci
        // koniec SK
        __enable_interrupt();
        elim_state = 0;
      }
      g_flags = 0;
      //WDTCTL = WDTPW + WDTHOLD;  //:TODO: wylacz watchdoga
      P2IE = BIT0 + BIT1;     // przyciski wyboru stacji
    }
    
    
  /***** spr, czy nie przyszlo kolejne przerwanie w trakcie petli glownej *****/
    __disable_interrupt();
      if (g_flags != 0)
      {
        __enable_interrupt();
        goto mainloop_internal;
      }
      //else : zasypiajac odblokujemy przerwania
  }
  return 0;
}

// fcja wysylajaca impuls e na wyswietlacz
inline void strobe_e()
{
  P3OUT |= CTRL_E;
  P3OUT &= ~CTRL_E;
}

void delayUs(unsigned int num)
{
  while(num--);
}

void delayMs(unsigned int num)
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
  TACTL |= MC_1;              // wlacza timer. przerwania musza byc wl.
  _BIS_SR(LPM0_bits + GIE);   // zasnij w oczekiwaniu na timer A
}

// wyswietla napis str na ekranie LCD
void display_string(char *str)
{
  int len = strlen(str);
  int i;
  P3OUT = 0x00;      
  P1OUT = LCD_CLEAR;         
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

// zapis do pamieci pod adres (mem_ptr + 2), w segmencie ograniczonym przez 
// segment_end
void save_to_memory(unsigned int int_to_save, unsigned int **mem_ptr,
                    unsigned int segment_start, unsigned int segment_end)
{
  *mem_ptr += 2;
  if(*mem_ptr >= (unsigned int*)segment_end)
  {
    clear_memory(segment_start);//kasuj pamiec
    *mem_ptr = (unsigned int*)segment_start;
  }
  WDTCTL = WDTPW + WDTHOLD;   // wylacz watch-doga
  __disable_interrupt();
  FCTL3 = FWKEY;              // wyczysc LOCK
  FCTL1 = FWKEY + WRT;        // wlacz zapis
  **mem_ptr = int_to_save;    // zapis
  FCTL1 = FWKEY;              // wylacz zapis
  FCTL3 = FWKEY + LOCK;       // ustaw LOCK
  //WDTCTL = WDTPW;         //:TODO: wlacz watchdoga
;;;;;;;;;;;                   // wlacz watch-doga
  __enable_interrupt();
}

// czyszczenie segmentu wskazanego przez mem_ptr
void clear_memory(unsigned int *mem_ptr)
{
  __disable_interrupt();
  WDTCTL = WDTPW + WDTHOLD;   // wylacz watch-doga
  //flash. 514 kHz < SMCLK < 952 kHz
  FCTL2 = FWKEY +FSSEL1+FN0;  // ustawienie zegarow SMLCK/2
  FCTL3 = FWKEY;              // wyczysc LOCK
  FCTL1 = FWKEY + ERASE;      // wlacz kasowanie
  *mem_ptr = 0;               // kasowanie segmentu
  FCTL1 = FWKEY;              // wylacz zapis
  FCTL3 = FWKEY + LOCK;       // ustaw LOCK
  //WDTCTL = WDTPW;         //:TODO: wlacz watchdoga
  ;;;;;                       // wlacz watch-doga
  __enable_interrupt();
}

// zwraca sume kontrolna dla wskazanego segmentu
unsigned int calculate_ctrl_sum(unsigned int seg_start, unsigned int seg_end)
{
  unsigned int *mem_ptr = (unsigned int*)seg_start;
  unsigned int ctrl_sum = 0;
  while (mem_ptr != (unsigned int*)seg_end)
  {
    if (*mem_ptr != 0xFF)
      ctrl_sum += *mem_ptr;
    mem_ptr += 2;
  }
  return ctrl_sum;
}

// wyszukiwanie adresu do zapisu w pamieci w segmencie seg_start - seg_end
// zwraca adres ostatniej zapisanej wartosci
// lub adres seg_start - 2, jesli nic nie znaleziono
unsigned int* find_address(unsigned int seg_start, unsigned int seg_end)
{
  unsigned int *mem_ptr = (unsigned int*)seg_end;
  // wyszukaj miejsce gdzie mozna zapisac
  do{
    if (*mem_ptr != 0xFF)
      return mem_ptr;
    mem_ptr -= 2;
  }
  while (mem_ptr != (unsigned int*)seg_start);  //poczatek segmentu
  // zwroc wartosc przed poczatkiem, jesli nie znaleziono
  return (unsigned int*)seg_start - 2;  
}

// przerwanie przyciskow - samoblokujace
#pragma vector=PORT2_VECTOR
__interrupt void Port1 (void)
{
  g_curr_input = P2IN;        // przepisz wartosc wejsc
  g_flags |= P2IFG & 0x0003;  // czy przyciski 0 lub 1 zglaszaja przerwanie?
  P2IFG = 0;                  // czysc znaczniki przerwan (:TODO: potrzebne?)
  P2IE = 0;                   // zablokuj wlasne przerwania
  //WDTCTL = WDTPW;         //:TODO: wlacz watchdoga
  _BIS_SR(LPM0_EXIT + GIE);
}

// przerwanie zegara - samowylaczajace
#pragma vector=TIMERA0_VECTOR
__interrupt void TimerA (void)
{
  TACTL &= 0xFFCF;            // wylacz timer
  _BIS_SR(LPM0_EXIT + GIE);
}
