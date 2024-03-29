/* CS3600, Spring 2013
 * Project 2 Starter Code
 * (c) 2013 Alan Mislove
 *
 * This file contains all of the basic functions that you will need 
 * to implement for this project.  Please see the project handout
 * for more details on any particular function, and ask on Piazza if
 * you get stuck.
 */

#define FUSE_USE_VERSION 26

#ifdef linux
/* For pread()/pwrite() */
#define _XOPEN_SOURCE 500
#endif

#define _POSIX_C_SOURCE 199309

#include <time.h>
#include <fuse.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <assert.h>
#include <sys/statfs.h>

#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

#include "3600fs.h"
#include "disk.h"

vcb* vcBlock;

//Masking for cache
int dread_cache(int blocknum, char* buf);
int dwrite_cache(int blocknum,char* buf);
void cache_initialize(int size);
char* data_cache;
char* tags_cache;
int cache_size;
int attempted_reads;

//returns data block number of 'first' free block
static int find_free_block();

//returns dirent number matching path and pointer to dirent, returns -1 if not found
static int find_dirent(const char* path, dirent* dirEntry);

/*
 * Initialize filesystem. Read in file system metadata and initialize
 * memory structures. If there are inconsistencies, now would also be
 * a good time to deal with that. 
 *
 * HINT: You don't need to deal with the 'conn' parameter AND you may
 * just return NULL.
 *
 */
static void* vfs_mount(struct fuse_conn_info *conn) {
    fprintf(stderr, "vfs_mount called\n");
    dconnect();
    vcBlock = (vcb*)calloc(1, sizeof(vcb));
    dread_cache(0, vcBlock);
    cache_initialize(0);
    // Do not touch or move this code; connects the disk
    if(vcBlock->magic_number != MAGIC_NUMBER || vcBlock->mounted == 1) { 
	    printf("Cannot mount file system\n");
        dunconnect();        
        exit(1);
    }
    vcBlock->mounted = 1;
    dwrite_cache(0, vcBlock);	
    return NULL;
}

/*
 * Called when your file system is unmounted.
 *
 */
static void vfs_unmount (void *private_data) {
    fprintf(stderr,"total reads without cache: %d\n",attempted_reads);
    fprintf(stderr, "vfs_unmount called\n");
    if (!vcBlock) {
    	return;
    }
    dread_cache(0, vcBlock);
    vcBlock->mounted = 0;
    dwrite_cache(0, vcBlock);

    /* 3600: YOU SHOULD ADD CODE HERE TO MAKE SURE YOUR ON-DISK STRUCTURES
             ARE IN-SYNC BEFORE THE DISK IS UNMOUNTED (ONLY NECESSARY IF YOU
             KEEP DATA CACHED THAT'S NOT ON DISK */

    // Do not touch or move this code; unconnects the disk
    dunconnect();
}

/* 
 *
 * Given an absolute path to a file/directory (i.e., /foo ---all
 * paths will start with the root directory of the CS3600 file
 * system, "/"), you need to return the file attributes that is
 * similar stat system call.
 *
 * HINT: You must implement stbuf->stmode, stbuf->st_size, and
 * stbuf->st_blocks correctly.
 *
 */
static int vfs_getattr(const char *path, struct stat *stbuf) {
    fprintf(stderr, "vfs_getattr called\n");
   	
    // Do not mess with this code 

    stbuf->st_nlink = 1; // hard links
    stbuf->st_rdev  = 0;
    stbuf->st_blksize = BLOCKSIZE;
   
	//find the file first
	char* filename = strtok(path, "/");
	//we only support one root directory for now
	if (strtok(NULL, "/")) {
		return -1;
	}
	if (!vcBlock) {
		return -1;
	}
	dread_cache(0, vcBlock);
	if (vcBlock->de_length == 0) {
		return -ENOENT;
	}
	if (!filename) {
		stbuf->st_mode  = 0777 | S_IFDIR;
    		stbuf->st_uid     = vcBlock->user;
    		stbuf->st_gid     = vcBlock->group; 
	    	stbuf->st_atime   = vcBlock->access_time;
	    	stbuf->st_mtime   = vcBlock->modify_time;
	   	stbuf->st_ctime   = vcBlock->create_time;
	    	stbuf->st_size    = 0;
	    	stbuf->st_blocks  = 0;
	} else {
		dirent* dirEntry = (dirent*)calloc(1, sizeof(dirent));
		if (!dirEntry) {
			return -1;
		}
		if (!filename) {
			filename = ".";
			fprintf(stderr, ".....");
		}
		int block = find_dirent(path, dirEntry);
		if (block == -1) {
			return -ENOENT;
		} else {
			stbuf->st_mode = dirEntry->mode;
			stbuf->st_uid = dirEntry->user;
			stbuf->st_gid = dirEntry->group;
			stbuf->st_atime = dirEntry->access_time;
			stbuf->st_mtime = dirEntry->modify_time;
			stbuf->st_ctime = dirEntry->create_time;
			stbuf->st_size = dirEntry->size;
			stbuf->st_blocks = dirEntry->size / BLOCKSIZE;
		}
	}
    return 0;
}

/*
 * Given an absolute path to a directory (which may or may not end in
 * '/'), vfs_mkdir will create a new directory named dirname in that
 * directory, and will create it with the specified initial mode.
 *
 * HINT: Don't forget to create . and .. while creating a
 * directory.
 */
/*
 * NOTE: YOU CAN IGNORE THIS METHOD, UNLESS YOU ARE COMPLETING THE 
 *       EXTRA CREDIT PORTION OF THE PROJECT.  IF SO, YOU SHOULD
 *       UN-COMMENT THIS METHOD.
static int vfs_mkdir(const char *path, mode_t mode) {

  return -1;
} */

/** Read directory
 *
 * Given an absolute path to a directory, vfs_readdir will return 
 * all the files and directories in that directory.
 *
 * HINT:
 * Use the filler parameter to fill in, look at fusexmp.c to see an example
 * Prototype below
 *
 * Function to add an entry in a readdir() operation
 *
 * @param buf the buffer passed to the readdir() operation
 * @param name the file name of the directory entry
 * @param stat file attributes, can be NULL
 * @param off offset of the next entry or zero
 * @return 1 if buffer is full, zero otherwise
 * typedef int (*fuse_fill_dir_t) (void *buf, const char *name,
 *                                 const struct stat *stbuf, off_t off);
 *			   
 * Your solution should not need to touch offset and fi
 *
 */
static int vfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi)
{
	fprintf(stderr, "vfs_readdir called");
	if (strncmp(path, "/", 1)) {
		return -1;
	} else {
		if (!vcBlock) {
			return -1;
		}
		dread_cache(0, vcBlock);
		dirent* dirEntry = (dirent*)calloc(1, sizeof(dirent));
		if (!dirEntry) {
			return -1;
		}
		int block = vcBlock->de_start + offset;
		while (block - vcBlock->de_start < vcBlock->de_length) {
			dread_cache(block, dirEntry);
			if (dirEntry->create_time) {
				if (!filler(buf, dirEntry->name, NULL, block)) {
					return 0;
				}
			}	
			block++;
		}
		return 0;
	}
}

/*
 * Given an absolute path to a file (for example /a/b/myFile), vfs_create 
 * will create a new file named myFile in the /a/b directory.
 *
 */
static int vfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    	fprintf(stderr, "vfs_create called\n");
	char* filename = strtok(path, "/");	
	//only support root dir
	if (strtok(NULL, "/") || !filename) {
		return -1;
	}

	if (!vcBlock) {
		return -1;
	}
	dread_cache(0, vcBlock);
	dirent* dirEntry = (dirent*)calloc(1, sizeof(dirent));
	if (!dirEntry) {
		return -1;
	}

	int block = find_dirent(path, dirEntry);
	if (block != -1) {
		return -EEXIST;
	}

	char foundEmpty = 0;
	block = vcBlock->de_start;
	while(block - vcBlock->de_start < vcBlock->de_length) {
		dread_cache(block, dirEntry);
		if (!dirEntry->create_time) {
			foundEmpty = 1;
			break;
		} else {
			block++;
		}
	}
	
	if (foundEmpty) {
		strncpy(dirEntry->name, filename, 476);	//shouldn't be hardcoded
		dirEntry->mode = mode;
		dirEntry->user = geteuid();
		dirEntry->group = getegid();

		struct timespec currentTime;
		clock_gettime(CLOCK_REALTIME, &currentTime);
		dirEntry->access_time = currentTime.tv_sec;
		dirEntry->modify_time = currentTime.tv_sec;
		dirEntry->create_time = currentTime.tv_sec;
		dwrite_cache(block, dirEntry);
		return 0;
	} else {
		return -1;
	}
}

/*
 * The function vfs_read provides the ability to read data from 
 * an absolute path 'path,' which should specify an existing file.
 * It will attempt to read 'size' bytes starting at the specified
 * offset (offset) from the specified file (path)
 * on your filesystem into the memory address 'buf'. The return 
 * value is the amount of bytes actually read; if the file is 
 * smaller than size, vfs_read will simply return the most amount
 * of bytes it could read. 
 *
 * HINT: You should be able to ignore 'fi'
 *
 */
static int vfs_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi)
{
    /* 
        TODO Fix offset reads (and writes) 
    */
    fprintf(stderr,"vfs_read called on path %s for size %d at offset %d\n",path,size,offset);

    dirent* dirEntry = (dirent*)calloc(1,sizeof(dirent));
    if (!dirEntry) {
        return -1;
    }
    char* data_block = (char*)calloc(BLOCKSIZE, sizeof(char));
    if (!data_block) {
			return -1;
	}
    fatent* fatEntry = (fatent*)calloc(1, sizeof(fatent));
    if (!fatEntry) {
			return -1;
	}

    int block = find_dirent(path,dirEntry);
    if ( block == -1 ) {
        return -1;
    }
    if ( !vcBlock ) {
        return -1;
    }
    dread_cache(0,vcBlock);
    if ( !dirEntry->valid ) {
        return 0;
    }
    if ( offset >= dirEntry->size ) {
        return 0;
    }
    int data_block_num = dirEntry->first_block;
    int blocksToSkip = offset / BLOCKSIZE;
    char* eof_char;
    int eof = 0;
    int count = 0;
    int bytesRead = 0;
    int fatent_block_num = data_block_num / FATENTS_PER_BLOCK;
    int fatent_block_offset = data_block_num % FATENTS_PER_BLOCK;
    int byte_offset = 0;
    
    
    while (blocksToSkip--) {
	dread_cache(vcBlock->fat_start + fatent_block_num, data_block);
	memcpy(fatEntry, data_block + fatent_block_offset * sizeof(fatent), sizeof(fatent));
   	data_block_num = fatEntry->next;
	fatent_block_num = data_block_num / FATENTS_PER_BLOCK;
	fatent_block_offset = data_block_num % FATENTS_PER_BLOCK;
    }

    while (!eof && bytesRead < size)
    {
	if (count) {
	    byte_offset = 0;
	} else {
	    byte_offset = offset % BLOCKSIZE;
        }
	fatent_block_num = data_block_num / FATENTS_PER_BLOCK;
        fatent_block_offset = data_block_num % FATENTS_PER_BLOCK;
	dread_cache(vcBlock->db_start + data_block_num, data_block); 
        memcpy(buf+count*BLOCKSIZE, data_block + byte_offset, BLOCKSIZE - byte_offset);
	bytesRead += BLOCKSIZE - byte_offset;
        dread_cache(vcBlock->fat_start + fatent_block_num, data_block);  
        fprintf(stderr, "reading from block number %i\n", data_block_num);
	memcpy(fatEntry, data_block + fatent_block_offset * sizeof(fatent), sizeof(fatent));
	eof = fatEntry->eof;
        data_block_num = fatEntry->next;
        count++;
    }	
    eof_char = strchr(buf, EOF);
    if (eof_char) {
        *eof_char = 0;
    }
    
	free(data_block);
    free(fatEntry);
    free(dirEntry);
	return strlen(buf);    
}

/*
 * The function vfs_write will attempt to write 'size' bytes from 
 * memory address 'buf' into a file specified by an absolute 'path'.
 * It should do so starting at the specified offset 'offset'.  If
 * offset is beyond the current size of the file, you should pad the
 * file with 0s until you reach the appropriate length.
 *
 * You should return the number of bytes written.
 *
 * HINT: Ignore 'fi'
 */
static int vfs_write(const char *path, const char *buf, size_t size,
                     off_t offset, struct fuse_file_info *fi)
{
  /* 3600: NOTE THAT IF THE OFFSET+SIZE GOES OFF THE END OF THE FILE, YOU
           MAY HAVE TO EXTEND THE FILE (ALLOCATE MORE BLOCKS TO IT). */
    

    fprintf(stderr,"vfs_write called on %s for size %d and offset %d\n",path,size,offset);
	dirent* dirEntry = (dirent*)calloc(2, sizeof(dirent));
	if (!dirEntry) {
		return -1;
	}
    int data_block_num = -1;	//data block number
	char fatentBlock = find_dirent(path, dirEntry);	
	if (fatentBlock == -1) {
		return -1;
	}
	int fat_count; 
    int fats;
    fatent* fatEntry = (fatent*)calloc(2, sizeof(fatent));
    if (!fatEntry) {
        return -1;
    }
    int fatent_block_num;	//block number to find the fatent
	int fatent_block_offset;	//offset within that block to find the fatent
    char* data_block = (char*)calloc(2*BLOCKSIZE, sizeof(char));
    if (!data_block) {
	    return -1;
    } 
    if (dirEntry->valid) {
	    data_block_num = dirEntry->first_block;
        int count = 0;
        while (offset/BLOCKSIZE && count < offset/BLOCKSIZE )
        {
            fatent_block_num = data_block_num / FATENTS_PER_BLOCK;
            fatent_block_offset = data_block_num % FATENTS_PER_BLOCK;
	        dread_cache(vcBlock->fat_start + fatent_block_num, data_block);
        	memcpy( fatEntry, data_block + fatent_block_offset * sizeof( fatent ), sizeof( fatent ) );
            data_block_num = fatEntry->next;
            if ( fatEntry->eof )
            {
                fatEntry->eof = 0;
                dwrite_cache(vcBlock->fat_start + fatent_block_num, data_block);
            }
            count++;
        }
        fprintf(stderr, "At data block %d\n", data_block_num);
    } else {
	    data_block_num = find_free_block();
        dirEntry->first_block = data_block_num;
    }
    if (data_block_num == -1 || data_block_num >= (vcBlock->fat_length * FATENTS_PER_BLOCK)) {
	    return -ENOSPC;
    }
    fat_count = 0; 
    fats = size/BLOCKSIZE + 1;
    int cpy_size = 0;
    while (fat_count < fats) {   
        fprintf(stderr,"Writing to data block #: %d\n", data_block_num);
	    int byte_offset = 0;
	if (!fat_count) {
		byte_offset = offset%BLOCKSIZE;;
	}
	    dread_cache(vcBlock->db_start + data_block_num, data_block);	   
        if (fat_count == fats-1) {
            cpy_size = size - fat_count * BLOCKSIZE + (offset%BLOCKSIZE);
            *(data_block + byte_offset + cpy_size) = EOF;
        } else {
            cpy_size = BLOCKSIZE - byte_offset;
        }       
        
        memcpy(data_block + byte_offset, buf+BLOCKSIZE*fat_count, cpy_size);
	    dwrite_cache(vcBlock->db_start + data_block_num, data_block);
	    fatent_block_num = data_block_num / FATENTS_PER_BLOCK;
	    fatent_block_offset = data_block_num % FATENTS_PER_BLOCK;
	    dread_cache(vcBlock->fat_start + fatent_block_num, data_block);  
        fat_count++; 	   
	    memcpy(fatEntry, data_block + fatent_block_offset * sizeof( fatent ), sizeof( fatent ) ); 
        fatEntry->used = 1;
        memcpy(data_block + fatent_block_offset*sizeof( fatent ), fatEntry, sizeof(fatent));
        dwrite_cache(vcBlock->fat_start + fatent_block_num, data_block); 

        if ( fat_count == fats ) {
            data_block_num = -1;
             fatEntry->eof = 1;
        } else {
             if (fatEntry->eof) {
                 fatEntry->eof = 0;
             }
	     data_block_num = find_free_block();
             fatEntry->next = data_block_num;
        }      
        memcpy(data_block + fatent_block_offset*sizeof( fatent ), fatEntry, sizeof(fatent));
        dwrite_cache(vcBlock->fat_start + fatent_block_num, data_block); 
    }     
    dirEntry->valid = 1;
    dirEntry->size = size + offset;
	struct timespec currentTime;
	clock_gettime(CLOCK_REALTIME, &currentTime);
	dirEntry->modify_time = currentTime.tv_sec;
	dwrite_cache(fatentBlock, dirEntry);
	
    free(dirEntry);
    free(fatEntry);
    free(data_block);
	return size;
}

/**
 * This function deletes the last component of the path (e.g., /a/b/c you 
 * need to remove the file 'c' from the directory /a/b).
 */
static int vfs_delete(const char *path)
{
    
  /* 3600: NOTE THAT THE BLOCKS CORRESPONDING TO THE FILE SHOULD BE MARKED
           AS FREE, AND YOU SHOULD MAKE THEM AVAILABLE TO BE USED WITH OTHER FILES */
    
    fprintf(stderr, "vfs_delete called on %s\n",path);
	unsigned int lastSlash = 0; 
	unsigned int i;	
	dirent* dirEntry;
	if ( strncmp(path, "/", 1) ) {
		fprintf( stderr, "Directory is not valid!\n" );
		return -1;
	} else {
		if ( !vcBlock ) {
			fprintf( stderr, "vcBlock is not valid!\n" );
			return -1;
		}
		dread_cache(0, (char*) vcBlock);
		dirEntry = (dirent*) calloc(1, sizeof( dirent ) );
		if ( !dirEntry ) {
			fprintf( stderr, "dirEntry is not valid!\n" );
			return -1;
		}
		
		for (i = 0; i < strlen( path ); i++) {
			if ( path[i] == '/' ) 
				lastSlash = i;
		}
	}
				
	int block = vcBlock->de_start;
	fatent* fat_block = (fatent*)calloc(BLOCKSIZE, sizeof(char));
     if (!fat_block) {
	        return -1;
     }
	while ( block - vcBlock->de_start < vcBlock->de_length ) {
		dread_cache(block, (char*) dirEntry);
		for ( i = lastSlash+1; i < strlen( path ); i++ ) {
			if ( path[i] != dirEntry->name[i-1] )
				break;
			if ( i == strlen( path ) -1 ) {
                
                if ( dirEntry->valid) {
                    
                    int data_block_num = dirEntry->first_block;
                   
                   
                    int eof = 0;
                    int fatent_block_num;
                    int fatent_block_offset;
                    while (!eof && dirEntry->size > 0)
                    {
                        
                        fatent_block_num = data_block_num / FATENTS_PER_BLOCK;
                        fatent_block_offset = data_block_num % FATENTS_PER_BLOCK;
                        dread_cache(vcBlock->fat_start + fatent_block_num, fat_block);  
                        eof = fat_block[fatent_block_offset].eof;
                        data_block_num = fat_block[fatent_block_offset].next;
                        memset( fat_block+fatent_block_offset, 0 , sizeof(fatent));
                        dwrite_cache(vcBlock->fat_start + fatent_block_num,fat_block);
                    }	
                    free(fat_block);
                }
				memset( dirEntry, 0 , BLOCKSIZE );
				dwrite_cache( block, (char*) dirEntry );
				return 0;
			}
		}
		block++;
	}
    free(fat_block);
	fprintf( stderr, "File not found!\n" );
	return -1;
}

/*
 * The function rename will rename a file or directory named by the
 * string 'oldpath' and rename it to the file name specified by 'newpath'.
 *
 * HINT: Renaming could also be moving in disguise
 *
 */
static int vfs_rename(const char *from, const char *to)
{
	fprintf( stderr, "vfs_rename called from %s to %s\n",from,to );	
	dirent*dirEntry = (dirent*) calloc(1, sizeof( dirent ) );;
	if ( strncmp(from, "/", 1) ) {
		fprintf( stderr, "Directory is not valid!\n" );
		return -1;
	} else {
		if ( !vcBlock ) {
			fprintf( stderr, "vcBlock is not valid!\n" );
			return -1;
		}
		dread_cache(0, (char*) vcBlock);
		if ( !dirEntry ) {
			fprintf( stderr, "dirEntry is not valid!\n" );
			return -1;
		}	
	}
	int block = vcBlock->de_start;
	unsigned int i;
	while ( block - vcBlock->de_start < vcBlock->de_length ) {
		dread_cache(block, (char*) dirEntry);
		fprintf( stderr, "to: %s cmp: %s\n",to,dirEntry->name);
		for ( i = 1; i < strlen( from ); i++ ) {
			if ( from[i] != dirEntry->name[i-1] )
				break;
			if ( i == strlen( from ) -1 ) {
				memset( dirEntry->name, 0 , sizeof( dirEntry->name) );
				for (i = 1; i < strlen( to ); i++)
					dirEntry->name[i-1] = to[i];
				dwrite_cache( block, (char*) dirEntry );
				return 0;
			}
		}
		block++;
	}
    free(dirEntry);
	fprintf( stderr, "File not found!\n" );
	return -1;
}


/*
 * This function will change the permissions on the file
 * to be mode.  This should only update the file's mode.  
 * Only the permission bits of mode should be examined 
 * (basically, the last 16 bits).  You should do something like
 * 
 * fcb->mode = (mode & 0x0000ffff);
 *
 */
static int vfs_chmod(const char *file, mode_t mode)
{
    fprintf( stderr, "vfs_chmod called on %s with %o\n",file,mode );	
	dirent*dirEntry = (dirent*) calloc(1, sizeof( dirent ) );
	if ( strncmp(file, "/", 1) ) {
		fprintf( stderr, "Directory is not valid!\n" );
		return -1;
	} else {
		if ( !vcBlock ) {
			fprintf( stderr, "vcBlock is not valid!\n" );
			return -1;
		}
		dread_cache(0, (char*) vcBlock);
		if ( !dirEntry ) {
			fprintf( stderr, "dirEntry is not valid!\n" );
			return -1;
		}	
	}
	int block = vcBlock->de_start;
	unsigned int i;
	while ( block - vcBlock->de_start < vcBlock->de_length ) {
		dread_cache(block, (char*) dirEntry);
		for ( i = 1; i < strlen( file ); i++ ) {
			if ( file[i] != dirEntry->name[i-1] )
				break;
			if ( i == strlen( file ) -1 ) {
                dirEntry->mode = mode;
				dwrite_cache( block, (char*) dirEntry );
				return 0;
			}
		}
		block++;
	}
    free(dirEntry);
	fprintf( stderr, "File not found!\n" );
	return -1;
}

/*
 * This function will change the user and group of the file
 * to be uid and gid.  This should only update the file's owner
 * and group.
 */
static int vfs_chown(const char *file, uid_t uid, gid_t gid)
{
	fprintf( stderr, "vfs_chown called on %s with %d %d\n", file , uid , gid );	
	dirent*dirEntry = (dirent*) calloc(1, sizeof( dirent ) );;
	if ( strncmp(file, "/", 1) ) {
		fprintf( stderr, "Directory is not valid!\n" );
		return -1;
	} else {
		if ( !vcBlock ) {
			fprintf( stderr, "vcBlock is not valid!\n" );
			return -1;
		}
		dread_cache(0, (char*) vcBlock); 
		if ( !dirEntry ) {
			fprintf( stderr, "dirEntry is not valid!\n" );
			return -1;
		}	
	}
	int block = vcBlock->de_start;
	unsigned int i;
	while ( block - vcBlock->de_start < vcBlock->de_length ) {
		dread_cache(block, (char*) dirEntry);
		for ( i = 1; i < strlen( file ); i++ ) {
			if ( file[i] != dirEntry->name[i-1] )
				break;
			if ( i == strlen( file ) -1 ) {
                if ( uid != -1 ) {
                    dirEntry->user = uid;
                }
                if ( gid != -1 ) {
                    dirEntry->group = gid;
                }
				dwrite_cache( block, (char*) dirEntry );
				return 0;
			}
		}
		block++;
	}
    free(dirEntry);
	fprintf( stderr, "File not found!\n" );
	return -1;
}

/*
 * This function will update the file's last accessed time to
 * be ts[0] and will update the file's last modified time to be ts[1].
 */
static int vfs_utimens(const char *file, const struct timespec ts[2])
{
    fprintf(stderr,"vfs_utimens called on file %s to update to current time\n",file);
    dirent* dirEntry = (dirent*)calloc(1,sizeof(dirent));
    if (!dirEntry) {
        return -1;
    }
    int block = find_dirent(file,dirEntry);
    dirEntry->access_time = ts[0].tv_sec;
    dirEntry->modify_time = ts[1].tv_sec;
    dwrite_cache(block,dirEntry);
    free(dirEntry);
	return 0;
}

/*
 * This function will truncate the file at the given offset
 * (essentially, it should shorten the file to only be offset
 * bytes long).
 */
static int vfs_truncate(const char *file, off_t offset)
{
	/* 3600: NOTE THAT ANY BLOCKS FREED BY THIS OPERATION SHOULD
           BE AVAILABLE FOR OTHER FILES TO USE. */
    fprintf(stderr,"vfs_truncate called on file %s for offset %d\n",file,offset);
	dirent* dirEntry = (dirent*)calloc(1,sizeof(dirent));
    if (!dirEntry) {
        return -1;
    }
    
    int block = find_dirent(file,dirEntry);
    if ( block == -1 ) {
        return -1;
    }
    if ( !vcBlock ) {
        return -1;
    }
    dread_cache(0,vcBlock);
    if ( !dirEntry->valid ) {
        return 0;
    }
    if ( dirEntry-> size < offset)
        return -1;
    dirEntry-> size = offset;
    int data_block_num = dirEntry->first_block;
    dwrite_cache(block,dirEntry);
    char* eof_char;
    char* data_block = (char*)calloc(BLOCKSIZE, sizeof(char));
    if (!data_block) {
			return -1;
	}
    fatent* fat_block = (fatent*)calloc(BLOCKSIZE, sizeof(char));
    if (!fat_block) {
			return -1;
	}
    int eof = 0;
    int count = 0;
    int fatent_block_num;
	int fatent_block_offset;
    while (!eof)
    {
        fatent_block_num = data_block_num / FATENTS_PER_BLOCK;
        fatent_block_offset = data_block_num % FATENTS_PER_BLOCK;
	    dread_cache(vcBlock->fat_start + fatent_block_num, fat_block);  
	    dread_cache(vcBlock->db_start + data_block_num, data_block); 
        eof = fat_block[fatent_block_offset].eof;
        if (count == offset/BLOCKSIZE) { 
            *(data_block+offset%BLOCKSIZE) = EOF;
            fat_block->eof = 1;
            dwrite_cache(vcBlock->fat_start + fatent_block_num, fat_block);  
            dwrite_cache(vcBlock->db_start + data_block_num, data_block);
        }
        data_block_num = fat_block[fatent_block_offset].next;
        if (count > offset/BLOCKSIZE || offset == 0) {
            memset( fat_block+fatent_block_offset, 0 , sizeof(fatent));
            dwrite_cache(vcBlock->fat_start + fatent_block_num,fat_block);
        }
        count++;
        
    }	
    free(dirEntry);
    free(data_block);
    free(fat_block);
	return 0;
}


/*
 * You shouldn't mess with this; it sets up FUSE
 *
 * NOTE: If you're supporting multiple directories for extra credit,
 * you should add 
 *
 *     .mkdir	 = vfs_mkdir,
 */
static struct fuse_operations vfs_oper = {
    .init    = vfs_mount,
    .destroy = vfs_unmount,
    .getattr = vfs_getattr,
    .readdir = vfs_readdir,
    .create	 = vfs_create,
    .read	 = vfs_read,
    .write	 = vfs_write,
    .unlink	 = vfs_delete,
    .rename	 = vfs_rename,
    .chmod	 = vfs_chmod,
    .chown	 = vfs_chown,
    .utimens	 = vfs_utimens,
    .truncate	 = vfs_truncate,
};

static int find_free_block() {
	if (!vcBlock) {
		return -1;
	}
    fatent* fatList = (fatent*) calloc(512,sizeof(char));
    dread_cache(vcBlock->fat_start,fatList);
    int fatent_offset = 0;
    int block_offset = 0;    
    while (fatList[fatent_offset % FATENTS_PER_BLOCK].used) {
        fatent_offset++;
        if ( fatent_offset % FATENTS_PER_BLOCK == 0 ) {
            block_offset++;
            dread_cache(vcBlock->fat_start + block_offset, fatList);
        }
    }
    if ( fatList[fatent_offset % FATENTS_PER_BLOCK].used ) {
        return -1; 
    }
    free( fatList );
    return fatent_offset;
}

static int find_dirent(const char* path, dirent* dirEntry) {
	char* filename = strtok(path, "/");	
	//only support root dir
	if (strtok(NULL, "/") || !filename) {
		return -1;
	}

	if (!vcBlock || !dirEntry) {
		return -1;
	}
	dread_cache(0, vcBlock);
	char found = 0;
	int block = vcBlock->de_start;
	while(block - vcBlock->de_start < vcBlock->de_length) {
		dread_cache(block, dirEntry);
		if (strlen(filename) == strlen(dirEntry->name) &&
		    strncmp(filename, dirEntry->name, strlen(filename)) == 0) {
			found = 1;
			break;
		} else {
			block++;
		}
	}
	if (found) {
		return block;
	} else {
		return -1;
	}
}
void cache_initialize(int size) { 
    cache_size = 500;
    attempted_reads = 0;
    data_cache = calloc(cache_size, BLOCKSIZE);
    tags_cache = calloc(cache_size,sizeof(int));
    if (data_cache == 0) { 
        free(tags_cache);
    }
    if (tags_cache == 0) {
        free(data_cache);
    }
    if (tags_cache && data_cache) {
        memset( tags_cache, -1, cache_size*sizeof(int) );
        fprintf(stderr,"Cache enabled with size %d blocks\n",cache_size);
    }
    return;
}
int dread_cache(int blocknum, char* buf) {
    attempted_reads++;
    int ret = BLOCKSIZE;
    if ( data_cache && tags_cache ) {
        if (tags_cache[blocknum%cache_size] == blocknum) {          
            memcpy(buf, data_cache + BLOCKSIZE*(blocknum%cache_size),BLOCKSIZE);            
            return ret;
        }
    }     
    ret  = dread(blocknum,buf);
    if ( data_cache && tags_cache ) {
        tags_cache[blocknum%cache_size] = blocknum;
        memcpy(data_cache + BLOCKSIZE*(blocknum%cache_size), buf, BLOCKSIZE);
    }
    return ret;
}
int dwrite_cache(int blocknum, char* buf) {
    if ( data_cache && tags_cache ) {
        tags_cache[blocknum%cache_size] = blocknum;
        memcpy(data_cache + BLOCKSIZE*(blocknum%cache_size), buf, BLOCKSIZE);
    }
    return dwrite(blocknum,buf);
}
int main(int argc, char *argv[]) {
    /* Do not modify this function */
    umask(0);
    if ((argc < 4) || (strcmp("-s", argv[1])) || (strcmp("-d", argv[2]))) {
      printf("Usage: ./3600fs -s -d <dir>\n");
      exit(-1);
    }
    return fuse_main(argc, argv, &vfs_oper, NULL);
}

