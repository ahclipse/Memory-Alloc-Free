#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include "mymem.h"
#include <assert.h>
#include <pthread.h>

void init_slab( int, void*);
void* Mem_Alloc_slab();
void* Mem_Alloc_nextFit(int);
int Mem_Free_nextFit(void *);
int Mem_Free_slab(void *);

int allocated_once = 0;

//bookmark of where we left off last
struct FreeHeader* nf_marker = NULL;

//Head and tail free slab list
struct FreeHeader* s_head;

//Head and tail free nf list
struct FreeHeader* nf_head;
struct FreeHeader* nf_tail;

//start of regions
char* nextRegionStartAddr;
char* slabRegionStartAddr;
 
int globalSlabSize;

int totalMemSize;

//Lock for concurrent processes 
pthread_mutex_t lock;

int makeMultiple16( int input )
{
  if( input % 16 != 0)
  {
    input = input + (16 - (input % 16) );
  }
  return input;
}

void* Mem_Init(int regionSize, int slabSize)
{
  pthread_mutex_lock(&lock);
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

  regionSize = makeMultiple16( regionSize );
  slabSize = makeMultiple16( slabSize );

  //Set the value of the slab region
  int s_regionSize = (int)(.25 * regionSize); 
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

  //init Slab
  struct FreeHeader* nextSlab;
  struct FreeHeader* currSlab;

  s_head = (struct FreeHeader *)( memStart);
  slabRegionStartAddr = (char *)( memStart);  

  currSlab = s_head;
  //currSlab->length = slabSize - sizeof(struct FreeHeader);  
  nextSlab = (struct FreeHeader*)((char*)currSlab + slabSize);

  while( ((char *)(nextSlab) - (char *)(memStart)) < s_regionSize)
  {
    currSlab->next = nextSlab; 
    currSlab = nextSlab;
    nextSlab = (struct FreeHeader*)((char*)currSlab + slabSize);
  }
  currSlab->next = NULL;

  assert( s_head != s_head->next);
  
  //Initialization of next fit memory 
  nf_marker = (struct FreeHeader*)(nextRegionStartAddr);
  nf_head = nf_marker;
  nf_tail = nf_marker;
  nf_marker->length = (int)(.75*regionSize)- sizeof(struct FreeHeader);
  assert( nf_head != s_head);
  nf_marker->next = nf_marker;
  
  assert( s_head != s_head->next);
  printf("init complete\n");
  pthread_mutex_unlock(&lock);
  return memStart;
}

void* Mem_Alloc(int size)
{
  if(size <= 0 || !allocated_once )
    return NULL;

  if( size == globalSlabSize)
  {
    pthread_mutex_lock(&lock);
    return Mem_Alloc_slab(); 
  }
  else
  {
    //Make sure the rest of requested memory is 16 byte aligned 
    if(size % 16 != 0 )
    {
      size = size + (16 - (size % 16)); 
    }
    return Mem_Alloc_nextFit(size);
  }
}

int Mem_Free(void *ptr){
  if( (char *)(ptr)  < slabRegionStartAddr || (char *)(ptr) > (char *)(slabRegionStartAddr + totalMemSize) )
  {
    printf("SEGFAULT\n");
    return -1;
  }
  if( (char *)(ptr) >= nextRegionStartAddr )
  {
    pthread_mutex_lock(&lock);
    return Mem_Free_nextFit(ptr);
  }
  else
  {
    pthread_mutex_lock(&lock);
    return Mem_Free_slab(ptr);
  }
}

void* Mem_Alloc_nextFit(int size)
{

  struct FreeHeader* currHeader = nf_marker;
  struct FreeHeader* prevHeader = currHeader;

  //empty list
  if(nf_marker == NULL)
    return NULL;

  //if first node is not big enough
  if( currHeader->length < size  )
  {
    //find the first node that fits
    currHeader = currHeader->next;

    while( currHeader != nf_marker && currHeader->length < size  )
    {
      currHeader = currHeader->next;
    }

    // Already checked this and there is not enough room
    // Nothing found of the right size
    if(currHeader == nf_marker)
    {
      printf("Nothing found\n");
      pthread_mutex_unlock(&lock);
      return NULL;
    }
  }
  //split if need be

  //enough room for current request another header and some data(16 byte aligned)
  if( currHeader->length > size + sizeof(struct AllocatedHeader) )
  {
    int originalSize = currHeader->length;//error checking
    
    struct FreeHeader* newHeader = (struct FreeHeader *)( (char *)(currHeader + 1) + size );

    newHeader->next = currHeader->next;
    currHeader->next = newHeader;

    newHeader->length = currHeader->length - (size + sizeof(struct FreeHeader));
    currHeader->length = size;

    assert(currHeader->length + sizeof(struct FreeHeader) + (currHeader->next)->length == originalSize);
  }
  
  //get node before the current one
  while(prevHeader->next != currHeader)
  {
    prevHeader = prevHeader->next;
    assert(prevHeader != NULL);
    assert(prevHeader != currHeader);
  }

  //if only one in list
  if( prevHeader == currHeader)
  {
    nf_tail = NULL;
    nf_head = NULL;
    nf_marker = NULL;
  }
  else
  {//if more than on in the list
    prevHeader->next = currHeader->next;

    if( currHeader == nf_head )
    {
      nf_head = prevHeader->next;
    }

    if( currHeader == nf_tail )
    {
      nf_tail = prevHeader;
    }

    nf_marker = prevHeader->next;
  }

  //change header of curr to be allocated
  ((struct AllocatedHeader *)currHeader)->magic = (void *)(MAGIC);
  
  //return acutal address beyond block header
  pthread_mutex_unlock(&lock);
  return currHeader +1;//becasuse the header sizes are the same
}


/****************************************************************************************************/
int Mem_Free_nextFit(void *ptr)
{
  //Check to see if valid nextFit pointer is given
  char * nPtr = ((char *)ptr) - sizeof(struct AllocatedHeader); 
  struct AllocatedHeader * freeRequest = (struct AllocatedHeader *)(nPtr); 

  //Is given pointer a valid AllocatedHeader???
  if((freeRequest->magic) != (void *)MAGIC)
  {
    pthread_mutex_unlock(&lock);
    return -1; 
  }

  //start at the front of the list
  struct FreeHeader * start = s_head;
 
  //Check if freeRequest occurs after the head in memory
  if(freeRequest > (struct AllocatedHeader *)start)
  {
    



  }
  //Else the freeRequest occurs before the FreeHeader list  
  else
  {



  } 
  pthread_mutex_unlock(&lock);
  return -1;
}
/*************************************************************************************************/

void* Mem_Alloc_slab()
{
  if( s_head == NULL)
    return Mem_Alloc_nextFit( globalSlabSize ); 
  // slab_head always free unless null
  struct FreeHeader* currSlab = s_head;
  //iterate over until the node before is found
  //Last Free slot
  s_head = currSlab->next;//will be NULL if last itme in list 
  //zero mem
  memset( (void *)(currSlab), '0', globalSlabSize );
  printf("finish alloc\n");
  pthread_mutex_unlock(&lock);
  return (void *)(currSlab);
}

int Mem_Free_slab( void * ptr)
{
  //Is slab aligned
  if( ( (char*)(ptr) - slabRegionStartAddr) % globalSlabSize != 0)
  {
    printf("Bad ptr\n");
    pthread_mutex_unlock(&lock);
    return -1;
  }

  if( (char *)(ptr) < slabRegionStartAddr || (char *)(ptr) >= nextRegionStartAddr )
  {
    pthread_mutex_unlock(&lock);
    return -1;
  }

  struct FreeHeader * currSlab = (struct FreeHeader *)(ptr);
  //If we were empty before
  if( s_head == NULL)
  { 
    s_head = currSlab;
    currSlab->next = NULL;
    pthread_mutex_unlock(&lock);
    return 0;
  }
  else if( currSlab < s_head)//lower address than current lowest address
  {
    currSlab->next = s_head;
    s_head = ptr;
    pthread_mutex_unlock(&lock);
    return 0;
  }
  else
  {
    //slab should only exist between already existing free slabs
    struct FreeHeader * prev = s_head;
    struct FreeHeader * start = s_head; //to avoid infinite loop
    while( !(prev < currSlab && (prev->next > currSlab || prev->next == NULL) ))//do until prev is before curr and the node after prev is after currSlab
    {
      prev = prev->next;
      if( start == prev )
      {
        printf("Could not find slab location in list\n");
	pthread_mutex_unlock(&lock);
        return -1;
      }
    }

    currSlab->next = prev->next;
    prev->next = currSlab;
    pthread_mutex_unlock(&lock);
    return 0; 
  }

}


void Mem_Dump()
{ 
  struct FreeHeader * start = s_head;
  struct FreeHeader * curr = start;
  int i = 0;

  if( s_head != NULL)
  {
    printf("======== START TEST ========\n");
    printf("%d\t%p\t%p\n------------------------\n",i, (void*)(start),(void*)(start->next ));
    curr = curr->next;
    i++;
    while( curr != NULL )
    {
      printf("%d\t%p\t%p\n-----------------------\n",i, (void*)(curr),(void*)(curr->next));
      curr = curr->next;
      i++;
    }
  }
  else
  {
    printf("Full Slab space\n");
  }
  struct FreeHeader * nfstart = nf_head;
  struct FreeHeader * nfcurr = nfstart;
  i = 0;
 
  printf("\n======== Next Fit Free Mem ========\n");
  printf("%d\t%p\t%p\t%d\n------------------------\n",i, (void*)(nfstart),(void*)(nfstart->next ), nfstart->length);
  nfcurr = nfcurr->next;
  i++;
  while( nfcurr->next != nfstart )
  {
    printf("%d\t%p\t%p\n-----------------------\n",i, (void*)(nfcurr),(void*)(nfcurr->next));
    nfcurr = nfcurr->next;
    i++;
  } 

  printf("\n%d\n", (nfcurr->length)); 
 
}
