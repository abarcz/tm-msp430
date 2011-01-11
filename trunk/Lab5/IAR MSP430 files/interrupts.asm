NAME INTERRUPTS
#include "msp430.h"           ; Processor specific definitions.

PUBLIC interrupts             ; Declare symbol to be exported.

EXTERN g_t_curr_char          ; wskaznik nastepnego znaku do wyslania przez trans.
EXTERN g_t_chars_to_send      ; int ilosc znakow pozostala do wyslania przez trans.
EXTERN g_r_curr_char          ; wskaznik miejsca na nastepny odebrany znak.
EXTERN g_r_chars_received     ; ilosc znakow odebrana przez receive.
EXTERN g_flags                ; znaczniki do komunikacji z aplikacja.
EXTERN g_receive_timeout      ; czas o jaki trzeba przesunac timer A.



RSEG CODE                     ; Code is relocatable.

/* znaczniki przekazywane miedzy ISR a petla glowna
b0 = przerwanie receive zg³osi³o rozpoczêcie odbioru ci¹gu znaków
b4 = przerwanie transmit zglosilo gotowosc do wyslania nowego tekstu
b5 = przerwanie timerA zglosilo timeout 
*/
interrupts

default_int:
        BIC.B #080h, P1OUT            ; zapala diode bledu.
        RETI
        
transmit_usart:
        PUSH R6
        CMP #0000h,g_t_chars_to_send  ; czy wszystko wyslano.
        JEQ end_transmision
        MOV g_t_curr_char,R6
        MOV.B @R6, U0TXBUF
        DEC g_t_chars_to_send
        ADD #0001h,g_t_curr_char      ; skok do nastepnego znaku.
        POP R6
        RETI
end_transmision:
        BIS #0010h, g_flags           ; powiadom ze chcemy kolejny tekst do wyslania.
        MOV 2(SP), R6
        BIC #CPUOFF, R6               ; zmodyfikuj lezace na stosie SR.
        MOV R6, 2(SP)                 ; aby obudzic procesor.
        POP R6
        RETI
            
receive_usart:
        PUSH R6 
        PUSH R7
        MOV.B U0RXBUF,R6 
        CMP #32, g_r_chars_received
        JNE not_full_yet
        BIS #0020h, g_flags
        POP R7                        ; jesli przekroczono ilosc znakow, nic nie rob.
        POP R6
        RETI      
not_full_yet:
        ;ADD g_receive_timeout, TACCR0 ; przesun timeout
        ;BIC #0001h, TACCTL0
        CMP #13, R6 ;czy koniec linii
        JNE cd
        BIS #0020h, g_flags
        MOV 4(SP), R7
        BIC #CPUOFF, R7               ; zmodyfikuj lezace na stosie SR.
        MOV R7, 4(SP)                 ; aby obudzic procesor.
        POP R7
        POP R6
        RETI

cd:
        MOV g_r_curr_char, R7
        MOV.B R6, 0(R7)
        INC g_r_chars_received
        SUB #0001h,g_r_curr_char
        
        CMP #0001h,g_r_chars_received
        JNE receive_next
        BIS #0001h, g_flags           ; powiadom app o rozpoczeciu pobierania.
        MOV 4(SP), R7
        BIC #CPUOFF, R7               ; zmodyfikuj lezace na stosie SR.
        MOV R7, 4(SP)                 ; aby obudzic procesor.
receive_next:
 
        POP R7
        POP R6
        RETI
        
timer_A_int:
        BIS #0020h, g_flags
        PUSH R6
        MOV 2(SP), R6
        BIC #CPUOFF, R6               ; zmodyfikuj lezace na stosie SR.
        MOV R6, 2(SP)                 ; aby obudzic procesor.
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