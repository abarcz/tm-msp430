
#include "io430x16x.h"   
#include "intrinsics.h"
#include "string.h"           // memset
#include "shared.h"           // wspolne definicje przerwan i aplikacji

#define ELEMENT_NUMBER 2    // liczba napisow w buforze
#define COUNT_TIMES 4       // tyle musi sie pojawic poprawnych zliczen
#define TOTAL_COUNTS 24     // maksymalna liczba zliczen

char* text_bufor[] = {            // bufor napisow
  "Marymont",
  "slodowiec"}; 

int index = 0;              // aktualnie wyswietlany index

/* bufory I/O realizujace podwojne buforowanie */


/* znaczniki przekazywane miedzy ISR a petla glowna
b0 = przycisk lewy zglasza przerwanie
b1 = przycisk prawy zglasza przerwanie
b2 = timer zglasza przerwanie
b3 = aktualny stan guzika
*/
int g_flags = 0; 

int main( void )
{ 
  // Stop watchdog timer to prevent time out reset.
  WDTCTL = WDTPW + WDTHOLD;
  
  /***************** inicjalizacja systemu *****************/
  int all_counts = 0;              // liczba wszystkich zliczen
  int counts_in_row = 0;           // liczba zliczen z rzedu
  int button_pressed= 0;      // ustawiana po eliminacji drgan stykow
  int waiting_for_relase = 0;  // @true oznacza ze czekamy na zwolnieni guzika
                                   // @false ze na nacisniecie guzika
  int index_change = 0;       // zmiana indeksu nastapila
  
  /*
  Przygotowanie zegarów.
  */   

  
  /*
  Przygotowanie timera A
  */
 
  
  /*
  Ustawienie portów:
  P1.7 - dioda b³êdu
  P3.4 - UTXD0
  P3.5 - URXD0
  */  
  P5DIR |= 0XFF;     
  // ustawienie portow jako wyjscia
  P1DIR |= BIT0 + BIT2+BIT3;       // linie danych
  P6OUT &= !BIT0;                 // zgaszenie diody bledu 
  P6DIR |= BIT0;                  // ustaw bit7 jako wyjsciowy
  P5OUT = 0;
  P1OUT = 0;
  
  
  /*P3SEL |= BIT4 + BIT5;           // ustaw piny do obslugi RS232
 */
  
  // POWER UP LCD
 /*  P5OUT = DX_FN_SET;
    __no_operation();
    P1OUT = CTRL_E;
    __no_operation();
    P1OUT &= !CTRL_E;
    P5OUT = DX_FN_SET;
    __no_operation();
    P1OUT = CTRL_E;
    __no_operation();
    P1OUT &= !CTRL_E;
    P5OUT = DX_FN_SET;
    __no_operation();
    P1OUT = CTRL_E;
    __no_operation();
    P1OUT &= !CTRL_E;
    
    P5OUT = DX_FN_SET;
    __no_operation();
    P1OUT = CTRL_E;
    __no_operation();
    P1OUT &= !CTRL_E;
    
    P5OUT = 0x08;
    __no_operation();
    P1OUT = CTRL_E;
    __no_operation();
    P1OUT &= !CTRL_E;
    
    P5OUT = 0x01;
    __no_operation();
    P1OUT = CTRL_E;
    __no_operation();
    P1OUT &= !CTRL_E;
    
    P5OUT = DX_ENTRY_MODE_SET ;
    __no_operation();
    P1OUT = CTRL_E;
    __no_operation();
    P1OUT &= !CTRL_E;
    
    P5OUT = 'a'; 
    __no_operation();
    P1OUT = CTRL_E;
    __no_operation();
    P1OUT &= !CTRL_E;*/
  /***************** czêœæ aplikacyjna *****************/
mainloop:
  while(1)
  {
   
    // Przejœcie w tryb uspienia + odlokowanie przerwañ
    //__bis_SR_register(LPM0_bits + GIE);
    __no_operation();
    
    P5OUT =DX_RETURN_HOME;
    __no_operation();
    P1OUT |= CTRL_E;
    __no_operation();
    P1OUT &= CTRL_NE;
    
    P5OUT = DX_FN_SET;
    __no_operation();
    P1OUT |= CTRL_E;
    __no_operation();
    P1OUT &= CTRL_NE;
    P5OUT = 0x0F;//DX_DISP_ON_OFF ;
    __no_operation();
    P1OUT |= CTRL_E;
    __no_operation();
    P1OUT &= CTRL_NE;
    P5OUT = DX_ENTRY_MODE_SET ;
    __no_operation();
    P1OUT |= CTRL_E;
    __no_operation();
    P1OUT &= CTRL_NE;
    //DANE
    P1OUT |= CTRL_RS;
    __no_operation();
    P1OUT |= CTRL_E;
    __no_operation();
    P1OUT &= CTRL_NE;
    P5OUT = 0x62; 
    __no_operation();
    P1OUT |= CTRL_E;
    __no_operation();
    P1OUT &= CTRL_NE;
      
    //  P5OUT =DX_RETURN_HOME ; 
    
    
mainloop_internal:
  //---------------KONCEPCJA
  // jezeli index byl zmieniany wypisz znak o indexsie na wyswietlacz //obsluga wyswietlacza
  /*if(index_change){
    //wypisz znak
    //zapisz indeks do pamieci
    index_change = 0;
  }
  // jezeli przerwanie zglosil przycisk zablokuj przerwania obu przyciskow i wlacz timer
  if((g_flags & BIT0)|| (g_flags & BIT1)){
      //blokuj przerwania obu przyciskow
      //wlacz timer
      // nie czysc flag
  }

  // eliminacja drgan stykow// liczenie do 4 // przerwania timera
  if(g_flags & BIT2){
    all_counts++;
    if(all_counts >= TOTAL_COUNTS){ // koniec eliminacji
      //TODOwylacz timer
      all_counts =0;
      counts_in_row = 0;
    }
    else{
      if(!waiting_for_relase){               // czekamy na nacisniecie
         if( g_flags & !BIT2)               // czy BIT2 == 0, wcisnieto
           counts_in_row++;
         else
           counts_in_row = 0;
      }
      else{                                 // czekamy na zwolnienie
         if( g_flags & BIT2)                // czy BIT2 == 1, zwolniono
           counts_in_row++;
         else
           counts_in_row = 0;
        
      }
      if(counts_in_row == COUNT_TIMES){   // jesli wykrylismy zmiana button to zmieniamy

          waiting_for_relase = !waiting_for_relase;
          if(!waiting_for_relase)         // jezeli przycisk jest puszczony
            ;//TODO wylacz timer
      }
    }
    g_flags &= !BIT2;         //kasuj flage
  }
 
  // jezeli wykryto wcisniecie przycisku zmien index, ustaw flage indexu
  if(button_pressed ){
      // zmien indeks
    if((g_flags & BIT0)&&(index>0) ){ // przesuniecie w lewo; zatrzask w 0
      index--;
    }
    if((g_flags &BIT1)&& (index < ELEMENT_NUMBER)){ //przesunie w prawo
      index++;
    }
    g_flags &= !  BIT0;
    g_flags &= !  BIT1;
    index_change = 1;
    button_pressed = 0;
    
  }
  // jezeli flagi nie puste powrot do mainloop
  if(g_flags!= 0)
    goto mainloop_internal;
  */
  }
  return 0;
}
