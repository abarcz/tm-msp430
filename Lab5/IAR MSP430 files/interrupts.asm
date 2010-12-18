NAME INTERRUPTS
#include "msp430.h"           ; Processor specific definitions.

PUBLIC interrupts             ; Declare symbol to be exported.
RSEG CODE                     ; Code is relocatable.

interrupts

default_int:
        BIC.B #080h, P1OUT    ; zapala diode bledu.
        RETI

timer_A_int:
        BIS #0004h, R15       ; ustaw znacznik timer_A_TACCR0.
        PUSH R6
        MOV 2(SP), R6
        BIC #CPUOFF, R6       ; zmodyfikuj lezace na stosie SR.
        MOV R6, 2(SP)         ; aby obudzic procesor.
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
        DC16 default_int
        
        ORG USART0RX_VECTOR   ; /* 0xFFF2 USART 0 Receive */.
        DC16 default_int
        
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