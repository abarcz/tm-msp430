
#include "io430x16x.h"   
#include "intrinsics.h"
#include "string.h"           // memset

#define BAUDRATE 115200       // zejscie ponizej 31250 wymusza wyliczanie U0BR1
#define SMCLK_FQ 8000000      // Hz
#define BUF_NUM 2             // ilosc buforow IO
#define BUF_SIZE 32           // rozmiar buforow odbiorczo-nadawczych

//timeout odbioru danych, pozwala stwierdzic koniec odbioru
const int g_receive_timeout = SMCLK_FQ / BAUDRATE * 20;

char *g_t_curr_char = NULL; // wskaznik nastepnego znaku do wyslania przez trans
int g_t_chars_to_send = 0;  // ilosc znakow pozostala do wyslania przez trans

char *g_r_curr_char = NULL; // wskaznik miejsca na nastepny odebrany znak
int g_r_chars_received = 0; // ilosc znakow odebrana przez receive

static char buffers[BUF_NUM][BUF_SIZE];
/* znaczniki przekazywane miedzy ISR a petla glowna
b0 = przerwanie receive zg�osi�o rozpocz�cie odbioru ci�gu znak�w
b4 = przerwanie transmit zglosilo gotowosc do wyslania nowego tekstu
b5 = przerwanie timerA zglosilo timeout 
*/
int g_flags = 0; 

int main( void )
{ 
  /***************** inicjalizacja systemu *****************/
  // bufory I/O
  
  char *err_buf = " MSP-ERROR: device reset.";
  const int err_buf_size = strlen(err_buf);
  
  int receive_buffer = 0;     // indeks bufora odbiorczego (t_buf=(r_buf+1)%2)
  int receive_on = 0;         // jestesmy w trakcie odbioru wiersza
  int transmit_on = 0;        // jestesmy w trakcie wysylania wiersza

  g_r_curr_char = buffers[receive_buffer] + BUF_SIZE - 1;
  g_r_chars_received = 0;

  // Stop watchdog timer to prevent time out reset.
  WDTCTL = WDTPW + WDTHOLD;
  
  // wyzerowanie bufora I/O
  memset(buffers, 0, BUF_SIZE * BUF_NUM);

  /*
  Przygotowanie zegar�w.
  */   
  BCSCTL2 |= SELS;                // SMCLK = XT2 (obecny dla ukladu 611)
                                  // gdyby go nie bylo, trzeba podlaczyc 
                                  // LFXT1 w trybie XT
  /*
  Ustawienie port�w:
  P1.7 - dioda b��du
  P3.4 - UTXD0
  P3.5 - URXD0
  */  
  P1OUT &= !BIT7;                 // zgaszenie diody bledu 
  P1DIR |= BIT7;                  // ustaw bit7 jako wyjsciowy
  P3SEL |= BIT4 + BIT5;           // ustaw piny do obslugi RS232
  P3DIR |= BIT4;        // ustaw pin 4 jako wyjscie :TODO: nie wiem czy potrzebne
  
  /*
  Przygotowanie timera A.
  Timer A slu�y do wykrycia idle line po zako�czeniu odbioru znak�w z URXD.
  */
  TACTL |= 0x0200;                // wybranie SMCLK dla TimerA
  TACCR0 = g_receive_timeout; 
  
  /*
  Przygotowanie USART w trybie UART
  */
  U0CTL |= SWRST;
  U0CTL |= PENA  + PEV + SPB;     
  U0TCTL |= SSEL1;                // wybranie SMCLK dla USART
  U0RCTL |= URXEIE;
  U0BR0 = SMCLK_FQ / BAUDRATE - 1;// tylko jesli baudrate > 312500!
  U0BR1 = 0;
  U0MCTL = 0;
  ME1 = UTXE0 + URXE0;            // wlaczenie receive i transmit
  U0CTL &= !SWRST;                // wyzerowanie SWRST
  IE1 |= URXIE0;                  // wlacz przerwania receive

  /***************** cz�� aplikacyjna *****************/
mainloop:
  while(1)
  {
    // Przej�cie w tryb uspienia + odlokowanie przerwa�
    __bis_SR_register(LPM0_bits + GIE);
    __no_operation();
    
    /***************** odebranie znaku *****************/
    //TACCTL0 &= 0xFFEE;          // wylacz przerwania timeraA (CCIE, CCIFG)
    if (g_flags & BIT0)         // rozpoczecie odbioru
    {
      if (receive_on && transmit_on)
        goto error;
      g_flags &= !BIT0;         // wyczyszczenie znacznika oczekujacej danej
      receive_on = 1;
      TACTL &= 0xFFCF;          // wylacz timerA
      TACCR0 = g_receive_timeout; // zresetuj timer
      TAR = 0;
      TACCTL0 |= CCIE;          // wlacz przerwania TODOOOOO
      TACTL |= MC_2;            // continous mode on
    }
    if (g_flags & BIT5)         // timerA
    {
      __disable_interrupt();
        if (transmit_on)
          goto error;
       
        g_t_curr_char = buffers[receive_buffer] + (BUF_SIZE - g_r_chars_received);
        g_t_chars_to_send = g_r_chars_received;
        
        g_r_chars_received = 0;
        receive_buffer = (receive_buffer + 1) % 2;  // przelaczenie buforow
        g_r_curr_char = buffers[receive_buffer] + BUF_SIZE - 1;
        TACTL &= 0xFFCF;          // wylacz timerA
        TACCR0 = g_receive_timeout; // zresetuj timer
        TAR = 0;
        transmit_on = 1;
        IE1 |= UTXIE0;
        receive_on = 0;
        g_flags &= !BIT5;
      __enable_interrupt();
    }
    __disable_interrupt();
    if (g_flags & BIT4)         // zakoncz transmisje
    {
      transmit_on = 0;
      g_flags &= !BIT4;
      IE1 &= 0x7F;              // wylacz przerwania transmit
      IFG1 |= UTXIFG0;          // zapewnij zgloszenie sie transmit po 
                                // odblokowaniu przerwania
    }
    __enable_interrupt();
  } // koniec petli glownej aplikacji
  
  /*************** obs�uga b��du = komunikat + reset urz�dzenia ***************/
error:
  __disable_interrupt();        // namieszalismy, trzeba posprzatac
  P1OUT |= BIT7;                // zapalenie diody bledu    
  
  // wylacz i zresetuj timerA
  TACTL &= 0xFFCF;              
  TACCR0 = g_receive_timeout;
  TAR = 0; 
  TACCTL0 &= 0xFFEE;            // wylacz przerwania timeraA (CCIE, CCIFG)
  
  // reset zmiennych sterujacych
  receive_buffer = 0;
  receive_on = 0;               
  transmit_on = 0;
  g_r_curr_char = buffers[receive_buffer] + BUF_SIZE - 1;
  g_r_chars_received = 0;
  
  // wylaczenie przerwan receive na czas wyslania komunikatu o bledzie
  IE1 &= 0xBF;                  // wylacz przerwania receive
  // przygotowanie do wyslania
  g_flags = 0;
  g_t_curr_char = err_buf;
  g_t_chars_to_send = err_buf_size;
  
  IFG1 |= UTXIFG0;              // zapewnij zgloszenie sie przerwania transmit
  IE1 |= UTXIE0;                // wlacz przerwania transmit  
  
  // wyslanie komunikatu o bledzie
  __enable_interrupt();
  while (!(g_flags & BIT4))
    ;
  
  __disable_interrupt();
  IE1 &= 0x7F;                  // wylacz przerwania transmit
  IE1 |= URXIE0;                // wlacz przerwania receive
  IFG1 &= 0xBF;                 // skasowanie informacji o przerwaniu receive
  IFG1 |= UTXIFG0;              // zapewnij zgloszenie sie przerwania transmit
  g_flags = 0;

goto mainloop;                  // petla aplikacyjna odblokuje przerwania

  return 0;
}
