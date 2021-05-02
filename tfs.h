/*
 *  Copyright (C) 2021 CS416 Rutgers CS
 *	Tiny File System
 *
 *	File:	tfs.h
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/limits.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <libgen.h>
#include <limits.h>
#include <fuse.h>
#include <math.h>
#include <pthread.h>
#include "block.h"

#ifndef _TFS_H
#define _TFS_H

#define MAGIC_NUM 0x5C3A
#define MAX_INUM 1024
#define MAX_DNUM 16384
#define FUSE_USE_VERSION 26 
#define TFS_DIR 1
#define TFS_FILE 2

typedef struct superblock {
	uint32_t	magic_num;			/* magic number */
	uint16_t	max_inum;			/* maximum inode number */
	uint16_t	max_dnum;			/* maximum data block number */
	uint32_t	i_bitmap_blk;		/* start block of inode bitmap */
	uint32_t	d_bitmap_blk;		/* start block of data block bitmap */
	uint32_t	i_start_blk;		/* start block of inode region */
	uint32_t	d_start_blk;		/* start block of data block region */
} sb;

struct inode {
	uint16_t	ino;				/* inode number */
	uint16_t	valid;				/* validity of the inode */
	uint32_t	size;				/* size of the file */
	uint32_t	type;				/* type of the file */
	uint32_t	link;				/* link count */
	int			direct_ptr[16];		/* direct pointer to data block */
	int			indirect_ptr[8];	/* indirect pointer to data block */
	struct stat	vstat;				/* inode stat */
};

struct dirent {
	uint16_t ino;					/* inode number of the directory entry */
	uint16_t valid;					/* validity of the directory entry */
	char name[208];					/* name of the directory entry */
	uint16_t len;					/* length of name */
};


/*
 * bitmap operations
 */
typedef unsigned char* bitmap_t;

void set_bitmap(bitmap_t b, int i) {
    b[i / 8] |= 1 << (i & 7);
}

void unset_bitmap(bitmap_t b, int i) {
    b[i / 8] &= ~(1 << (i & 7));
}

uint8_t get_bitmap(bitmap_t b, int i) {
    return b[i / 8] & (1 << (i & 7)) ? 1 : 0;
}

// User functions and variables
pthread_mutex_t lock;
struct inode* create_inode(int ino, int inodeType);
void count_blocks();
#endif
