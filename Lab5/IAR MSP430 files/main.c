
#include "io430.h"   

int main( void )
{
  // Stop watchdog timer to prevent time out reset.
  WDTCTL = WDTPW + WDTHOLD;

  P1DIR |= BIT7;                  // ustaw bit7 jako wyjsciowy
                                  // (dioda bledu)

  return 0;
}
