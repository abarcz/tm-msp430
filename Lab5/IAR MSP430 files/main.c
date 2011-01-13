
#include "io430x16x.h"   
#include "intrinsics.h"
#include "string.h"           // memset

#define BAUDRATE 115200       // zejscie ponizej 31250 wymusza wyliczanie U0BR1
#define SMCLK_FQ 8000000      // Hz
#define BUF_NUM 2             // ilosc buforow IO
#define BUF_SIZE 20           // rozmiar buforow odbiorczo-nadawczych
#define LF 10
#define CR 13

char *g_t_curr_char = NULL; // wskaznik nastepnego znaku do wyslania przez trans
int g_t_chars_to_send = 0;  // ilosc znakow pozostala do wyslania przez trans

char *g_r_curr_char = NULL; // wskaznik miejsca na nastepny odebrany znak
int g_r_chars_received = 0; // ilosc znakow odebrana przez receive
int g_rec_addr_step = -1;

static char buffers[BUF_NUM][BUF_SIZE + 2];
/* znaczniki przekazywane miedzy ISR a petla glowna
b4 = przerwanie transmit zglosilo gotowosc do wyslania nowego tekstu
b5 = przerwanie receive zglosilo koniec linii (odebrany wiersz)
b6 = przerwanie receive zlosilo przepelnienie bufora odbiorczego
*/
int g_flags = 0; 

int temp;

int main( void )
{ 
  // Stop watchdog timer to prevent time out reset.
  WDTCTL = WDTPW + WDTHOLD;
  
  /***************** inicjalizacja systemu *****************/
  // bufory I/O
  char *err_buf1 = " MSP-ERROR: Line too long. Device reset. ";
  char *err_buf2 = " MSP-ERROR: Requested second transfer. Device reset. ";
  char *err_buf = err_buf1;
  
  int receive_buffer = 0;     // indeks bufora odbiorczego (t_buf=(r_buf+1)%2)
  int transmit_on = 0;        // jestesmy w trakcie wysylania wiersza

  g_r_curr_char = buffers[receive_buffer] + BUF_SIZE - 1 - 2;
  g_r_chars_received = 0;
  
  // wyzerowanie bufora I/O
  memset(buffers, 0, (BUF_SIZE + 2) * BUF_NUM);

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
  g_r_chars_received = 0;
mainloop:
  while(1)
  {
    // Przejœcie w tryb uspienia + odlokowanie przerwañ
    __bis_SR_register(LPM0_bits + GIE);
    __no_operation();
    
  mainloop_internal:
    /***************** zakonczenie odbioru *****************/
    if (g_flags & BIT5)         // zakonczenie odbioru (znak konca linii)
    {                           // IE receive wylaczone
        if (transmit_on)
        {
          err_buf = err_buf2;
          goto error;           // zakonczono kolejny odbior, gdy trwa transmit
        }
       
        g_t_curr_char = buffers[receive_buffer] 
            + (BUF_SIZE - g_r_chars_received - 2);
        buffers[receive_buffer][BUF_SIZE - 1] = LF;
        buffers[receive_buffer][BUF_SIZE - 2] = CR;
        g_t_chars_to_send = g_r_chars_received + 2;

        g_r_chars_received = 0;
        receive_buffer = (receive_buffer + 1) % 2;  // przelaczenie buforow
        g_r_curr_char = buffers[receive_buffer] + BUF_SIZE - 1 - 2;
        transmit_on = 1;
        g_flags &= !BIT5;
        IE1 |= UTXIE0 + URXIE0; // wlacz przerwania transmit aby wyslac
    }                           // oraz przerwania receive - drugi bufor
    
    /**************** zakonczenie transmisji ****************/
    if (g_flags & BIT4)         // transmisja zakonczona
    {
      transmit_on = 0;
      g_flags &= !BIT4;
      IE1 &= 0x7F;              // wylacz przerwania transmit
      IFG1 |= UTXIFG0;          // zapewnij zgloszenie sie transmit po 
                                // odblokowaniu przerwania
    }
    
    /****************** blad przepelnienia ******************/
    if (g_flags & BIT6)         // blad przepelnienia (IE receive wylaczone)
    {
        err_buf = err_buf1;
        goto error;             // wyslij informacje o bledzie
    }
    
    /************ sprawdz czy nie zglosilo sie kolejne przerwanie ************/
    __disable_interrupt();
    if (g_flags != 0)           // cos przyszlo w trakcie petli glownej
      goto mainloop_internal;   // musimy to obsluzyc zanim zasniemy
  } // koniec petli glownej aplikacji
  
  /*************** obs³uga b³êdu = komunikat + reset urz¹dzenia ***************/
error:
  __disable_interrupt();        // namieszalismy, trzeba posprzatac
  P1OUT |= BIT7;                // zapalenie diody bledu    

  // reset zmiennych sterujacych
  receive_buffer = 0;              
  transmit_on = 0;
  g_r_curr_char = buffers[receive_buffer] + BUF_SIZE - 1 - 2;
  g_r_chars_received = 0;
  
  // wylaczenie przerwan receive na czas wyslania komunikatu o bledzie
  IE1 &= 0xBF;                  // wylacz przerwania receive
  // przygotowanie do wyslania
  g_flags = 0;
  g_t_curr_char = err_buf;
  g_t_chars_to_send = strlen(err_buf);
  
  IFG1 |= UTXIFG0;              // zapewnij zgloszenie sie przerwania transmit
  IE1 |= UTXIE0;                // wlacz przerwania transmit  
  
  // wyslanie komunikatu o bledzie
  __enable_interrupt();
  while (!(g_flags & BIT4))
    ;
  
  __disable_interrupt();
  g_t_chars_to_send = 0;
  IE1 &= 0x7F;                  // wylacz przerwania transmit
  IE1 |= URXIE0;                // wlacz przerwania receive
  IFG1 &= 0xBF;                 // skasowanie informacji o przerwaniu receive
  IFG1 |= UTXIFG0;              // zapewnij zgloszenie sie przerwania transmit
  g_flags = 0;

goto mainloop;                  // petla aplikacyjna odblokuje przerwania

  return 0;
}
