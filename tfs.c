/*
 *  Copyright (C) 2021 CS416 Rutgers CS
 *	Tiny File System
 *	File:	tfs.c
 *
 */
#define FUSE_USE_VERSION 26

#include "tfs.h"

char diskfile_path[PATH_MAX];
sb* superblock;
int inodesPerBlock = BLOCK_SIZE/sizeof(struct inode);
// Declare your in-memory data structures here

/* 
 * Get available inode number from bitmap
 */
int get_avail_ino() {

	// Step 1: Read inode bitmap from disk
	bitmap_t inode_bitmap = malloc(BLOCK_SIZE);
	bio_read(1, inode_bitmap);

	// Step 2: Traverse inode bitmap to find an available slot
	int i;
	for(i = 0; i < superblock->max_inum; i++) {
		// Step 3: Update inode bitmap and write to disk 
		if(get_bitmap(inode_bitmap, i) == 0) {
			set_bitmap(inode_bitmap, i);
			bio_write(1, inode_bitmap);
			free(inode_bitmap);
			return 0;
		}
	}

	puts("No available inode number found.");	
	free(inode_bitmap);
	return -1;
}

/* 
 * Get available data block number from bitmap
 */
int get_avail_blkno() {

	// Step 1: Read data block bitmap from disk
	bitmap_t data_bmap = malloc(BLOCK_SIZE);
	bio_read(2, data_bmap);

	// Step 2: Traverse data block bitmap to find an available slot
	int i;
	for(i = 0; i < superblock->max_dnum; i++) {
		// Step 3: Update data block bitmap and write to disk 
		if(get_bitmap(data_bmap, i) == 0) {
			set_bitmap(data_bmap, i);
			bio_write(2, data_bmap);
			free(data_bmap);
			return 0;
		}
	}
	
	puts("No available data block found.");
	free(data_bmap);
	return -1;
}

/* 
 * inode operations
 */
int readi(uint16_t ino, struct inode *inode) {
	int return_status = 0;

  // Step 1: Get the inode's on-disk block number
	int block = superblock->i_start_blk + ceil((float)ino/(float)inodesPerBlock);
	struct inode* inodeBlock = (struct inode*) malloc(BLOCK_SIZE);
	return_status = bio_read(block, inodeBlock);

	if(return_status < 0) {
		puts("Error reading disk.");
		return return_status;
	}

	// Step 2: Get the offset in the block where this inode resides on disk
	int offset = (ino % inodesPerBlock) * sizeof(struct inode);
	
	// Step 3: Read the block from disk and then copy into inode structure
	*inode = *(inodeBlock + offset);

	return return_status;
}

int writei(uint16_t ino, struct inode *inode) {
	int return_status = 0;
	// Step 1: Get the block number where this inode resides on disk
	int block = superblock->i_start_blk + ceil((float)ino/(float)inodesPerBlock);
	struct inode* inodeBlock = (struct inode*) malloc(BLOCK_SIZE);
	return_status = bio_read(block, inodeBlock);

	if(return_status < 0) {
		puts("Error reading disk.");
		return return_status;
	}

	// Step 2: Get the offset in the block where this inode resides on disk
	int offset = (ino % inodesPerBlock) * sizeof(struct inode);
	//TODO: Overwrite the inode:
	*(inodeBlock + offset) = *inode;
	// Step 3: Write inode to disk 
	return_status = bio_write(block, inodeBlock);
	if(return_status < 0) {
		puts("Error writing to disk.");
		return return_status;
	}
	return return_status;
}


/* 
 * directory operations
 */
int dir_find(uint16_t ino, const char *fname, size_t name_len, struct dirent *dirent) {

  // Step 1: Call readi() to get the inode using ino (inode number of current directory)

  // Step 2: Get data block of current directory from inode

  // Step 3: Read directory's data block and check each directory entry.
  //If the name matches, then copy directory entry to dirent structure

	return 0;
}

int dir_add(struct inode dir_inode, uint16_t f_ino, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and check each directory entry of dir_inode
	
	// Step 2: Check if fname (directory name) is already used in other entries

	// Step 3: Add directory entry in dir_inode's data block and write to disk

	// Allocate a new data block for this directory if it does not exist

	// Update directory inode

	// Write directory entry

	return 0;
}

int dir_remove(struct inode dir_inode, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and checks each directory entry of dir_inode
	
	// Step 2: Check if fname exist

	// Step 3: If exist, then remove it from dir_inode's data block and write to disk

	return 0;
}

/* 
 * namei operation
 */
int get_node_by_path(const char *path, uint16_t ino, struct inode *inode) {
	
	// Step 1: Resolve the path name, walk through path, and finally, find its inode.
	// Note: You could either implement it in a iterative way or recursive way

	return 0;
}

/* 
 * Make file system
 */
int tfs_mkfs() {

	// Call dev_init() to initialize (Create) Diskfile
	dev_init(diskfile_path);
	dev_open(diskfile_path);

	if(diskfile == -1) {
		puts("Error opening disk");
		exit(EXIT_FAILURE);
	}
	// write superblock information
	superblock = malloc(sizeof(sb));
	superblock->magic_num = MAGIC_NUM;
	superblock->max_inum = MAX_INUM;
	superblock->max_dnum = MAX_DNUM;
	superblock->i_bitmap_blk = 1;
	superblock->d_bitmap_blk = 2;
	superblock->i_start_blk = 3;
	
	superblock->d_start_blk = superblock->i_start_blk + ceil((float)superblock->max_inum/(float)inodesPerBlock);


	// initialize inode bitmap
	bitmap_t inode_bmap = calloc(1, superblock->max_inum/8);

	// initialize data block bitmap
	bitmap_t data_bmap = calloc(1, superblock->max_dnum/8);

	// update bitmap information for root directory
	bio_write(1, inode_bmap);
	bio_write(2, data_bmap);

	// update inode for root directory
	inode* root = create_inode(TFS_DIR);
	writei(0, root);
	return 0;
}

inode* create_inode(int inodeType) {
	inode* temp = calloc(1, sizeof(inode));
	temp->ino = get_avail_ino();
	temp->valid = 1;
	temp->size = 0;
	temp->type = inodeType;
	temp->link = (inodeType == TFS_DIR) ? 2 : 1;

	for(int i = 0; i < 16; i++) {
		temp->direct_ptr[i] = -1;
		if(i < 8) {
			temp->indirect_ptr[i] = -1;
		}
	}

	struct stat* vstat = malloc(sizeof(struct stat));
	vstat->st_mode = (inodeType == TFS_DIR) ? (S_IFDIR | 0755) : (S_IFREG | 0666);
	time(&vstat->st_mtime);

	temp->vstat = *vstat;

	return temp;
}
/* 
 * FUSE file operations
 */
static void *tfs_init(struct fuse_conn_info *conn) {
	// Step 1a: If disk file is not found, call mkfs
	dev_open(diskfile_path);
	if(diskfile == -1) {
		if(tfs_mkfs() != 0)
			puts("Error making disk.");
	}
	// Step 1b: If disk file is found, just initialize in-memory data structures
	// and read superblock from disk
	else  {
		superblock = (sb*) malloc(sizeof(sb));
		bio_read(0, superblock);
	}
	return NULL;
}

static void tfs_destroy(void *userdata) {

	// Step 1: De-allocate in-memory data structures
	free(superblock);
	// Step 2: Close diskfile
	dev_close(diskfile);
}

static int tfs_getattr(const char *path, struct stat *stbuf) {

	// Step 1: call get_node_by_path() to get inode from path

	// Step 2: fill attribute of file into stbuf from inode

		stbuf->st_mode   = S_IFDIR | 0755;
		stbuf->st_nlink  = 2;
		time(&stbuf->st_mtime);

	return 0;
}

static int tfs_opendir(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: If not find, return -1

    return 0;
}

static int tfs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: Read directory entries from its data blocks, and copy them to filler

	return 0;
}


static int tfs_mkdir(const char *path, mode_t mode) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name

	// Step 2: Call get_node_by_path() to get inode of parent directory

	// Step 3: Call get_avail_ino() to get an available inode number

	// Step 4: Call dir_add() to add directory entry of target directory to parent directory

	// Step 5: Update inode for target directory

	// Step 6: Call writei() to write inode to disk
	

	return 0;
}

static int tfs_rmdir(const char *path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name

	// Step 2: Call get_node_by_path() to get inode of target directory

	// Step 3: Clear data block bitmap of target directory

	// Step 4: Clear inode bitmap and its data block

	// Step 5: Call get_node_by_path() to get inode of parent directory

	// Step 6: Call dir_remove() to remove directory entry of target directory in its parent directory

	return 0;
}

static int tfs_releasedir(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int tfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name

	// Step 2: Call get_node_by_path() to get inode of parent directory

	// Step 3: Call get_avail_ino() to get an available inode number

	// Step 4: Call dir_add() to add directory entry of target file to parent directory

	// Step 5: Update inode for target file

	// Step 6: Call writei() to write inode to disk

	return 0;
}

static int tfs_open(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: If not find, return -1

	return 0;
}

static int tfs_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {

	// Step 1: You could call get_node_by_path() to get inode from path

	// Step 2: Based on size and offset, read its data blocks from disk

	// Step 3: copy the correct amount of data from offset to buffer

	// Note: this function should return the amount of bytes you copied to buffer
	return 0;
}

static int tfs_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
	// Step 1: You could call get_node_by_path() to get inode from path

	// Step 2: Based on size and offset, read its data blocks from disk

	// Step 3: Write the correct amount of data from offset to disk

	// Step 4: Update the inode info and write it to disk

	// Note: this function should return the amount of bytes you write to disk
	return size;
}

static int tfs_unlink(const char *path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name

	// Step 2: Call get_node_by_path() to get inode of target file

	// Step 3: Clear data block bitmap of target file

	// Step 4: Clear inode bitmap and its data block

	// Step 5: Call get_node_by_path() to get inode of parent directory

	// Step 6: Call dir_remove() to remove directory entry of target file in its parent directory

	return 0;
}

/**
 *  Don't need to implement.
 * 
 **/
static int tfs_truncate(const char *path, off_t size) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int tfs_release(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}

static int tfs_flush(const char * path, struct fuse_file_info * fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int tfs_utimens(const char *path, const struct timespec tv[2]) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}


static struct fuse_operations tfs_ope = {
	.init		= tfs_init,
	.destroy	= tfs_destroy,

	.getattr	= tfs_getattr,
	.readdir	= tfs_readdir,
	.opendir	= tfs_opendir,
	.releasedir	= tfs_releasedir,
	.mkdir		= tfs_mkdir,
	.rmdir		= tfs_rmdir,

	.create		= tfs_create,
	.open		= tfs_open,
	.read 		= tfs_read,
	.write		= tfs_write,
	.unlink		= tfs_unlink,

	.truncate   = tfs_truncate,
	.flush      = tfs_flush,
	.utimens    = tfs_utimens,
	.release	= tfs_release
};


int main(int argc, char *argv[]) {
	int fuse_stat;

	getcwd(diskfile_path, PATH_MAX);
	strcat(diskfile_path, "/DISKFILE");

	fuse_stat = fuse_main(argc, argv, &tfs_ope, NULL);

	return fuse_stat;
}

