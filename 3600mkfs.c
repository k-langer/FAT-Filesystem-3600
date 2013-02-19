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


void myformat(int size) {
    // Do not touch or move this function
    dcreate_connect();
    if (size < 256) {
      	printf("Disk to small to format\n");
      	return;
    }
    //size = (k+k/100) 
    //int k = ((size-101)*129)/128; //80 data blocks, 8 directory blocks, and 1 fat block
    char *tmp = (char *) malloc(BLOCKSIZE);
    int place = 0;
    memset(tmp, 0, BLOCKSIZE);
    vcb* vcb_block;
    vcb_block = (vcb*)tmp;
    vcb_block->magic_number = MAGIC_NUMBER;
    vcb_block->blocksize = BLOCKSIZE;
    vcb_block->de_start = 1;
    vcb_block->de_length = MAX_FILES;
    vcb_block->fat_start = vcb_block->de_length+1; 
    vcb_block->fat_length = (size-MAX_FILES-1)/128;
    vcb_block->db_start = vcb_block->de_length+vcb_block->fat_length+1;
    //printf("de_start %d\nde_length %d\nfat_start %d\nfat_length %d\ndb_start %d\n",vcb_block->de_start,vcb_block->de_length,vcb_block->fat_start,vcb_block->fat_length,vcb_block->db_start);

     dwrite(place++, tmp);	
  	
    memset(tmp, 0, BLOCKSIZE); 
    dirent* dir_block = (dirent*)tmp;
    dir_block->size = DIRENT_SIZE;
    

    int start = place; 
    while(place<start+MAX_FILES)
  	dwrite(place++,tmp);
    
    start = place; 
    memset(tmp, 0, BLOCKSIZE); 
    fatent*fat_block = (fatent*)tmp;
    fat_block->used = 0;
     while(place<(start+(size-MAX_FILES-1)/128))
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
