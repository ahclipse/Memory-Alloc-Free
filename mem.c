#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include "mymem.h"


void init_slab( int, void*);
void* Mem_alloc_slab();
void* Mem_Alloc_nextFit(int);

typedef struct block_hd{
  /* The blocks are maintained as a linked list */
  /* The blocks are ordered in the increasing order of addresses */
  struct block_hd* next;

  /* size of the block is always a multiple of 4 */
  /* ie, last two bits are always zero - can be used to store other information*/
  /* LSB = 0 => free block */
  /* LSB = 1 => allocated/busy block */

  /* So for a free block, the value stored in size_status will be the same as the block size*/
  /* And for an allocated block, the value stored in size_status will be one more than the block size*/

  /* The value stored here does not include the space required to store the header */

  /* Example: */
  /* For a block with a payload of 24 bytes (ie, 24 bytes data + an additional 8 bytes for header) */
  /* If the block is allocated, size_status should be set to 25, not 24!, not 23! not 32! not 33!, not 31! */
  /* If the block is free, size_status should be set to 24, not 25!, not 23! not 32! not 33!, not 31! */
  int size_status;

}block_header;

block_header* list_head = NULL;
void* nextRegionStartAddr;
void* slabHead = NULL;
int allocated_once = 0;
void* nextRegionAddr;
int s_regionSize; 
int globalSlabSize;

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
  if( slabHead == NULL )
  {
    return NULL;
  }

  //Initialization of next fit memory 
  list_head = (block_header*)(nextRegionAddr);
  list_head->next = NULL;
  
  return memStart;
}

void init_slab( int slabSize, void* s_regionStart)
{
  void * nextSlab;
  void * currSlab;

  slabHead = s_regionStart;
  
  currSlab = s_regionStart;  
  nextSlab = (void *)(currSlab + slabSize);

  while( nextSlab - s_regionStart < s_regionSize)
  {
   // *currSlab = nextSlab; 
    //currSlab = nextSlab;
   /// nextSlab = currSlab + slabSize;
  }
 // *currSlab = NULL;
}

void* Mem_Alloc(int size)
{
  if( size == globalSlabSize)
    return Mem_alloc_slab();
  else
    return Mem_Alloc_nextFit(size);
}

/* Function for allocating 'size' bytes. */
/* Returns address of allocated block on success */
/* Returns NULL on failure */
/* Here is what this function should accomplish */
/* - Check for sanity of size - Return NULL when appropriate */
/* - Round up size to a multiple of 4 */
/* - Traverse the list of blocks and allocate the best free block which can accommodate the requested size */
/* -- Also, when allocating a block - split it into two blocks when possible */
void* Mem_Alloc_nextFit(int size)
{
  block_header *current;
  block_header *newblock;
  block_header *bestfit = NULL;
  int buff_size;
  int leftover;

  if( size < 1  ){
    return NULL;
  }

  /* align size with 4 bytes */
  if( size % 16 != 0)
    buff_size = size + ( 16 - ( size % 16) );
  else
    buff_size = size;

  /* initialize current */
  current = list_head;

  /* find a single available block that has size bits */
  while(bestfit == NULL)
  {
  /* if block is available */
    if(current->size_status % 2 != 1)
    {
      /* no better fit than exact */
      if( current->size_status == buff_size )
      {
        current->size_status++;
        return current + 1;
      }
      
      if( current->size_status > buff_size)
      {
        bestfit = current;
      }
    }

    /* true if at end of the list,with no value that fits */
    if( current->next == NULL && bestfit == NULL)   
    {
      return NULL;
    }
    else
    {
      current = current->next;
    }

  }

  /* Look through the rest of the list besiides last element for fit*/
  while( current != NULL )
  {
    /* if block is available*/
    if(current->size_status % 2 != 1)
    {
      /* No better fit than exact */
      if( current->size_status == buff_size )
      {
        current->size_status++;
        return current + 1;
      }
      
      if( current->size_status > buff_size
         && current->size_status < bestfit->size_status )
      {
        bestfit = current;
      }
    }
    current = current->next;
  }

  /*Is block big enough for request, header, and smallest requestable size(4)*/
  if(  bestfit->size_status >= buff_size + (int)sizeof(block_header) + 4 )
  {
    newblock = (block_header *)( (( char *) (bestfit + 1)) + buff_size  ) ;  

  /*Remaining space in blcok, status will be zero by default because aligned*/
    leftover = bestfit->size_status - buff_size - (int)sizeof(block_header);
    newblock->size_status = leftover;
    
    newblock->next = bestfit->next;
    bestfit->next = newblock;
    bestfit->size_status = buff_size;
  }
  
/* To indicate the block is busy*/
  bestfit->size_status++;
  return bestfit + 1;
 
}

/* Function for freeing up a previously allocated block */
/* Argument - ptr: Address of the block to be freed up */
/* Returns 0 on success */
/* Returns -1 on failure */
/* Here is what this function should accomplish */
/* - Return -1 if ptr is NULL */
/* - Return -1 if ptr is not pointing to the first byte of a busy block */
/* - Mark the block as free */
/* - Coalesce if one or both of the immediate neighbours are free */
int Mem_Free_nextFit(void *ptr)
{
  /* Pointers to block headers */
  block_header *previous;
  block_header *current;
  int header_size;
  
  if( ptr == NULL)
    return -1;

  /* Set up *ptr so it points to header not block itself */
  ptr = ((char *) ptr) - 8;

  /*if block is not busy something is wrong
  
  if ptr doest point to a header it would still -1 as would 
  failing this test, there is nothing lost by checking this before checking
  if it is present in list*/
  if( ((block_header *) ptr)->size_status % 2 == 0 )
  {
    return -1;
  }

  current = list_head;
  header_size = (int)sizeof(block_header);

/* if first block is the block to be freed */
  if( current == ptr )
  {
    if( current->next != NULL && current->next->size_status % 2 == 0)
    {
   /* coalesce blocks */
      current->size_status += current->next->size_status + header_size ;
      current->next = current->next->next;
    }

  /* Mark as free block */
    current->size_status --;  
    return 0;
  }

  /* set up  previous */
  previous = current;
  current = current->next;

  while(  current->next != NULL )
  {
    /* If the current blcok is the one we want */
    if( current == ptr)
    {
      if(previous->size_status % 2 == 0 && current->next->size_status % 2 == 0)
      { 
       /* Coalesce blocks and set status to free*/
        previous->size_status+= current->size_status -1;
        previous->size_status +=  2*header_size +current->next->size_status;	
        previous->next = current->next->next;
      }
      else if( previous->size_status % 2 == 0)
      {
      /*Coalesce blocks and set status to free */
        previous->size_status += current->size_status + header_size -1;
        previous->next = current->next;
      }
      else if( current->next->size_status % 2 == 0 )
      {
      /*Coalesce blocks and set status to free */
        current->size_status += current->next->size_status + header_size -1;
        current->next = current->next->next;
      }
      else
      {
        /* Size status as free*/
        current->size_status--;
      }
      return 0;
    }

    /* Set block_headers for next loop iteration */
    previous = current;
    current = current->next;
  }

  /* At end of the list check final block */
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

  /* all blocks checked and block matching ptr not found */
  return -1;
}




void* Mem_alloc_slab()
{
  return NULL;
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
  int counter;
  block_header* current = NULL;
  char* t_Begin = NULL;
  char* Begin = NULL;
  int Size;
  int t_Size;
  char* End = NULL;
  int free_size;
  int busy_size;
  int total_size;
  char status[5];

  free_size = 0;
  busy_size = 0;
  total_size = 0;
  current = list_head;
  counter = 1;
  fprintf(stdout,"************************************Block list***********************************\n");
  fprintf(stdout,"No.\tStatus\tBegin\t\tEnd\t\tSize\tt_Size\tt_Begin\n");
  fprintf(stdout,"---------------------------------------------------------------------------------\n");
  while(NULL != current)
  {
    t_Begin = (char*)current;
    Begin = t_Begin + (int)sizeof(block_header);
    Size = current->size_status;
    strcpy(status,"Free");
    if(Size & 1) /*LSB = 1 => busy block*/
    {
      strcpy(status,"Busy");
      Size = Size - 1; /*Minus one for ignoring status in busy block*/
      t_Size = Size + (int)sizeof(block_header);
      busy_size = busy_size + t_Size;
    }
    else
    {
      t_Size = Size + (int)sizeof(block_header);
      free_size = free_size + t_Size;
    }
    End = Begin + Size;
    fprintf(stdout,"%d\t%s\t0x%08lx\t0x%08lx\t%d\t%d\t0x%08lx\n",counter,status,(unsigned long int)Begin,(unsigned long int)End,Size,t_Size,(unsigned long int)t_Begin);
    total_size = total_size + t_Size;
    current = current->next;
    counter = counter + 1;
  }
  fprintf(stdout,"---------------------------------------------------------------------------------\n");
  fprintf(stdout,"*********************************************************************************\n");

  fprintf(stdout,"Total busy size = %d\n",busy_size);
  fprintf(stdout,"Total free size = %d\n",free_size);
  fprintf(stdout,"Total size = %d\n",busy_size+free_size);
  fprintf(stdout,"*********************************************************************************\n");
  fflush(stdout);
  return;
}
