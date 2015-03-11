#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "mymem.h"
#include <stdlib.h> 

int main(int argc, char * argv[])
{
  int regionSize = 8192;
  int slabSize = 256;
  void * mem; 

  if((mem = Mem_Init(regionSize, slabSize)) != NULL)
  {
    Mem_Dump();
  }

  exit(0);
}
