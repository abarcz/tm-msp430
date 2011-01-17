
#include "io430x16x.h"   
#include "intrinsics.h"
#include "string.h"           // memset
#include "shared.h"           // wspolne definicje przerwan i aplikacji


#define ELEMENT_NUMBER 2    // liczba napisow w buforze
text_bufor[] = {            // bufor napisow
  "Marymont",
  "slodowiec"} 
int index = 0;              // aktualnie wyswietlany index

/* bufory I/O realizujace podwojne buforowanie */


/* znaczniki przekazywane miedzy ISR a petla glowna
b0 = przycisk lewy zglasza przerwanie
b1 = przycisk prawy zglasza przerwanie
b2 = timer zglasza przerwanie

*/
int g_flags = 0; 

int main( void )
{ 
  // Stop watchdog timer to prevent time out reset.
  WDTCTL = WDTPW + WDTHOLD;
  
  /***************** inicjalizacja systemu *****************/
   
  bool button_pressed= false;      // ustawiana po eliminacji drgan stykow
  bool index_change = false;       // zmiana indeksu nastapila
  /*
  Przygotowanie zegarów.
  */   
  
  /*
  Ustawienie portów:
  P1.7 - dioda b³êdu
  P3.4 - UTXD0
  P3.5 - URXD0
  */  
/*  P1OUT &= !BIT7;                 // zgaszenie diody bledu 
  P1DIR |= BIT7;                  // ustaw bit7 jako wyjsciowy
  P3SEL |= BIT4 + BIT5;           // ustaw piny do obslugi RS232
 */
  /*
  Przygotowanie USART w trybie UART
  */
 /* U0CTL |= SWRST;   
  U0TCTL |= SSEL1;                // wybranie SMCLK dla USART
  U0RCTL |= URXEIE;
  U0BR0 = SMCLK_FQ / BAUDRATE - 1;// tylko jesli baudrate > 312500!
  U0BR1 = 0;
  U0MCTL = 0;
  ME1 = UTXE0 + URXE0;            // wlaczenie receive i transmit
  U0CTL &= !SWRST;                // wyzerowanie SWRST
  IE1 |= URXIE0;                  // wlacz przerwania receive
*/
  /***************** czêœæ aplikacyjna *****************/
mainloop:
  while(1)
  {
    // Przejœcie w tryb uspienia + odlokowanie przerwañ
    __bis_SR_register(LPM0_bits + GIE);
    __no_operation();
    
mainloop_internal:
  //---------------KONCEPCJA
  // jezeli index byl zmieniany wypisz znak o indexsie na wyswietlacz //obsluga wyswietlacza
  if(index_change){
    //wypisz znak
    index_change = false;
  }
  // jezeli przerwanie zglosil przycisk zablokuj przerwania obu przyciskow i wlacz timer
  if((g_flags & BIT0)|| (g_flags & BIT1)){
      //blokuj przerwania obu przyciskow
      //wlacz timer
      // nie czysc flag
  }

  // eliminacja drgan stykow// liczenie do 4 // przerwania timera
  if(g_flags & BIT2){
  }
 
  // jezeli wykryto wcisniecie przycisku zmien index, ustaw flage indexu
  if(button_pressed){
      // zmien indeks
    if((g_flags & BIT0)&&(index>0) ){ // przesuniecie w lewo; zatrzask w 0
      index--;
    }
    if((g_flags &BIT1)&& (index < ELEMENT_NUMBER)){
      index++;
    }
    g_flags &= !  BIT0;
    g_flags &= !  BIT1;
    index_change = true;
    button_pressed = false;
    
  }
  // jezeli flagi nie puste powrot do mainloop
  if(g_flags!= 0)
    goto mainloop_internal;
  
  
  /***************** zakonczenie odbioru *****************/
  /*  if (g_flags & BIT5)         // zakonczenie odbioru (znak konca linii)
    {   */                        // przerwania receive sa wylaczone
        /* jesli trwa wysylanie, nie mozemy uruchomic kolejnego - blad */
    /*    if (ongoing_trans)
        {
          err_buf = err_buf_sec_trans;  // ustaw komunikat bledu
          goto error;           
        }
      */ 
        /* przygotuj aktualny bufor odbiorczy jako bufor do wyslania */
       // g_t_curr_char = g_r_curr_char + 1;
        //g_t_chars_count = g_r_chars_count + LN_ENDING_CHARS;
        
        /* przygotuj drugi bufor jako bufor odbiorczy */
       /* rec_buff_index = (rec_buff_index + 1) % 2;  
        g_r_curr_char = g_buffers[rec_buff_index] + BUF_SIZE - 1;
        g_r_chars_count = 0;
        
        g_flags &= !BIT5;
        */
        /* wlacz wysylanie z jednego bufora oraz odbieranie do drugiego */
        /*ongoing_trans = 1;      // trwa wysylanie
        IE1 |= UTXIE0 + URXIE0; // wlacz przerwania transmit i receive
    }                           
    */
    /**************** zakonczenie transmisji ****************/
/*if (g_flags & BIT4)         // transmisja zakonczona
    {
      ongoing_trans = 0;
      g_flags &= !BIT4;
      IE1 &= 0x7F;              // wylacz przerwania transmit
      IFG1 |= UTXIFG0;          // zapewnij zgloszenie sie transmit po 
                                // odblokowaniu przerwania
    }
  */  
    /****************** blad przepelnienia ******************/
   /* if (g_flags & BIT6)         // blad przepelnienia (IE receive wylaczone)
    {
        err_buf = err_buf_overflow; // ustaw komunikat bledu
        goto error;             // wyslij informacje o bledzie
    }
    */
    /************ sprawdz czy nie zglosilo sie kolejne przerwanie ************/
    /*__disable_interrupt();
      if (g_flags != 0)           // cos przyszlo w trakcie petli glownej
      {
        __enable_interrupt();
        goto mainloop_internal;   // musimy to obsluzyc zanim zasniemy
      }*/                           // petla glowna odblokuje przerwania
  }                               // koniec petli glownej aplikacji
  
  /*************** obs³uga b³êdu = komunikat + reset urz¹dzenia ***************/
/*error:
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
*/
  return 0;
}
