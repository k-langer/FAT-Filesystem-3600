/*
 * CS3600, Spring 2013
 * Project 2 Starter Code
 * (c) 2013 Alan Mislove
 *
 */
#define MAGIC_NUMBER 0xBADB10CC
#define BLOCKSIZE 512
typedef char BYTE;

typedef struct VCB_s {
	unsigned int magic_number;
	int blocksize;
	int de_start;
	int de_length;
	int fat_start;
	int fat_length;
	int db_start;

	//root metadata
	uid_t user;
	gid_t group;
	mode_t mode;
	int access_time;
	int modify_time;
	int create_time;
} vcb;

//64 bytes 512/64=8
#define MAX_FILES 100
typedef struct dirent_s
{
	unsigned int valid;			//4 bytes
	unsigned int first_block;	//4 bytes
	unsigned int size;			//4 bytes
	uid_t user;					//4 bytes
	gid_t group;				//4 bytes
	mode_t mode;				//4 bytes
	int access_time;			//4 bytes
	int modify_time;			//4 bytes
	int create_time;			//4 bytes
	char name[28];
} dirent;


//32 bits
typedef struct fatent_s
{
	unsigned int used:1;
	unsigned int eof:1;
	unsigned int next:30;
} fatent;


#ifndef __3600FS_H__
#define __3600FS_H__


#endif
