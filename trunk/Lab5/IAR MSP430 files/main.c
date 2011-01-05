
#include "io430x16x.h"   
#include "intrinsics.h"
#include "string.h"         // memset

#define BAUDRATE 115200     // baud. zejscie ponizej 31250 wymusza
                            // wyliczanie U0BR1

#define ACLK_FQ 8000000     // Hz
#define BUF_SIZE 32         // rozmiar bufora

//timeout odbioru danych, pozwala stwierdzic koniec odbioru
const int receive_timeout = ACLK_FQ / BAUDRATE * 10;
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
  
  // bufory I/O
  char buffer[BUF_SIZE];
  char *err_buf = "MSP-ERROR: device reset.";
  const int err_buf_size = strlen(err_buf);
  char i_char;                  // tymczasowy char do przechowania znaku

  int r_buf_pos = BUF_SIZE - 1; // pozycja kursora w buforze receive
  int receive_on = 0;           // jestesmy w trakcie odbioru wiersza
  int transmit_on = 0;
  int ready_to_transmit = 0;    // istnieja dane gotowe do przeslania
  
  // Stop watchdog timer to prevent time out reset.
  WDTCTL = WDTPW + WDTHOLD;
  
  // wyzerowanie bufora I/O
  memset(buffer, 0, BUF_SIZE);
  BCSCTL1 &= !BIT0;            // wlaczenie XT2 (XT2OFF = 0)
  BCSCTL1 |= RSEL0 + RSEL1 + RSEL2;
  
  /*
  Przygotowanie zegarów.
  */
  //BCSCTL1 |= XTS;               // prze³¹czenie LFXT1 na HF (ACLK)
  //BCSCTL2 |= SELM1;             // MCLK = present(XT2) ? XT2 : LFXT1;
   
  BCSCTL2 |= SELS;                // SMCLK = XT2 jesli obecny
  
  /*
  Ustawienie portów:
  P1.7 - dioda b³êdu
  P3.4 - UTXD0
  P3.5 - URXD0
  */  
  P1OUT &= !BIT7;               // zgaszenie diody bledu 
  P1DIR |= BIT7;                // ustaw bit7 jako wyjsciowy
  P3SEL |= BIT4 + BIT5;         // ustaw piny do obslugi RS232
  P3DIR |= BIT4;        // ustaw pin 4 jako wyjscie :TODO: nie wiem czy potrzebne
  
  /*
  Przygotowanie timera A.
  Timer A slu¿y do wykrycia idle line po zakoñczeniu odbioru znaków z URXD.
  */
  TACTL |= 0x0200;              // wybranie SMCLK
  TACCR0 = receive_timeout; 
  
  /*
  Przygotowanie USART w trybie UART
  */
  U0CTL |= SWRST;
  U0CTL |= PENA  + PEV + SPB;   // :TODO:
  U0TCTL |= SSEL1;              // wybranie SMCLK
  U0RCTL |= URXEIE;
  //:TODO: ewentualne budzenie procesora na rising edge receive
  // ale nie potrzebujemy tego koniecznie, jesli nie wylaczymy zegara
  U0BR0 = ACLK_FQ / BAUDRATE;   // tylko jesli baudrate > 312500!
  U0BR1 = 0;
  U0MCTL = 0;
  ME1 = UTXE0 + URXE0;          // wlaczenie receive i transmit
  
  // ew. dalsza czesc inicjalizacji tutaj
  
  U0CTL &= !SWRST;              // wyzerowanie SWRST
  IE1 |= URXIE0;                // wlacz przerwania receive

  /***************** czêœæ aplikacyjna *****************/
mainloop:
  while(1)
  {
    // Przejœcie w tryb uspienia + odlokowanie przerwañ
    __bis_SR_register(LPM0_bits + GIE);
    __no_operation();
    
    /***************** odebranie znaku *****************/
    TACCTL0 &= 0xFFEE;          // wylacz przerwania timeraA (CCIE, CCIFG)
    if (g_flags & BIT0)         // przerwanie receive zglasza dana do odebrania
    {
      /* sekcja krytyczna odczytu */
      __disable_interrupt();    
        i_char = g_r_char;        // przepisanie znaku do bufora ap
        g_flags &= !BIT0;         // wyczyszczenie znacznika oczekujacej danej
        TACTL &= 0xFFCF;          // wylacz timerA
        TACCR0 = TAR + receive_timeout; // zapewnij przerwanie po rec_timeout
      __enable_interrupt();
      /* koniec sekcji krytycznej odczytu */
      
      // jesli aplikacja nie nadaza z odbieraniem danych od przerwania, blad
      if (g_flags & BIT1)
        goto error;      
      if (transmit_on == 1)     // nie mo¿e jednoczeœnie odbieraæ i wysy³aæ
        goto error;
      if (receive_on == 0)      // jest to poczatek odbierania
      { 
        receive_on = 1;
        TACCR0 = receive_timeout; // zresetuj timer
        TAR = 0;
        TACCTL0 |= CCIE;        // wlacz przerwania
        TACTL |= MC_2;          // continous mode on
      }
      
      if (i_char == 10)         //10 = LF 13 = CR koniec odbioru
      {
        receive_on = 0;         // koniec odbierania
        TACCR0 = receive_timeout; // zresetuj timer
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
        TACTL |= MC_2;        // continous mode on
      }
    }
    else if (g_flags & BIT5)    // timerA zglosil timeout, a receive milczy
    {
      receive_on = 0;           // koniec odbierania
      TACTL &= 0xFFCF;          // wylacz i zresetuj timerA
      TACCR0 = receive_timeout;
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
      r_buf_pos = BUF_SIZE - 1;
      IE1 &= 0x7F;              // wylacz przerwania transmit
      IFG1 |= UTXIFG0;          // zapewnij zgloszenie sie transmit po 
                                // odblokowaniu przerwania
    }
  }
  
  /*************** obs³uga b³êdu = komunikat + reset urz¹dzenia ***************/
error:
  __disable_interrupt();        // namieszalismy, trzeba posprzatac
  P1OUT |= BIT7;                // zapalenie diody bledu    
  
  // wylacz i zresetuj timerA
  TACTL &= 0xFFCF;              
  TACCR0 = receive_timeout;
  TAR = 0; 
  TACCTL0 &= 0xFFEE;            // wylacz przerwania timeraA (CCIE, CCIFG)
  
  // reset zmiennych sterujacych
  receive_on = 0;               
  transmit_on = 0;
  ready_to_transmit = 0;
  r_buf_pos = BUF_SIZE - 1;
  
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
