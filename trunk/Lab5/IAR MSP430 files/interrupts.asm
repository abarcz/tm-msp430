NAME INTERRUPTS

#include "msp430.h"           ; Processor specific definitions.
#include "shared.h"           ; wspolne definicje przerwan i aplikacji.

PUBLIC interrupts             ; Declare symbol to be exported.

EXTERN g_t_curr_char          ; wskaznik nastepnego znaku do wyslania przez tr.
EXTERN g_t_chars_count        ; int ilosc znakow pozostala do wyslania przez tr.
EXTERN g_r_curr_char          ; wskaznik miejsca na nastepny odebrany znak.
EXTERN g_r_chars_count        ; ilosc znakow odebrana przez receive.
EXTERN g_flags                ; znaczniki do komunikacji z aplikacja.
EXTERN g_rec_addr_step        ; krok o jaki zmieniamy adres przy odbiorze.

RSEG CODE                     ; Code is relocatable.

/* znaczniki przekazywane miedzy ISR a petla glowna
b4 = przerwanie transmit zglosilo gotowosc do wyslania nowego tekstu
b5 = przerwanie receive zglosilo koniec linii (odebrany wiersz)
b6 = przerwanie receive zlosilo przepelnienie bufora odbiorczego
*/
interrupts

default_int:
        BIC.B #080h, P1OUT            ; zapala diode bledu.
        RETI
        
transmit_usart:
        PUSH R6
        CMP #0000h,g_t_chars_count    ; czy wszystko wyslano.
        JEQ end_transmision     
        MOV g_t_curr_char,R6          ; wpisz adres znaku do R6.
        MOV.B @R6, U0TXBUF            ; wyslij znak spod adresu z R6.
        DEC g_t_chars_count           ; zmniejsz ilosc znakow do wyslania.
        ADD #0001h,g_t_curr_char      ; skok do nastepnego znaku.
        POP R6
        RETI
end_transmision:
        BIS #0010h, g_flags           ; powiadom o zakonczeniu wysylania.
        MOV 2(SP), R6
        BIC #CPUOFF, R6               ; zmodyfikuj lezace na stosie SR.
        MOV R6, 2(SP)                 ; aby obudzic procesor.
        POP R6
        RETI
            
receive_usart:
        PUSH R6 
        MOV.B U0RXBUF,R6              ; przepisz odebrany znak jak najszybciej.
        CMP #LINE_END, R6             ; koniec linii? (nie zapisywany).
        JNE not_endl
        BIC.B #0040h, IE1             ; blokuje przerwania receive.
        BIS #0020h, g_flags           ; znacznik 'koniec linii'.
        MOV 2(SP), R6
        BIC #CPUOFF, R6               ; zmodyfikuj lezace na stosie SR.
        MOV R6, 2(SP)                 ; aby obudzic procesor.
        POP R6
        RETI
not_endl: 
        CMP #BUF_SIZE, g_r_chars_count; przepelnienie?.
        JNE not_full
        BIC.B #0040h, IE1             ; blokuje przerwania receive.
        BIS #0040h, g_flags           ; znacznik 'przepelnienie'.
        MOV 2(SP), R6
        BIC #CPUOFF, R6               ; zmodyfikuj lezace na stosie SR.
        MOV R6, 2(SP)                 ; aby obudzic procesor.                     
        POP R6
        RETI      
not_full:                             ; wpisz znak do tablicy.                    
        PUSH R7
        MOV g_r_curr_char, R7
        MOV.B R6, 0(R7)
        INC g_r_chars_count           ; zwieksz ilosc odebranych znakow.
        ADD g_rec_addr_step, g_r_curr_char  ; zmien adres o wartosc kroku.
        POP R7
        POP R6
        RETI

COMMON INTVEC(1)              ; Interrupt vectors.
        
        ORG PORT2_VECTOR      ; /* 0xFFE2 Port 2 */.
        DC16 default_int
        
        ORG USART1TX_VECTOR   ; /* 0xFFE4 USART 1 Transmit */.
        DC16 default_int
        
        ORG USART1RX_VECTOR   ; /* 0xFFE6 USART 1 Receive */.
        DC16 default_int
        
        ORG PORT1_VECTOR      ; /* 0xFFE8 Port 1 */.
        DC16 default_int
        
        ORG TIMERA1_VECTOR    ; /* 0xFFEA Timer A CC1-2, TA */.
        DC16 default_int
        
        ORG TIMERA0_VECTOR    ; /* 0xFFEC Timer A CC0 */.
        DC16 default_int
        
        ORG ADC12_VECTOR      ; /* 0xFFEE ADC */.
        DC16 default_int
        
        ORG USART0TX_VECTOR   ; /* 0xFFF0 USART 0 Transmit */.
        DC16 transmit_usart
        
        ORG USART0RX_VECTOR   ; /* 0xFFF2 USART 0 Receive */.
        DC16 receive_usart
        
        ORG WDT_VECTOR        ; /* 0xFFF4 Watchdog Timer */.
        DC16 default_int
        
        ORG COMPARATORA_VECTOR; /* 0xFFF6 Comparator A */.
        DC16 default_int
        
        ORG TIMERB1_VECTOR    ; /* 0xFFF8 Timer B CC1-6, TB */.
        DC16 default_int
        
        ORG TIMERB0_VECTOR    ; /* 0xFFFA Timer B CC0 */.
        DC16 default_int
        
        ORG NMI_VECTOR        ; /* 0xFFFC Non-maskable */.
        DC16 default_int
        
        ;ORG RESET_VECTOR      /* 0xFFFE Reset [Highest Priority] */.

END