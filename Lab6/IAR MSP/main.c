    #include "msp430x14x.h"
    #include "intrinsics.h"
    #include "math.h"
    #include "string.h"


    #define MASK_E 0x01
    #define MASK_RS 0x02
    #define ELEMENT_NUMBER 2    // liczba napisow w buforze
    #define COUNT_TIMES 4       // tyle musi sie pojawic poprawnych zliczen
    #define TOTAL_COUNTS 24     // maksymalna liczba zliczen
    #define DCO_FQ 1000000      // czestotliwosc DCO
    #define ELIM_TIME_DIV 500  // 1/ELIM_TIME_DIV = czas eliminacji drgan
    
    const int ELIM_CHECKS = DCO_FQ / ELIM_TIME_DIV;

char *stacje[] = {"Kabaty", "Natolin", "Imielin", "Stoklosy", "Ursynow",
"Sluzew", "Wilanowska", "Wierzbno", "Raclawicka", "Pola Mokotowskie", "Politechnika",
"Centrum", "Swietokrzyska", "Ratusz Arsenal", "Dworzec Gdanski", "Plac Wilsona",
"Marymont", "Slodowiec", "Stare Bielany", "Wawrzyszew", "Mlociny"};

const int stations_num = 21;

  int timer_on = 0;
int index = 0;              // aktualnie wyswietlany index
    //Obsluga wyswietlacza
    void strobeE();
    inline void delayUs(unsigned int num);
    inline void delayMs(unsigned int num);
    void display_string(char *tab);

    /* znaczniki przekazywane miedzy ISR a petla glowna
    b0 = przycisk lewy zglasza przerwanie (dolny)
    b1 = przycisk prawy zglasza przerwanie (gorny)
    */
    int g_flags = 0;

    int curr_p2_state = 0;  // stan portu P2 (WE, przyciski na b0 i b1)

    int main( void )
    {
       // Stop watchdog timer to prevent time out reset
      WDTCTL = WDTPW + WDTHOLD;
      
      // przyciski
      P2IES = 0xFF;        // przerwanie wyzwalane 1->0
      P2IE = BIT0 + BIT1;  // przyciski wyboru stacji
      
      // timer A
      TACTL |= TASSEL_1;
      TACCTL0 |= CCIE;
      TACCR0 = 5;
     
      int i = 0;
      int elim_state = 0;   // stan eliminacji drgan : 0 - czekamy na 0, 1 - czekamy na 1

       
       //Port 1 to wyjscie danych na wyswietlacz
       P1DIR = 0xFF;
       P1OUT = 0x00;

       //Port 3 to wyjscie sterujace wyswietlaczem
       // pin 1 = wyjscie strobujace E
       // pin 2 = wyjscie RS
       P3DIR = 0xFF;
       P3OUT = 0x00;
       
       //Ustawienie wyswietlacza
       P1OUT = 0x38;          //Function set
       strobeE();
       P1OUT = 0x0C;          //Display on/off control
       strobeE();
       P1OUT = 0x06;          //Entry mode set
       strobeE();
       delayUs(40);
       P1OUT = 0x01;          //Clear
       strobeE();
       delayMs(3);
       
       display_string(stacje[index]);
       //petla glowna programu
       while(1)
       {
          //zasypiamy
         _BIS_SR(LPM0_bits + GIE);
         __disable_interrupt();
         

         
         if ((elim_state == 0) && ((g_flags & BIT0)|| (g_flags & BIT1)))
         {
            i = 0;
            while ((i < ELIM_CHECKS) && (curr_p2_state == P2IN))
            {
              i++;  
            }
            if (i == ELIM_CHECKS)
            {
              if (g_flags & BIT1)
                index = (index + 1) % stations_num;  
              else if (g_flags & BIT0)
                index = (index == 0) ? (stations_num - 1): index - 1;
              display_string(stacje[index]);
              // SEKCJA KRYTYCZNA!
              P2IES = 0;
              P2IFG = 0;
              // koniec SK
              elim_state = 1;
            }
            g_flags = 0;
            P2IE = BIT0 + BIT1;  // przyciski wyboru stacji
         }
         else if ((elim_state == 1) && ((g_flags & BIT0)|| (g_flags & BIT1)))
        {
            i = 0;
            while ((i < ELIM_CHECKS) && (curr_p2_state == P2IN))
            {
              i++;  
            }
            if (i == ELIM_CHECKS)
            {
              // SEKCJA KRYTYCZNA!
              P2IES = 0xFF;
              P2IFG = 0;
              // koniec SK
              elim_state = 0;
            }
            g_flags = 0;
            P2IE = BIT0 + BIT1;  // przyciski wyboru stacji
         }
           

          __enable_interrupt();
       }
       return 0;
    }


    void strobeE()
    {
       P3OUT |= MASK_E;
       P3OUT &= ~MASK_E;
    }

    inline void delayUs(unsigned int num)
    {
      while(num--);
    }

    inline void delayMs(unsigned int num)
    {
      while(num--)
        delayUs(80);
    }
    
    #pragma vector=PORT2_VECTOR
    __interrupt void Port1 (void)
    {
       curr_p2_state = P2IN;
       g_flags |= P2IFG & 0x0003;
       _BIS_SR(LPM0_EXIT + GIE);
       P2IFG = 0;
       P2IE = 0;
    }
    
    #pragma vector=TIMERA0_VECTOR
    __interrupt void TimerA (void)
    {

      g_flags |= BIT2;
      _BIS_SR(LPM0_EXIT + GIE);
    }
    
    void display_string(char *tab)
    {
      int len = strlen(tab);
      int i;
      P3OUT = 0x00;      
      P1OUT = 0x01;          //Clear
      strobeE();
      delayMs(3);
      for (i = 0; i < len; i++)
      {
        P1OUT = tab[i];
        P3OUT = MASK_RS;
        strobeE();
      }
    }
