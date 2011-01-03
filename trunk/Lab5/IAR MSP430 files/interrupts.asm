NAME INTERRUPTS
#include "msp430.h"           ; Processor specific definitions.

PUBLIC interrupts             ; Declare symbol to be exported.

EXTERN g_r_char               ; bufor receive do przekazywania pojedynczego znaku
EXTERN g_t_curr_char          ; wskaznik nastepnego znaku do wyslania przez trans
EXTERN g_t_chars_to_send      ; int ilosc znakow pozostala do wyslania przez trans
EXTERN g_flags                ; int flagi 
RSEG CODE                     ; Code is relocatable.

/* znaczniki przekazywane miedzy ISR a petla glowna
b0 = przerwanie receive zglosilo nowy znak w g_r_char;
b1 = przerwanie zauwazylo, ze aplikacja nie wyczyscila b0 - nie nadazyla
    z odebraniem danej z bufora g_r_char
b4 = przerwanie transmit zglosilo gotowosc do wyslania nowego tekstu
b5 = przerwanie timerA zglosilo timeout 
*/  

interrupts

default_int:
        BIC.B #080h, P1OUT            ; zapala diode bledu.
        RETI
transmit_usart:
        CMP #0000h,g_t_chars_to_send  ; czy wszystko wyslano
        JEQ end_transmision
        DEC g_t_chars_to_send          
        MOV.B &g_t_curr_char, U0TXBUF ; czy dobrze to dziala??
        ADD #0002h,g_t_curr_char      ; skok do nastepnego znaku
        RETI
end_transmision:
        BIS #0010h, g_flags           ; powiadom ze chcemy kolejny teks do wyslania
        MOV 2(SP), R6
        BIC #CPUOFF, R6               ; zmodyfikuj lezace na stosie SR.
        MOV R6, 2(SP)                 ; aby obudzic procesor.
        POP R6
        RETI
        
        
receive_usart:
        BIT #0001h, g_flags           ; czy zdarzylismy odebrac
        JZ recive_next
        ; TODO jakis error
        RETI  
recive_next:
        MOV.B &U0RXBUF,g_r_char       ; czy to dobrze?
        BIS #0001h,g_flags            ; powiadom ze odebralismy
        MOV 2(SP), R6
        BIC #CPUOFF, R6               ; zmodyfikuj lezace na stosie SR.
        MOV R6, 2(SP)                 ; aby obudzic procesor.
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
        DC16 timer_A_int
        
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