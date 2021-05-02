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
int direntPerBlock = BLOCK_SIZE/sizeof(struct dirent);
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
			return i;
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
			return i;
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
	int block = superblock->i_start_blk + floor((float)ino/(float)inodesPerBlock);
	struct inode* inodeBlock = (struct inode*) malloc(BLOCK_SIZE);
	return_status = bio_read(block, inodeBlock);

	if(return_status < 0) {
		puts("Error reading disk.");
		return return_status;
	}

	// Step 2: Get the offset in the block where this inode resides on disk
	int offset = (ino % inodesPerBlock);
	
	// Step 3: Read the block from disk and then copy into inode structure
	*inode = *(inodeBlock + offset);

	return return_status;
}

int writei(uint16_t ino, struct inode *inode) {
	int return_status = 0;
	// Step 1: Get the block number where this inode resides on disk
	int block = superblock->i_start_blk + floor((float)ino/(float)inodesPerBlock);
	struct inode* inodeBlock = (struct inode*) malloc(BLOCK_SIZE);
	return_status = bio_read(block, inodeBlock);

	if(return_status < 0) {
		puts("Error reading disk.");
		return return_status;
	}

	// Step 2: Get the offset in the block where this inode resides on disk
	int offset = (ino % inodesPerBlock);
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
	printf("In dir_find with fname: %s, ino %d\n", fname, ino);
  	// Step 1: Call readi() to get the inode using ino (inode number of current directory)
	struct inode* currDir = (struct inode*)malloc(sizeof(struct inode));
	int r = readi(ino, currDir);

	if(r < 0) {
		puts("Error reading current directory's inode.");
		return r;
	}

  	// Step 2: Get data block of current directory from inode
	int* currDirData = currDir->direct_ptr; 
	// Step 3: Read directory's data block and check each directory entry.
	//If the name matches, then copy directory entry to dirent structure
	int dataIndex;
	struct dirent* currBlock = calloc(1, BLOCK_SIZE);

	for(dataIndex = 0; dataIndex < 16; dataIndex++) {
		if(dataIndex==0) printf("data at 0: %d", currDirData[dataIndex]);
		if(currDirData[dataIndex] == -1) continue;

		bio_read(superblock->d_start_blk + currDirData[dataIndex] ,currBlock);
		int direntIndex;

		for(direntIndex = 0; direntIndex < direntPerBlock; direntIndex++) {
			struct dirent* entry = currBlock + direntIndex;

			if(entry == NULL || entry->valid == 0) continue;
			printf("dir entry: |%s|%s|\n", fname, entry->name);
			if(strcmp(entry->name, fname) == 0) {
				*dirent = *entry;
				free(currDir);
				free(currBlock);
				return 0;
			}
		}
	}
	//Name not found.
	free(currDir);
	free(currBlock);
	return -1;
}

int dir_add(struct inode dir_inode, uint16_t f_ino, const char *fname, size_t name_len) {
	printf("In dir_add with fname: %s, ino %d\n", fname, dir_inode.ino);
	// Step 1: Read dir_inode's data block and check each directory entry of dir_inode
	struct dirent* dataBlock = calloc(1, BLOCK_SIZE);
	// Step 2: Check if fname (directory name) is already used in other entries
	int inUse = dir_find(dir_inode.ino, fname, name_len, dataBlock);

	if(inUse == 0) {
		puts("This directory already exists, exiting without overwriting.");
		return -1;
	}

	int* currDirData = dir_inode.direct_ptr;
	// Step 3: Add directory entry in dir_inode's data block and write to disk
	struct dirent* newDir = malloc(sizeof(struct dirent));
	newDir->valid = 1; //make it valid
	newDir->ino = f_ino; //ino from function args
	strncpy(newDir->name, fname, name_len+1); //name from function args
	newDir->len = name_len; //len from function args

	// Allocate a new data block for this directory if it does not exist
	int dataIndex;
	int direntIndex;
	struct dirent* entry;
	struct dirent* currBlock = calloc(1, BLOCK_SIZE);
	int flag = 0;
	for(dataIndex = 0; dataIndex < 16; dataIndex++) {
		if(currDirData[dataIndex] == -1) {
			int available = get_avail_blkno();
			*(currDirData+dataIndex) = available;
			printf("set data at %d to %d\n", dataIndex, dir_inode.direct_ptr[dataIndex]);
			bio_read(superblock->d_start_blk + currDirData[dataIndex], currBlock);
			for(direntIndex = 0; direntIndex<direntPerBlock; direntIndex++){
				entry = currBlock + direntIndex;
				entry->valid = 0;
			}
			bio_write(superblock->d_start_blk + currDirData[dataIndex], currBlock);
		}

		bio_read(superblock->d_start_blk + currDirData[dataIndex], currBlock);
		for(direntIndex = 0; direntIndex < direntPerBlock; direntIndex++) {
			entry = currBlock + direntIndex;
			if(entry == NULL || entry->valid == 0) {
				// Update new directory inode
				*entry = *newDir;
				// Write new directory entry
				bio_write(superblock->d_start_blk + currDirData[dataIndex], currBlock);
				flag = 1;
				printf("added file name %s into dir with available block %d\n", entry->name, currDirData[dataIndex]);
				break;
			}
		}
		if(flag) break;
	}
	if(!flag) {
		puts("Probably Error: no space in datablocks for new directory");
		return -1;
	}

	// Update directory inode
	dir_inode.size += sizeof(struct dirent);
	time(&(dir_inode.vstat.st_mtime));
	dir_inode.link += 1;
	// Write directory entry
	printf("block in dir_add: %d\n", dir_inode.direct_ptr[0]);
	writei(dir_inode.ino, &dir_inode);

	return 0;
}

int dir_remove(struct inode dir_inode, const char *fname, size_t name_len) {
	struct dirent* dataBlock = calloc(1, BLOCK_SIZE);
	int inUse = dir_find(dir_inode.ino, fname, name_len, dataBlock);

	if(inUse == -1) {
		puts("Unable to remove directory.");
		return -1;
	}
	int* currDirData = dir_inode.direct_ptr;
	// Allocate a new data block for this directory if it does not exist
	int dataIndex;
	int direntIndex;
	struct dirent* entry;
	struct dirent* currBlock = calloc(1, BLOCK_SIZE);
	int flag = 0;
	for(dataIndex = 0; dataIndex < 16; dataIndex++) {
		if(currDirData[dataIndex] == -1) continue;

		bio_read(superblock->d_start_blk + currDirData[dataIndex], currBlock);
		for(direntIndex = 0; direntIndex < direntPerBlock; direntIndex++) {
			entry = currBlock + direntIndex;
			if(entry != NULL && entry->valid == 1 && strcmp(entry->name, fname)==0) {
				
				// Update new directory inode
				entry->valid = 0;
				// Write new directory entry
				bio_write(superblock->d_start_blk + currDirData[dataIndex], currBlock);
				flag = 1;
				break;
			}
		}
		if(flag) break;
	}
	if(!flag) {
		puts("Error: found directory in dir_find but didn't remove it.");
		return -1;
	}

	// Update directory inode
	dir_inode.size -= sizeof(struct dirent);
	time(&(dir_inode.vstat.st_mtime));
	dir_inode.link -= 1;
	// Write directory entry
	writei(dir_inode.ino, &dir_inode);
	
	// Step 1: Read dir_inode's data block and checks each directory entry of dir_inode
	
	// Step 2: Check if fname exist

	// Step 3: If exist, then remove it from dir_inode's data block and write to disk

	return 0;
}

/* 
 * namei operation
 */
int get_node_by_path(const char *path, uint16_t ino, struct inode *inode) {
	printf("in get_node_by_path %d\n", ino);
	// Step 1: Resolve the path name, walk through path, and finally, find its inode.
	// Note: You could either implement it in a iterative way or recursive way
	if(strcmp(path, "/") == 0 || strlen(path) == 0){
		printf("Current path is \'%s\'.\n", path);
		readi(ino, inode);
		return 0;
	}
	char* next = strstr(path, "/");
	printf("Current path is \'%s\'.\n", path);
	printf("Next thing is \'%s\'\n", next);
	if(next==NULL){ // at end
		struct dirent* dirent = (struct dirent*) malloc(sizeof(struct dirent));
		int i = dir_find(ino, path, strlen(path)+1, dirent); // check is strlen is correct
		if(i==-1){
			puts("unable to get node by path");
			return -1;
		}
		readi(dirent->ino, inode);
		return 0;
	}
	next = next+1;
	int length = strlen(path)-strlen(next);
	char* dir_path = (char*)malloc(length+2);
	memcpy(dir_path, path, length);
	dir_path[length-1] = '\0';
	printf("Path to here is \'%s\'\n", dir_path);
	if(length==0 || length==1) return get_node_by_path(next, ino, inode);
	struct dirent *dirent = (struct dirent*)malloc(sizeof(struct dirent));
	int i = dir_find(ino, dir_path, length, dirent);
	if(i==-1){
		puts("unable to get node by path // dir");
		return -1;
	}
	return get_node_by_path(next, dirent->ino, inode);
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
	int ino = get_avail_ino();
	printf("root ino: %d\n", ino);
	struct inode* root = create_inode(ino, TFS_DIR);
	printf("NEWINODE:::::::%d\n", get_avail_ino());
	writei(0, root);
	return 0;
}

struct inode* create_inode(int ino, int inodeType) {
	struct inode* temp = calloc(1, sizeof(struct inode));
	temp->ino = ino;
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

	bitmap_t inode_bitmap = malloc(BLOCK_SIZE);
	bio_read(1, inode_bitmap);
	set_bitmap(inode_bitmap, ino);
	bio_write(1, inode_bitmap);
	free(inode_bitmap);

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
	puts("in tfs_getattr");
	// Step 1: call get_node_by_path() to get inode from path
	struct inode* inode = (struct inode*)malloc(sizeof(struct inode));
	int found = get_node_by_path(path, 0, inode);
	if(found == -1) {
		free(inode);
		return -ENOENT;
	}

	// Step 2: fill attribute of file into stbuf from inode
		stbuf->st_ino=inode->ino;
		stbuf->st_nlink=inode->link;
		stbuf->st_size=inode->size;
		stbuf->st_uid = getuid();
		stbuf->st_gid = getgid();
		stbuf->st_mode   = S_IFDIR | 0755;
		stbuf->st_mtime=inode->vstat.st_mtime;

		if(inode->type==TFS_DIR){
			stbuf->st_mode = S_IFDIR | 0755;
		} else if (inode->type==TFS_FILE){
			stbuf->st_mode = S_IFREG | 0644;
		}
	
	free(inode);
	return 0;
}

static int tfs_opendir(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path
	struct inode* inode = (struct inode*) malloc(sizeof(struct inode));
	int found = get_node_by_path(path, 0, inode);

	// Step 2: If not find, return -1
	if(found == -1){
		free(inode);
		return -1;
	}
	free(inode);
    return 0;
}

static int tfs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path
	struct inode* inode = (struct inode*) malloc(sizeof(struct inode));
	int found = get_node_by_path(path, 0, inode);

	if(found == -1){
		free(inode);
		return -1;
	}
	int* currDirData = inode->direct_ptr;
	int dataIndex;
	int direntIndex;
	struct dirent* currBlock = calloc(1, BLOCK_SIZE);
	
	// Step 2: Read directory entries from its data blocks, and copy them to filler
	for(dataIndex = 0; dataIndex < 16; dataIndex++){
		if(currDirData[dataIndex] == -1) continue;

		bio_read(superblock->d_start_blk+currDirData[dataIndex], currBlock);

		for(direntIndex = 0; direntIndex < direntPerBlock; direntIndex++){
			struct dirent* entry = currBlock + direntIndex;
			if(entry==NULL || entry->valid==0) continue;
			// put directory entry into the filler
			int full = filler(buffer, entry->name, NULL, offset);
			if(full==-1){
				free(inode);
				free(currBlock);
				return 0;
			}
		}
	}

	free(inode);
	free(currBlock);
	return 0;
}


static int tfs_mkdir(const char *path, mode_t mode) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name
	char* dirPath = (char*) malloc(strlen(path) + 1);
	strncpy(dirPath, path, strlen(path) + 1);

	char* basePath = strrchr(path, '/');

	dirname(dirPath);

	// Step 2: Call get_node_by_path() to get inode of parent directory
	struct inode* parent = (struct inode*) malloc(sizeof(struct inode));
	if(get_node_by_path(dirPath, 0, parent) == -1) {
		puts("Unable to get parent directory.");
		free(dirPath);
		free(parent);
		return -1;
	}
	// Step 3: Call get_avail_ino() to get an available inode number
	int available = get_avail_ino();
	printf("MAKING DIRECTORY: %s\n", basePath);
	// Step 4: Call dir_add() to add directory entry of target directory to parent directory
	//int return_status = dir_add(*parent, available, basePath, strlen(basePath)); // dir_add also makes sure dir doesn't already exist
	if(dir_add(*parent, available, basePath+1, strlen(basePath)) == -1){
		puts("could not create directory");
		// need to unset bitmap
		bitmap_t inode_bitmap = malloc(BLOCK_SIZE);
		bio_read(1, inode_bitmap);
		unset_bitmap(inode_bitmap, available);
		bio_write(1, inode_bitmap);
		free(inode_bitmap);
		// frees
		free(dirPath);
		free(parent);
		return -1;
	}
	// Step 5: Update inode for target directory
	struct inode* new_dir = create_inode(available, TFS_DIR);
	new_dir->vstat.st_mode = mode;
	// Step 6: Call writei() to write inode to disk
	writei(available, new_dir);

	free(new_dir);
	free(dirPath);
	free(parent);
	return 0;
}

static int tfs_rmdir(const char *path) { // ADD FREES

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name
	char* dirPath = (char*) malloc(strlen(path) + 1);
	strncpy(dirPath, path, strlen(path) + 1);

	char* temp = strdup(path);
	basename(temp);
	char* basePath = strrchr(temp, '/');

	dirname(dirPath);


	// Step 2: Call get_node_by_path() to get inode of target directory
	struct inode* target = (struct inode*) malloc(sizeof(struct inode));
	if(get_node_by_path(path, 0, target) == -1) {
		puts("No such file or directory.");
		free(dirPath);
		free(target);
		return -1;
	}

	// Step 3: Clear data block bitmap of target directory
	if(target->link != 2) {
		printf("links: %d\n", target->link);
		puts("Directory not empty.");
		return -1;
	}

	bitmap_t data_bitmap = malloc(BLOCK_SIZE);
	bio_read(2, data_bitmap);
	int dataIndex=0;
	int* currDirData = target->direct_ptr;
	for(dataIndex=0; dataIndex<16; dataIndex++){
		if(currDirData[dataIndex]!=-1){ // there is block listed
			if(get_bitmap(data_bitmap, currDirData[dataIndex])==1){ // listed block is still in use
				puts("rmdir: failed to remove directory: directory not empty");
				return -1;
			}
		}
	}

	// Step 4: Clear inode bitmap and its data block
	bitmap_t inode_bitmap = malloc(BLOCK_SIZE);
	bio_read(1, inode_bitmap);
	unset_bitmap(inode_bitmap, target->ino);
	bio_write(1, inode_bitmap);
	free(inode_bitmap);


	// Step 5: Call get_node_by_path() to get inode of parent directory
	struct inode* parent = (struct inode*) malloc(sizeof(struct inode));
	if(get_node_by_path(dirPath, 0, parent) == -1) {
		puts("Unable to get parent directory.");
		free(dirPath);
		free(target);
		free(parent);
		return -1;
	}

	// Step 6: Call dir_remove() to remove directory entry of target directory in its parent directory
	int return_status = dir_remove(*parent, basePath+1, strlen(basePath));
	if(return_status==-1){
		puts("could not remove directory");
		return -1;
	}

	return 0;
}

static int tfs_releasedir(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int tfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
	printf("In create.\n");
	// Step 1: Use dirname() and basename() to separate parent directory path and target file name
	char* dirPath = (char*) malloc(strlen(path) + 1);
	strncpy(dirPath, path, strlen(path) + 1);

	char* basePath = strrchr(path, '/');

	dirname(dirPath);

	// Step 2: Call get_node_by_path() to get inode of parent directory
	struct inode* parent = (struct inode*) malloc(sizeof(struct inode));
	if(get_node_by_path(dirPath, 0, parent) == -1) {
		puts("No such file or directory.");
		free(dirPath);
		free(parent);
		return -1;
	}

	// Step 3: Call get_avail_ino() to get an available inode number
	int available = get_avail_ino();
	// Step 4: Call dir_add() to add directory entry of target file to parent directory
	if(dir_add(*parent, available, basePath+1, strlen(basePath)) == -1) {
		puts("Could not create file.");

		//unset bitmap
		bitmap_t inode_bitmap = malloc(BLOCK_SIZE);
		bio_read(1, inode_bitmap);

		unset_bitmap(inode_bitmap, available);

		bio_write(1, inode_bitmap);

		free(inode_bitmap);
		free(dirPath);
		free(parent);

		return -1;
	}
	// Step 5: Update inode for target file
	printf("new inode number %d\n", available);
	struct inode* new_file = create_inode(available, TFS_FILE);

	// Step 6: Call writei() to write inode to disk
	writei(available, new_file);

	free(new_file);
	free(dirPath);
	free(parent);
	printf("Created file %s\n", path);
	return 0;
}

static int tfs_open(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path
	struct inode* inode = (struct inode*) malloc(sizeof(struct inode));
	int found = get_node_by_path(path, 0, inode);

	// Step 2: If not find, return -1
	if(found == -1){
		free(inode);
		return -1;
	}
	free(inode);
    return 0;
}

static int tfs_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
	// Step 1: You could call get_node_by_path() to get inode from path
	struct inode* inode = (struct inode*) malloc(sizeof(struct inode));
	if(get_node_by_path(path, 0, inode) == -1) {
		puts("File not found");
		return -1; //No bytes were read --> error state lowkey??
	}

	// Step 2: Based on size and offset, read its data blocks from disk
	int bytesRead = 0;
	char* bufferEnd = buffer;
	int* currData = inode->direct_ptr;
	int* dataBlock = malloc(BLOCK_SIZE);
	for(int dataIndex = offset/BLOCK_SIZE; dataIndex < 16; dataIndex++) {
		if(currData[dataIndex] == -1) break;
			
		bio_read(superblock->d_start_blk + currData[dataIndex], dataBlock);
		int start = (dataIndex == offset/BLOCK_SIZE) ? (offset % BLOCK_SIZE) : 0;

		int read = BLOCK_SIZE - start;
		int bytesLeft = size - bytesRead;

		// Step 3: copy the correct amount of data from offset to buffer
		if (read < bytesLeft) { //Still more to read.
			memcpy(bufferEnd, (dataBlock + start), read);
			bufferEnd += read;
			bytesRead += read;
		} else { //We read everything.
			memcpy(bufferEnd, (dataBlock + start), bytesLeft);
			bufferEnd += bytesLeft;
			bytesRead += bytesLeft;

			time(&(inode->vstat.st_atime));
			writei(inode->ino, inode);

			free(inode);
			free(dataBlock);

			return bytesRead;
		}
	}
	
	// Note: this function should return the amount of bytes you copied to buffer
	return -1; //Error state? 
}

static int tfs_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
	// Step 1: You could call get_node_by_path() to get inode from path
	struct inode* inode = (struct inode*) malloc(sizeof(struct inode));
	if(get_node_by_path(path, 0, inode) == -1) {
		puts("File not found");
		return -1; //No bytes were written --> error state lowkey??
	}
	// Step 2: Based on size and offset, read its data blocks from disk
	int bytesWritten = 0;
	char* bufferEnd = buffer;

	int* currData = inode->direct_ptr;
	int* dataBlock = malloc(BLOCK_SIZE);

	for(int dataIndex = offset/BLOCK_SIZE; dataIndex < 16; dataIndex++) {
		if(currData[dataIndex] == -1) {
			int available = get_avail_blkno();
			currData[dataIndex] = available;
		} 

		// Step 3: Write the correct amount of data from offset to disk
		bio_read(superblock->d_start_blk + currData[dataIndex], dataBlock);
		int start = (dataIndex == offset/BLOCK_SIZE) ? (offset % BLOCK_SIZE) : 0;

		int written = BLOCK_SIZE - start;
		int bytesLeft = size - bytesWritten;

		if(written < bytesWritten) {
			memcpy((dataBlock + start), bufferEnd, written);
			bufferEnd += written;
			bytesWritten += written;
			bio_write(superblock->d_start_blk + currData[dataIndex], dataBlock);
		} else {
			memcpy((dataBlock + start), bufferEnd, bytesLeft);
			bufferEnd += bytesLeft;
			bytesWritten += bytesLeft;
			bio_write(superblock->d_start_blk + currData[dataIndex], dataBlock);

			break;
		}

	}

	// Step 4: Update the inode info and write it to disk
	time(&(inode->vstat.st_atime));
	inode->size = (inode->size > offset+ bytesWritten) ? inode->size : (offset + bytesWritten);
	writei(inode->ino, inode);
	// Note: this function should return the amount of bytes you write to disk
	return bytesWritten;
}

static int tfs_unlink(const char *path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name
	char* dirPath = (char*) malloc(strlen(path) + 1);
	strncpy(dirPath, path, strlen(path) + 1);

	char* basePath = strrchr(path, '/');

	dirname(dirPath);
	
	// Step 2: Call get_node_by_path() to get inode of target file
	struct inode* fileInode = (struct inode*) malloc(sizeof(struct inode));
	if(get_node_by_path(path, 0, fileInode) == -1) {
		puts("Could not find file in tfs_unlink.");
		free(fileInode);
		free(dirPath);
		return -1;
	}
	// Step 3: Clear data block bitmap of target file
	bitmap_t data_bitmap = malloc(BLOCK_SIZE);
	bio_read(2, data_bitmap);
	int* fileData = fileInode->direct_ptr;
	for(int dataIndex = 0; dataIndex < 16; dataIndex++) {
		if(fileData[dataIndex] == -1) continue;
		unset_bitmap(data_bitmap, fileData[dataIndex]);
	}
	free(data_bitmap);
	// Step 4: Clear inode bitmap and its data block
	bitmap_t inode_bitmap = malloc(BLOCK_SIZE);
	bio_read(1, inode_bitmap);
	unset_bitmap(inode_bitmap, fileInode->ino);
	bio_write(1, inode_bitmap);

	free(inode_bitmap);
	// Step 5: Call get_node_by_path() to get inode of parent directory
	struct inode* dirToRemove = (struct inode*) malloc(sizeof(struct inode));
	if(get_node_by_path(dirPath, 0, dirToRemove) == -1) {
		puts("Directory not found in tfs_unlink.");
		free(dirToRemove);
		free(dirPath);
		return -1;
	}
	// Step 6: Call dir_remove() to remove directory entry of target file in its parent directory
	if(dir_remove(*dirToRemove, basePath+1, strlen(basePath)) == -1) {
		puts("Could not remove directory.");
		return -1;
	}
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
