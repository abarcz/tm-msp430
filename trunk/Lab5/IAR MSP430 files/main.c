
#include "io430x16x.h"   
#include "intrinsics.h"
#include "string.h"         // memset

#define BAUDRATE 38400      // baud. zejscie ponizej 31250 wymusza
                            // wyliczanie U0BR1

#define ACLK_FQ 8000000     // Hz
#define BUF_SIZE 256        // rozmiar bufora

char g_r_char = '0';        // bufor receive do przekazywania pojedynczego znaku
char *g_t_curr_char = NULL; // wskaznik nastepnego znaku do wyslania przez trans
int g_t_chars_to_send = 0;  // ilosc znakow pozostala do wyslania przez trans

/* znaczniki przekazywane miedzy ISR a petla glowna
b0 = przerwanie receive zglosilo nowy znak w g_r_char;
b1 = przerwanie zauwazylo, ze aplikacja nie wyczyscila b0 - nie nadazyla
     z odebraniem danej z bufora g_r_char
b4 = przerwanie transmit zglosilo gotowosc do wyslania nowego tekstu
b5 = przerwanie timerA zglosilo timeout 
*/
int g_flags = 0;            

int main( void )
{
  //timeout odbioru danych, pozwala stwierdzic koniec odbioru
  const int receive_timeout = ACLK_FQ / BAUDRATE * 4;
  
  // bufory I/O
  char buffer[BUF_SIZE];
  char i_char;                  // tymczasowy char do przechowania znaku

  int r_buf_pos = BUF_SIZE - 1; // pozycja kursora w buforze receive
  int receive_on = 0;           // jestesmy w trakcie odbioru wiersza
  int transmit_on = 0;
  int ready_to_transmit = 0;    // istnieja dane gotowe do przeslania
  
  // wyzerowanie bufora I/O
  memset(buffer, 0, BUF_SIZE);
  
  // Stop watchdog timer to prevent time out reset.
  WDTCTL = WDTPW + WDTHOLD;
  
  /*
  Przygotowanie zegarów.
  */
  BCSCTL1 |= XTS;               // prze³¹czenie LFXT1 na HF (ACLK)
  BCSCTL2 |= SELM1;             // MCLK = present(XT2) ? XT2 : LFXT1;

  /*
  Ustawienie portów:
  P1.7 - dioda b³êdu
  P3.4 - UTXD0
  P3.5 - URXD0
  */  
  P1DIR |= BIT7;                // ustaw bit7 jako wyjsciowy
  P3SEL |= BIT4 + BIT5;         // ustaw piny do obslugi RS232
  P3DIR |= BIT4;        // ustaw pin 4 jako wyjscie :TODO: nie wiem czy potrzebne
  
  /*
  Przygotowanie timera A.
  Timer A slu¿y do wykrycia idle line po zakoñczeniu odbioru znaków z URXD.
  */
  TACTL |= TASSEL_1 + TAIE;     // wybranie ACLK, w³ przerwania TAIFG
  TACCR1 = receive_timeout; 
  
  /*
  Przygotowanie USART w trybie UART
  */
  U0CTL |= SWRST;
  U0TCTL |= SSEL0;              // wybranie ACLK
  //:TODO: ewentualne budzenie procesora na rising edge receive
  // ale nie potrzebujemy tego koniecznie, jesli nie wylaczymy zegara
  U0BR0 = ACLK_FQ / BAUDRATE;   // tylko jesli baudrate > 312500!
  U0BR1 = 0;
  U0MCTL = 0;
  ME1 = UTXE0 + URXE0;          // wlaczenie receive i transmit
  
  // ew. dalsza czesc inicjalizacji tutaj
  
  U0CTL &= !SWRST;              // wyzerowanie SWRST
  IE1 = URXIE0;                 // wlacz przerwania receive
  
  /***************** czêœæ aplikacyjna *****************/
  while(1)
  {
    // Przejœcie w tryb uspienia + odlokowanie przerwañ
    __bis_SR_register(LPM0_bits + GIE);
    __no_operation();
    
    /***************** odebranie znaku *****************/
    TACCTL0 &= !(CCIE + CCIFG); // wylacz przerwania timeraA
    if (g_flags & BIT0)         // przerwanie receive zglasza dana do odebrania
    {
      i_char = g_r_char;        // przepisanie znaku do bufora ap
      g_flags &= !BIT0;         // wyczyszczenie znacznika oczekujacej danej
      r_buf_pos++;
      // jesli aplikacja nie nadaza z odbieraniem danych od przerwania, blad
      if (g_flags & BIT1)
        goto error;
      
      TACTL &= !(MC_0 + MC_1);  // wylacz timerA
      TACCR1 = TAR + receive_timeout; // zapewnij przerwanie po rec_timeout
      
      if (transmit_on == 1)     // nie mo¿e jednoczeœnie odbieraæ i wysy³aæ
        goto error;
      if (receive_on == 0)      // jest to poczatek odbierania
      { 
        receive_on = 1;
        // poniewaz odebrano bit adresu, nie rob nic wiecej
        TACCTL0 |= CCIE;        // wlacz przerwania
        TACTL |= MC_1;          // continous mode on
      }
      else
      {
        if (i_char == 10) //10 = LF 13 = CR koniec odbioru
        {
          receive_on = 0;       // koniec odbierania
          TACCR1 = receive_timeout; // zresetuj timer
          TAR = 0;
          ready_to_transmit = 1;
        }
        else if (r_buf_pos < 0)
        {
          goto error;
        }
        else                    // zapisz dana do bufora
        {
          buffer[r_buf_pos] = i_char;
          r_buf_pos--;
          TACCTL0 |= CCIE;      // wlacz przerwania
          TACTL |= MC_1;        // continous mode on
        }
      }
    }
    else if (g_flags & BIT5)    // timerA zglosil timeout, a receive milczy
    {
      receive_on = 0;           // koniec odbierania
      TACTL &= !(MC_0 + MC_1);  // wylacz i zresetuj timerA
      TACCR1 = receive_timeout;
      TAR = 0;
      
      g_flags &= !BIT5;
      if (r_buf_pos != BUF_SIZE - 1)    // jesli cos odebrano
      {
        ready_to_transmit = 1;
      }
    }
    
    /***************** rozpoczecie transmisji *****************/
    if ((!transmit_on) && (ready_to_transmit))
    {
      transmit_on = 1;
      ready_to_transmit = 0;
      g_t_curr_char = buffer + r_buf_pos + 1;
      g_t_chars_to_send = BUF_SIZE - 1 - r_buf_pos;
      IE1 |= UTXIE0;            // wlacz przerwania transmit
    }
    
    /***************** transmisja zakonczona *****************/
    if (g_flags & BIT4)
    {
      transmit_on = 0;
      g_flags &= !BIT4;
      IE1 &= !UTXIE0;           // wylacz przerwania transmit
      IFG1 |= UTXIFG0;          // zapewnij zgloszenie sie transmit po 
                                // odblokowaniu przerwania
    }
  }
  
error:         
  P1OUT |= BIT7;                // zapalenie diody bledu         
  return 0;
}
