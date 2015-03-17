#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include "mymem.h"


void init_slab( int, void*);
void* Mem_Alloc_slab();
void* Mem_Alloc_nextFit(int);
int Mem_Free_nextFit(void *);
int Mem_Free_slab(void *);

int allocated_once = 0;

//bookmark of where we left off last
struct FreeHeader *list_head = NULL;
void** slab_head = NULL;

//Head and tail free slab list
void** s_head;
void** s_tail;

//start of regions
void* nextRegionStartAddr;
void* slabRegionStartAddr;

int s_regionSize; 
int globalSlabSize;

int totalMemSize;

void* Mem_Init(int regionSize, int slabSize)
{
  int fd;
  //int alloc_size;
  void* memStart;
  //int nextRegionOffset;
  //static int initialized = 0;
   
  if(0 != allocated_once)
  {
    return NULL;
  }
  if(regionSize <= 0)
  {
    return NULL;
  }

  //Set the value of the slab region
  s_regionSize = (int)(.25 * regionSize); 
  globalSlabSize = slabSize;
  totalMemSize = regionSize;

  //mmap to allocate memory
  fd = open("/dev/zero", O_RDWR);
  if(-1 == fd)
  {
    return NULL;
  }
  memStart = mmap(NULL, regionSize, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
  if (MAP_FAILED == memStart)
  {
    return NULL;
  }
  
  allocated_once = 1;
  
  //TODO assuming instuctors are nice and slab size fits well in a quarter of the 
  //	allocated space
  //	Going to Have to figure out a way to do below operation..
  nextRegionStartAddr = (int)(.25*(regionSize)) + memStart;
  
  if(slabSize <= 0)
  {
    return NULL;
  }

  init_slab(slabSize ,nextRegionStartAddr);
  if( slab_head == NULL )
  {
    return NULL;
  }

  //Initialization of next fit memory 
  list_head = (struct FreeHeader*)(nextRegionStartAddr);
  list_head->length = (int)(.75*regionSize)- sizeof(struct FreeHeader);
  list_head->next = NULL;
  
  return memStart;
}

void init_slab( int slabSize, void* s_regionStart)
{
  void** nextSlab;
  void** currSlab;

  slab_head = s_regionStart;
  slabRegionStartAddr = s_regionStart;  

  s_head = s_regionStart;
  currSlab = s_regionStart;  
  nextSlab = (void *)(currSlab + slabSize);

  while( *nextSlab - s_regionStart <= s_regionSize)
  {
    *currSlab = nextSlab; 
    currSlab = nextSlab;
    nextSlab = (void **)(currSlab + slabSize);
  }
  *currSlab = s_regionStart;
  s_tail = currSlab;
}

void* Mem_Alloc(int size)
{
  if( size == globalSlabSize)
    return Mem_Alloc_slab();
  else
    return Mem_Alloc_nextFit(size);
}

int Mem_Free(void *ptr){
  if( ptr < slabRegionStartAddr || ptr > (void *)(slabRegionStartAddr + totalMemSize) )
  {
    printf("SEGFAULT\n");
    return -1;
  }

  if( ptr >= nextRegionStartAddr )
    return Mem_Free_nextFit(ptr);
  else
    return Mem_Free_slab(ptr);
}

void* Mem_Alloc_nextFit(int size)
{
  return NULL;
}

int Mem_Free_nextFit(void *ptr)
{
  /* Pointers to block headers /
  block_header *previous;
  block_header *current;
  int header_size;
  
  if( ptr == NULL)
    return -1;

  // Set up *ptr so it points to header not block itself /
  ptr = ((char *) ptr) - sizeof(block_header);

  //if block is not busy something is wrong
  
 // if ptr doest point to a header it would still -1 as would 
 // failing this test, there is nothing lost by checking this before checking
  //if it is present in list
  if( ((block_header *) ptr)->size_status % 2 == 0 )
  {
    return -1;
  }

  current = list_head;
  header_size = (int)sizeof(block_header);

// if first block is the block to be freed /
  if( current == ptr )
  {
    if( current->next != NULL && current->next->size_status % 2 == 0)
    {
   // coalesce blocks /
      current->size_status += current->next->size_status + header_size ;
      current->next = current->next->next;
    }

  // Mark as free block /
    current->size_status --;  
    return 0;
  }

  // set up  previous 
  previous = current;
  current = current->next;

  while(  current->next != NULL )
  {
    // If the current blcok is the one we want
    if( current == ptr)
    {
      if(previous->size_status % 2 == 0 && current->next->size_status % 2 == 0)
      { 
       // Coalesce blocks and set status to free
        previous->size_status+= current->size_status -1;
        previous->size_status +=  2*header_size +current->next->size_status;	
        previous->next = current->next->next;
      }
      else if( previous->size_status % 2 == 0)
      {
      //Coalesce blocks and set status to free /
        previous->size_status += current->size_status + header_size -1;
        previous->next = current->next;
      }
      else if( current->next->size_status % 2 == 0 )
      {
      //Coalesce blocks and set status to free /
        current->size_status += current->next->size_status + header_size -1;
        current->next = current->next->next;
      }
      else
      {
        // Size status as free
        current->size_status--;
      }
      return 0;
    }

    // Set block_headers for next loop iteration 
    previous = current;
    current = current->next;
  }

  // At end of the list check final block 
  if( current == ptr)
  {
    if( previous->size_status % 2 == 0)
    {
      previous->size_status += current->size_status + header_size -1;
      previous->next = NULL;
    }
    else
    {
      current->size_status--;
    }

    return 0;
  }

  // all blocks checked and block matching ptr not found */
  return -1;
}

void* Mem_Alloc_slab()
{
  if( slab_head == NULL)
    return Mem_Alloc_nextFit( globalSlabSize ); 
  // slab_head always free unless null
  void** currSlab = slab_head;
  void** prevSlab = currSlab;
  //iterate over until the node before is found
  //Last Free slot
  if( currSlab == *currSlab )
  {
    slab_head = NULL; 
  }
  else
  {
    while( *prevSlab != currSlab )
    {
      prevSlab = *prevSlab;
    }
    *prevSlab = *currSlab;
    slab_head = *prevSlab;
  }
  //zero mem
  memset( currSlab, '0', globalSlabSize );
  return currSlab;
}

int Mem_Free_slab( void * ptrIn)
{
  void ** ptr = (void **) ptrIn;
  //TODO check if this makes sense especially at first allocation
  if( (*ptr - slabRegionStartAddr) % globalSlabSize != 0)
    return -1;

  if( *ptr < slabRegionStartAddr || *ptr >= nextRegionStartAddr )
    return -1;

  //If we were empty before
  if( slab_head == NULL)
  { 
    slab_head = ptr;
    *ptr = ptr;
    return 0;
  }
  else if( ptr < s_head)//lower address than current lowest address
  {
    *s_tail = ptr;
    *ptr = s_head;
    s_head = ptr;
    return 0;
  }
  else if( ptr > s_tail)//higher address than current highest address
  {
    *s_tail = ptr;
    *ptr = s_head;
    s_tail = ptr;
    return 0;
  }
  else
  {
    //slab should only exist between already existing free slabs
    void** prev = slab_head;
    void** start = slab_head; //to avoid infinite loop
    while( !(prev < ptr && (void **)(*prev) > ptr) )//do this until it's true
    {
      prev = *prev;
      if( start == prev )
      {
        printf("Could not find slab location in list\n");
        return -1;
      }
    }

    *ptr = *prev;
    *prev = ptr;
    return 0; 
  }

}


/* Function to be used for debug */
/* Prints out a list of all the blocks along with the following information for each block */
/* No.      : Serial number of the block */
/* Status   : free/busy */
/* Begin    : Address of the first useful byte in the block */
/* End      : Address of the last byte in the block */
/* Size     : Size of the block (excluding the header) */
/* t_Size   : Size of the block (including the header) */
/* t_Begin  : Address of the first byte in the block (this is where the header starts) */
void Mem_Dump()
{

}
