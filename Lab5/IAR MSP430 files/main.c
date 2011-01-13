
#include "io430x16x.h"   
#include "intrinsics.h"
#include "string.h"           // memset
#include "shared.h"           // wspolne definicje przerwan i aplikacji

#define BAUDRATE 115200       // zejscie ponizej 31250 wymusza wyliczanie U0BR1
#define SMCLK_FQ 8000000      // czestotliwosc SMCLK (ustawiany jako XT2)
#define BUF_NUM 2             // ilosc buforow I/O
#define LN_ENDING_CHARS 2     // ilosc znakow konczacych wysylana linie
#define LF 10                 // znak LF
#define CR 13                 // znak CR

char *g_t_curr_char = NULL; // wskaznik nastepnego znaku do wyslania przez trans
int g_t_chars_count = 0;    // ilosc znakow pozostala do wyslania przez trans

char *g_r_curr_char = NULL; // wskaznik miejsca na nastepny odebrany znak
int g_r_chars_count = 0;    // ilosc znakow odebrana przez receive
int g_rec_addr_step = -1;   // krok w pamieci, o jaki nalezy zmienic adres
                            // bufora odbiorczego po odebraniu znaku

/* bufory I/O realizujace podwojne buforowanie */
static char g_buffers[BUF_NUM][BUF_SIZE + LN_ENDING_CHARS];

/* znaczniki przekazywane miedzy ISR a petla glowna
b4 = przerwanie transmit zglosilo gotowosc do wyslania nowego tekstu
b5 = przerwanie receive zglosilo koniec linii (odebrany wiersz)
b6 = przerwanie receive zlosilo przepelnienie bufora odbiorczego
*/
int g_flags = 0; 

int main( void )
{ 
  // Stop watchdog timer to prevent time out reset.
  WDTCTL = WDTPW + WDTHOLD;
  
  /***************** inicjalizacja systemu *****************/
   // wyzerowanie bufora I/O
  memset(g_buffers, 0, (BUF_SIZE + LN_ENDING_CHARS) * BUF_NUM);
  
  // bufory I/O - ustawienie znakow konca linii dla wysylania
  if (LN_ENDING_CHARS == 2)
  {
    g_buffers[0][BUF_SIZE + 1] = LF;
    g_buffers[0][BUF_SIZE + 0] = CR;
    g_buffers[1][BUF_SIZE + 1] = LF;
    g_buffers[1][BUF_SIZE + 0] = CR;
  }
  
  int rec_buff_index = 0;       // indeks bufora odbiorczego (t_buf=(r_buf+1)%2)
  int ongoing_trans = 0;        // czy jestesmy w trakcie wysylania wiersza
  
  // inicjalizacja bufora odbiorczego
  g_r_curr_char = g_buffers[rec_buff_index] + BUF_SIZE - 1;
  g_r_chars_count = 0;
  
  // bufory bledow
  char *err_buf_overflow = " MSP-ERROR: Line too long. Device reset. ";
  char *err_buf_sec_trans 
    = " MSP-ERROR: Requested second transfer. Device reset. ";
  char *err_buf = err_buf_overflow;

  /*
  Przygotowanie zegarów.
  */   
  BCSCTL2 |= SELS;                // SMCLK = XT2 (obecny dla ukladu 611)
                                  // gdyby go nie bylo, trzeba podlaczyc 
                                  // LFXT1 w trybie XT
  /*
  Ustawienie portów:
  P1.7 - dioda b³êdu
  P3.4 - UTXD0
  P3.5 - URXD0
  */  
  P1OUT &= !BIT7;                 // zgaszenie diody bledu 
  P1DIR |= BIT7;                  // ustaw bit7 jako wyjsciowy
  P3SEL |= BIT4 + BIT5;           // ustaw piny do obslugi RS232
 
  /*
  Przygotowanie USART w trybie UART
  */
  U0CTL |= SWRST;   
  U0TCTL |= SSEL1;                // wybranie SMCLK dla USART
  U0RCTL |= URXEIE;
  U0BR0 = SMCLK_FQ / BAUDRATE - 1;// tylko jesli baudrate > 312500!
  U0BR1 = 0;
  U0MCTL = 0;
  ME1 = UTXE0 + URXE0;            // wlaczenie receive i transmit
  U0CTL &= !SWRST;                // wyzerowanie SWRST
  IE1 |= URXIE0;                  // wlacz przerwania receive

  /***************** czêœæ aplikacyjna *****************/
mainloop:
  while(1)
  {
    // Przejœcie w tryb uspienia + odlokowanie przerwañ
    __bis_SR_register(LPM0_bits + GIE);
    __no_operation();
    
mainloop_internal:
    /***************** zakonczenie odbioru *****************/
    if (g_flags & BIT5)         // zakonczenie odbioru (znak konca linii)
    {                           // przerwania receive sa wylaczone
        /* jesli trwa wysylanie, nie mozemy uruchomic kolejnego - blad */
        if (ongoing_trans)
        {
          err_buf = err_buf_sec_trans;  // ustaw komunikat bledu
          goto error;           
        }
       
        /* przygotuj aktualny bufor odbiorczy jako bufor do wyslania */
        g_t_curr_char = g_r_curr_char + 1;
        g_t_chars_count = g_r_chars_count + LN_ENDING_CHARS;
        
        /* przygotuj drugi bufor jako bufor odbiorczy */
        rec_buff_index = (rec_buff_index + 1) % 2;  
        g_r_curr_char = g_buffers[rec_buff_index] + BUF_SIZE - 1;
        g_r_chars_count = 0;
        
        g_flags &= !BIT5;
        
        /* wlacz wysylanie z jednego bufora oraz odbieranie do drugiego */
        ongoing_trans = 1;      // trwa wysylanie
        IE1 |= UTXIE0 + URXIE0; // wlacz przerwania transmit i receive
    }                           
    
    /**************** zakonczenie transmisji ****************/
    if (g_flags & BIT4)         // transmisja zakonczona
    {
      ongoing_trans = 0;
      g_flags &= !BIT4;
      IE1 &= 0x7F;              // wylacz przerwania transmit
      IFG1 |= UTXIFG0;          // zapewnij zgloszenie sie transmit po 
                                // odblokowaniu przerwania
    }
    
    /****************** blad przepelnienia ******************/
    if (g_flags & BIT6)         // blad przepelnienia (IE receive wylaczone)
    {
        err_buf = err_buf_overflow; // ustaw komunikat bledu
        goto error;             // wyslij informacje o bledzie
    }
    
    /************ sprawdz czy nie zglosilo sie kolejne przerwanie ************/
    __disable_interrupt();
      if (g_flags != 0)           // cos przyszlo w trakcie petli glownej
      {
        __enable_interrupt();
        goto mainloop_internal;   // musimy to obsluzyc zanim zasniemy
      }                           // petla glowna odblokuje przerwania
  }                               // koniec petli glownej aplikacji
  
  /*************** obs³uga b³êdu = komunikat + reset urz¹dzenia ***************/
error:
  __disable_interrupt();        // namieszalismy, trzeba posprzatac
  P1OUT |= BIT7;                // zapalenie diody bledu    

  // reset zmiennych sterujacych
  rec_buff_index = 0;              
  ongoing_trans = 0;
  g_r_curr_char = g_buffers[rec_buff_index] + BUF_SIZE - 1;
  g_r_chars_count = 0;
  
  // wylaczenie przerwan receive na czas wyslania komunikatu o bledzie
  IE1 &= 0xBF;                  // wylacz przerwania receive
  // przygotowanie do wyslania
  g_flags = 0;
  g_t_curr_char = err_buf;
  g_t_chars_count = strlen(err_buf);
  
  IFG1 |= UTXIFG0;              // zapewnij zgloszenie sie przerwania transmit
  IE1 |= UTXIE0;                // wlacz przerwania transmit  
  
  // wyslanie komunikatu o bledzie
  __enable_interrupt();
  while (!(g_flags & BIT4))
    ;
  
  __disable_interrupt();
  g_t_chars_count = 0;
  IE1 &= 0x7F;                  // wylacz przerwania transmit
  IE1 |= URXIE0;                // wlacz przerwania receive
  IFG1 &= 0xBF;                 // skasowanie informacji o przerwaniu receive
  IFG1 |= UTXIFG0;              // zapewnij zgloszenie sie przerwania transmit
  g_flags = 0;

goto mainloop;                  // petla aplikacyjna odblokuje przerwania

  return 0;
}
