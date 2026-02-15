#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void bytesToHexChars(unsigned char *, int, int);

int main(int argc, char *argv[])
{
   if (argc != 3) {
      printf("Usage: bin_to_c <filename> <varname>\n");
      printf("(output is written to stdout)\n");
      return 0; // no filename passed
   }

   FILE *filePtr = fopen(argv[1],"rb"); // open input file
   
   if (filePtr == NULL) {
      printf("Unable to open file: %s\n", argv[1]);
      return -1; // bad filename passed
   }
   
   fseek(filePtr, 0L, SEEK_END); // get the file size
   int iSize = (int)ftell(filePtr);
   fseek(filePtr, 0, SEEK_SET);
   unsigned char *p = (unsigned char *)malloc(0x10000); // allocate 64k to play with

   printf("const uint8_t %s[] = {", argv[2]); // start of data array

   int iData;

   while (iSize)
   {
      iData = fread(p, 1, 0x10000, filePtr); // try to read 64k
      bytesToHexChars(p, iData, iSize == iData); // create the output data
      iSize -= iData;
   }

   free(p);
   fclose(filePtr);
   printf("};\n"); // final closing brace
   return 0;
}

void bytesToHexChars(unsigned char *p, int iLen, int bLast) {

   int i, j, iCount;
   char szTemp[256], szOut[256];

   iCount = 0;
   for (i=0; i<iLen>>4; i++) { // do lines of 16 bytes
      strcpy(szOut, "\t");
      for (j=0; j<16; j++)
      {
         if (iCount == iLen-1 && bLast) // last one, skip the comma
            sprintf(szTemp, "0x%02x", p[(i*16)+j]);
         else
            sprintf(szTemp, "0x%02x,", p[(i*16)+j]);
         strcat(szOut, szTemp);
         iCount++;
      }
      if (!bLast || iCount != iLen)
         strcat(szOut, "\n");
      printf("%s",szOut);
   }

   p += (iLen & 0xfff0); // point to last section

   if (iLen & 0xf) { // any remaining characters?

      strcpy(szOut, "\t");
      for (j=0; j<(iLen & 0xf); j++)
      {
         if (iCount == iLen-1 && bLast)
            sprintf(szTemp, "0x%02x", p[j]);
         else
            sprintf(szTemp, "0x%02x,", p[j]);
         strcat(szOut, szTemp);
         iCount++;
      }
      if (!bLast)
         strcat(szOut, "\n");
      printf("%s",szOut);
   }
}

