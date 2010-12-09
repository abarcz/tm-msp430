
/**************************************************************
*  Author: Aleksy Barcz                                       *
*  Date: 09.12.2010                                           *
*                                                             *
*  Program poprawia rozjechane komentarze w listingu          *
*  programu IAR Embedded Workbench, tworzac nowy plik.        *
*  Do poprawnego dzialania wymagane jest, aby kazdy           *
*  komentarz konczyl sie znakiem commEnd.                     *
*  W zwiazku z tym znak commEnd nie moze pojawic sie          *
*  w komentarzu.                                              *
*  Po przebiegu programu komentarze w wynikowym pliku moga    *
*  wciaz miec nadmiarowe spacje.                              *
**************************************************************/

#include <stdio.h>
#include <string>
#include <iostream>

FILE *inFile, *outFile;
int c, state;
const int commEnd = '.';      //znak konca komentarza

int main() 
{
   std::string inFilename, outFilename;
   std::cout << "Podaj nazwe pliku ('z' = \"asm.lst\"): ";
	std::cin >> inFilename;
	if(inFilename == "z")
		inFilename = "asm.lst";
	outFilename = inFilename;
	outFilename.append("_fixed");

	inFile = fopen(inFilename.c_str(), "r");
	if(inFile == NULL)
	{
		std::cout << "Plik nie istnieje!" << std::endl;
		return 0;
	}
	outFile = fopen(outFilename.c_str(), "w");
	if(outFile == NULL)
	{
		std::cout << "Plik nie istnieje!" << std::endl;
		return 0;
	}

	c = 'x';
	state = 0;
	while ((c = fgetc(inFile)) != EOF)
	{
		if (state == 0)		   // normalne przepisywanie kodu
		{
			if (c == ';')
			{
				fputc(c, outFile);
				fputc(' ', outFile);
				state = 1;
				continue;
			}
			fputc(c, outFile);
			continue;
		} 
		else if (state == 1)	   // przetwarzanie ciagu spacji/enterow w komentarzu
		{
			if ((c == ' ') || (c == 13) || (c == 10) || (c == 9)) //spacja enter lub tab
				continue;
			if (c == commEnd)		// sekretny znak konca komentarza
			{
				state = 0;
				continue;
			}
			fputc(c, outFile);
			state = 2;			   // wykryto znak niebedacy bialym znakiem w komentarzu
			continue;
		}
		else if (state == 2)	   // Wypisuj znaki komentarza do outFile
		{
			if ((c == ' ') || (c == 13) || (c == 10) || (c == 9)) //spacja enter lub tab
			{
				fputc(' ', outFile);
				state = 1;
				continue;
			}
			if (c == commEnd)		// sekretny znak konca komentarza
			{
				state = 0;
				continue;
			}
			fputc(c, outFile);
		}
	}
	
	fclose(inFile);
	fclose(outFile);
	std::cout << "Finished processing input file" << std::endl;
	return 0;
}