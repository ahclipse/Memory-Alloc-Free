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
    void* ptr = Mem_Alloc(256);
    Mem_Dump();
    void* ptr2 = Mem_Alloc(256);
    Mem_Dump();
    void* ptr3 = Mem_Alloc(256);
    Mem_Dump();
    Mem_Free(ptr);
    Mem_Dump();
    Mem_Free(ptr3);
    Mem_Dump();
  }

  exit(0);
}
