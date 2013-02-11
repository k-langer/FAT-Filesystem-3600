/*
* CS3600, Spring 2013
* Project 2 Starter Code
* (c) 2013 Alan Mislove
*
* This program is intended to format your disk file, and should be executed
* BEFORE any attempt is made to mount your file system. It will not, however
* be called before every mount (you will call it manually when you format
* your disk file).
*/

#include <math.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "3600fs.h"
#include "disk.h"

#define MAGIC_NUMBER 0xBADB10CC
typedef char BYTE;

typedef struct VCB_s
{
int magic_number;
int blocksize;
int de_start;
int de_length;
int fat_start;
int fat_length;
int db_start;
uid_t user;
gid_t group;
mode_t mode;
int access_time;
int modify_time;
int create_time;
BYTE unused[460];
}vcb;

//64 bytes 512/64=8
#define DIRENT_SIZE 64
typedef struct dirent_s
{
unsigned int valid;
unsigned int first_block;
unsigned int size;
uid_t user;
gid_t group;
mode_t mode;
int access_time;
int modify_time;
int create_time;
char name[28];
} dirent;


//5* 512/(32) = 80
typedef struct fatent_s
{
unsigned int used:1;
unsigned int eof:1;
unsigned int next:30;
} fatent;

void myformat(int size) {
  // Do not touch or move this function
  dcreate_connect();

  int i = size/89; //80 data blocks, 8 directory blocks, and 1 fat block
  char *tmp = (char *) malloc(BLOCKSIZE);
  int place = 0;
  memset(tmp, 0, BLOCKSIZE);
  vcb* vcb_block;
  vcb_block = (vcb*)tmp;
  vcb_block->magic_number = MAGIC_NUMBER;
  vcb_block->blocksize = BLOCKSIZE;
  vcb_block->de_start = 1;
  vcb_block->de_length = i*8;
  vcb_block->fat_start = i*8+1; 
  vcb_block->fat_length = i;
  vcb_block->db_start = 1+i*8+i;
  //printf("de_start %d\nde_length %d\nfat_start %d\nfat_length %d\ndb_start %d\n",vcb_block->de_start,vcb_block->de_length,vcb_block->fat_start,vcb_block->fat_length,vcb_block->db_start);
  //block->user = getuid();
  //block->group = getgid();
  //block->create_time = time();
   dwrite(place++, tmp);	
	
  memset(tmp, 0, BLOCKSIZE); 
  dirent* dir_block = (dirent*)tmp;
  dir_block->size = DIRENT_SIZE;
  

  int start = place; 
  while(place<(start+i*8))
	dwrite(place++,tmp);
  
  start = place; 
  memset(tmp, 0, BLOCKSIZE); 
  fatent*fat_block = (fatent*)tmp;
  fat_block->used = 0;
   while(place<(start+i))
	dwrite(place++,tmp);

 memset(tmp, 0, BLOCKSIZE);
  // now, write that to every block
  for (int i=place; i<size; i++)
    if (dwrite(i, tmp) < 0)
      perror("Error while writing to disk");
  free(tmp);
  // voila! we now have a disk containing all zeros

  // Do not touch or move this function
  dunconnect();
}

int main(int argc, char** argv) {
  // Do not touch this function
  if (argc != 2) {
    printf("Invalid number of arguments \n");
    printf("usage: %s diskSizeInBlockSize\n", argv[0]);
    return 1;
  }

  unsigned long size = atoi(argv[1]);
  printf("Formatting the disk with size %lu \n", size);
  myformat(size);
}
