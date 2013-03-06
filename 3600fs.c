/*
 * CS3600, Spring 2013
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
    dread(0, vcBlock);
    // Do not touch or move this code; connects the disk
    if(vcBlock->magic_number != MAGIC_NUMBER) { 
	printf("Cannot mount file system\n");
        exit(1);
    }	
    return NULL;
}

/*
 * Called when your file system is unmounted.
 *
 */
static void vfs_unmount (void *private_data) {
    fprintf(stderr, "vfs_unmount called\n");

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
	dread(0, vcBlock);
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
		dread(0, vcBlock);
		dirent* dirEntry = (dirent*)calloc(1, sizeof(dirent));
		if (!dirEntry) {
			return -1;
		}
		int block = vcBlock->de_start + offset;
		while (block - vcBlock->de_start < vcBlock->de_length) {
			dread(block, dirEntry);
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
	dread(0, vcBlock);
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
		dread(block, dirEntry);
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
		dwrite(block, dirEntry);
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
	dirent* dirEntry = (dirent*)calloc(1, sizeof(dirent));
	if (!dirEntry) {
		return -1;
	}

	int block = find_dirent(path, dirEntry);
	if (block == -1) {
		return -1;
	} else {
		if (!vcBlock) {
			return -1;
		}
		dread(0, vcBlock);
		if (!dirEntry->valid) {
			return 0;
		}
		int data_block_num = dirEntry->first_block;
		char* data_block = (char*)calloc(BLOCKSIZE, sizeof(char));
		if (!data_block) {
			return -1;
		}
		dread(vcBlock->db_start + data_block_num, data_block);
		memcpy(buf, data_block, size);
		char* eof = strchr(buf, EOF);
		*eof = 0;
	}
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
	
	dirent* dirEntry = (dirent*)calloc(1, sizeof(dirent));
	if (!dirEntry) {
		return -1;
	}
	int block = find_dirent(path, dirEntry);
	
	if (block == -1) {
		return -1;
	} else {
		int data_block_num = -1;
		if (dirEntry->valid) {
			data_block_num = dirEntry->first_block;
		} else {
			data_block_num = find_free_block();
		}
		if (data_block_num == -1 || data_block_num >= (vcBlock->fat_length * FATENTS_PER_BLOCK)) {
			return -ENOSPC;
		}
		int byte_offset = offset;
		char* data_block = (char*)calloc(BLOCKSIZE, sizeof(char));
		dread(vcBlock->db_start + data_block_num, data_block);
		memcpy(data_block + byte_offset, buf, size);
		*(data_block + byte_offset + size) = EOF;
		dwrite(vcBlock->db_start + data_block_num, data_block);

		int fatent_block_num = data_block_num / FATENTS_PER_BLOCK;
		int fatent_block_offset = data_block_num % FATENTS_PER_BLOCK;
		dread(vcBlock->fat_start + fatent_block_num, data_block);
		fatent* fatEntry = (fatent*)calloc(1, sizeof(fatent));
		memcpy(data_block + fatent_block_offset * sizeof(fatent), fatEntry, sizeof(fatent));
		fatEntry->used = 1;
		fatEntry->eof = 1;
		memcpy(fatEntry, data_block + fatent_block_offset, sizeof(fatent));
		dwrite(vcBlock->fat_start + fatent_block_num, data_block);
		free(fatEntry);
		free(data_block);

		dirEntry->first_block = data_block_num;
		dirEntry->valid = 1;
		dirEntry->size = size + offset;
		struct timespec currentTime;
		clock_gettime(CLOCK_REALTIME, &currentTime);
		dirEntry->modify_time = currentTime.tv_sec;
		dwrite(block, dirEntry);
	}
	free(dirEntry);
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
		dread(0, (char*) vcBlock);
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
	
	while ( block - vcBlock->de_start < vcBlock->de_length ) {
		dread(block, (char*) dirEntry);
		for ( i = lastSlash+1; i < strlen( path ); i++ ) {
			if ( path[i] != dirEntry->name[i-1] )
				break;
			if ( i == strlen( path ) -1 ) {
				memset( dirEntry, 0 , BLOCKSIZE );
				dwrite( block, (char*) dirEntry );
				return 0;
			}
		}
		block++;
	}
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
	dirent*dirEntry;
	if ( strncmp(from, "/", 1) ) {
		fprintf( stderr, "Directory is not valid!\n" );
		return -1;
	} else {
		if ( !vcBlock ) {
			fprintf( stderr, "vcBlock is not valid!\n" );
			return -1;
		}
		dread(0, (char*) vcBlock);
		dirEntry = (dirent*) calloc(1, sizeof( dirent ) );
		if ( !dirEntry ) {
			fprintf( stderr, "dirEntry is not valid!\n" );
			return -1;
		}	
	}
	int block = vcBlock->de_start;
	unsigned int i;
	while ( block - vcBlock->de_start < vcBlock->de_length ) {
		dread(block, (char*) dirEntry);
		fprintf( stderr, "to: %s cmp: %s\n",to,dirEntry->name);
		/*
        //TODO Fix duplicate name files on rename
        for ( i = 1; i < strlen( to ); i++ ) {
		    if ( to[i] != dirEntry->name[i-1] || strlen( dirEntry->name ) != strlen( to ) -1 ) {
			    break; 		
		    } 
            if ( i == strlen( to ) ){
		        return -EEXIST;
            }
		}
        */
		for ( i = 1; i < strlen( from ); i++ ) {
			if ( from[i] != dirEntry->name[i-1] )
				break;
			if ( i == strlen( from ) -1 ) {
				memset( dirEntry->name, 0 , sizeof( dirEntry->name) );
				for (i = 1; i < strlen( to ); i++)
					dirEntry->name[i-1] = to[i];
				dwrite( block, (char*) dirEntry );
				return 0;
			}
		}
		block++;
	}
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
	dirent*dirEntry;
	if ( strncmp(file, "/", 1) ) {
		fprintf( stderr, "Directory is not valid!\n" );
		return -1;
	} else {
		if ( !vcBlock ) {
			fprintf( stderr, "vcBlock is not valid!\n" );
			return -1;
		}
		dread(0, (char*) vcBlock);
		dirEntry = (dirent*) calloc(1, sizeof( dirent ) );
		if ( !dirEntry ) {
			fprintf( stderr, "dirEntry is not valid!\n" );
			return -1;
		}	
	}
	int block = vcBlock->de_start;
	unsigned int i;
	while ( block - vcBlock->de_start < vcBlock->de_length ) {
		dread(block, (char*) dirEntry);
		for ( i = 1; i < strlen( file ); i++ ) {
			if ( file[i] != dirEntry->name[i-1] )
				break;
			if ( i == strlen( file ) -1 ) {
                dirEntry->mode = mode;
				dwrite( block, (char*) dirEntry );
				return 0;
			}
		}
		block++;
	}
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
	dirent*dirEntry;
	if ( strncmp(file, "/", 1) ) {
		fprintf( stderr, "Directory is not valid!\n" );
		return -1;
	} else {
		if ( !vcBlock ) {
			fprintf( stderr, "vcBlock is not valid!\n" );
			return -1;
		}
		dread(0, (char*) vcBlock);
		dirEntry = (dirent*) calloc(1, sizeof( dirent ) );
		if ( !dirEntry ) {
			fprintf( stderr, "dirEntry is not valid!\n" );
			return -1;
		}	
	}
	int block = vcBlock->de_start;
	unsigned int i;
	while ( block - vcBlock->de_start < vcBlock->de_length ) {
		dread(block, (char*) dirEntry);
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
				dwrite( block, (char*) dirEntry );
				return 0;
			}
		}
		block++;
	}
	fprintf( stderr, "File not found!\n" );
	return -1;
}

/*
 * This function will update the file's last accessed time to
 * be ts[0] and will update the file's last modified time to be ts[1].
 */
static int vfs_utimens(const char *file, const struct timespec ts[2])
{
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

	int fatent_offset = 0;
	int block_offset = 0;
	char* tempBlock = (char*) calloc(512, sizeof(char));
	dread(vcBlock->fat_start, tempBlock);
	fatent* fatEntry = (fatent*) calloc(1, sizeof(fatent));
	memcpy(fatEntry, tempBlock, sizeof(fatent));

	while(fatEntry->used) {
		fatent_offset++;
		if (fatent_offset % FATENTS_PER_BLOCK == 0) {
			block_offset++;
			dread(vcBlock->fat_start + block_offset, tempBlock);
		}
		memcpy(fatEntry, tempBlock + (fatent_offset % FATENTS_PER_BLOCK) * sizeof(fatent), sizeof(fatent));
	}
	free(tempBlock);
	if (fatEntry->used) {
		free(fatEntry);
		return -1;
	} else {
		free(fatEntry);
		return fatent_offset;
	}
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
	dread(0, vcBlock);
	char found = 0;
	int block = vcBlock->de_start;
	while(block - vcBlock->de_start < vcBlock->de_length) {
		dread(block, dirEntry);
		if (strncmp(filename, dirEntry->name, strlen(filename)) == 0) {
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

int main(int argc, char *argv[]) {
    /* Do not modify this function */
    umask(0);
    if ((argc < 4) || (strcmp("-s", argv[1])) || (strcmp("-d", argv[2]))) {
      printf("Usage: ./3600fs -s -d <dir>\n");
      exit(-1);
    }
    return fuse_main(argc, argv, &vfs_oper, NULL);
}

