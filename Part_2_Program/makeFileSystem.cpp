#include "makeFileSystem.h"

struct superBlock superB;
struct freeSpaceMgmt freeSpace;

char * getTime(char * timeStr);
struct superBlock createSuperBlock(int blockSize, int numberOfInodes);
struct freeSpaceMgmt createFreeSpaceMgmt(int blockSize, int numberOfInodes, int numberOfBlocks);
void createDisk(FILE *fp, const int blockSize, const int numberOfInodes, const char *diskName);

int main(int argc, char const *argv[])
{
	if( argc != 4 ){
		printf("%s\n", "Wrong number of arguments. You must enter 4 arguments!!!");
		return 0;
	}
	int blockSize = atoi(argv[1]) * 1024;		// kilobyte to byte. (size of an block )
	int numberOfInodes = atoi(argv[2]);			// number of free inodes.
	if( blockSize <= 0 || numberOfInodes <= 0 ){
		printf("%s\n", "Parameters must be greater than 0.");
		return 0;
	}
	const char * diskName = argv[3];			// disk name
	FILE *fp = fopen(diskName, "wb+");
	if( fp == NULL){
		perror("Error!");
		return 0;
	}
	createDisk(fp, blockSize, numberOfInodes, diskName);		// create disk
    printf("%s\n", "Disk successfully created.");
    fclose(fp);
	return 0;
}

void createDisk(FILE *fp, const int blockSize, const int numberOfInodes, const char *diskName){
	bool oneByte = false;
	for( int i = 0; i < 1024 * 1024; i++){				// create the disk default.
   		fwrite(&oneByte, 1, sizeof(oneByte), fp);
	}

	fseek(fp, 0, SEEK_SET);
	createSuperBlock(blockSize, numberOfInodes);			// load the super-block.
	fwrite(&superB, sizeof(struct superBlock), 1, fp);

	fseek(fp, (superB.startBlockOfFreeSpaceMgmt * blockSize), SEEK_SET);
	createFreeSpaceMgmt(blockSize, numberOfInodes, superB.numberOfBlocks);	// load freespace-management partition.
	fwrite(&freeSpace, sizeof(struct freeSpaceMgmt), 1, fp);

	fseek(fp, (superB.startBlockOfInodes * blockSize), SEEK_SET);
	struct inode inodeEmpty;											// load freeInodes that is empty.
	inodeEmpty.size = -1;
	for( int i = 0; i < numberOfInodes; i++){
		fwrite(&inodeEmpty, sizeof(struct inode), 1, fp);
	}

	fseek(fp, (superB.startBlockOfRootDir * blockSize), SEEK_SET );			// load rootdir at first.
	struct entry entryEmpty;
	entryEmpty.inodeNum = -1;

	for( int i = 0; i < ceil( (float) superB.blockSize / sizeof(struct entry)); i++){	// all root dir entries are empty at first. ( this is for next)
		fwrite(&entryEmpty, sizeof(struct entry), 1, fp);
	} 

	fseek(fp, (superB.startBlockOfRootDir * blockSize), SEEK_SET );			// directory blocks default entries.
	struct entry root1;
	strcpy( root1.fileOrDirName, "." );
	root1.inodeNum = 0;

	struct entry root2;
	strcpy( root2.fileOrDirName, ".." );
	root2.inodeNum = 0;
	fwrite(&root1, sizeof(struct entry), 1, fp);			// write to disk default.
	fwrite(&root2, sizeof(struct entry), 1, fp);

	fseek(fp, (superB.startBlockOfInodes * blockSize), SEEK_SET);		// root-node loads.
	struct inode inodeRoot;
	inodeRoot.mode = 'D';
	inodeRoot.size = blockSize;
	char timeStr[30];
	getTime(timeStr);
	strcpy(inodeRoot.time, timeStr);

	inodeRoot.directPointers[0] = superB.startBlockOfRootDir;
	fwrite(&inodeRoot, sizeof(struct inode), 1, fp);

	freeSpace.freeListInodes[0] = true;
	fseek(fp, (superB.startBlockOfFreeSpaceMgmt * blockSize), SEEK_SET);		// update free-space in disk.
	fwrite(freeSpace.freeListInodes, sizeof(bool), superB.numberOfInodes, fp);
	fwrite(freeSpace.freeListDataBlock, sizeof(bool), superB.numberOfBlocks, fp);
}

struct superBlock createSuperBlock(int blockSize, int numberOfInodes){	// this function returns a super block that desired size and quantity.
	superB.blockSize = blockSize;
	superB.numberOfInodes = numberOfInodes;

	int sizeSuperBlock = sizeof(struct superBlock);
	int sizeFreeSpaceMgmt = sizeof(struct freeSpaceMgmt);
	int sizeInode = sizeof(struct inode);
	int sizeEntry = sizeof(struct entry);

	superB.sizeSuperBlock = sizeSuperBlock;
	superB.sizeInode = sizeInode;
	superB.sizeEntry = sizeEntry;
	superB.numberOfBlocks = ceil( (float) (1024 * 1024) / superB.blockSize );
	superB.sizeFreeSpaceMgmt = sizeof(bool) * numberOfInodes + sizeof(bool) * superB.numberOfBlocks;
	superB.startBlockOfSuperBlock = 0;
	superB.startBlockOfFreeSpaceMgmt = ceil ((float) sizeSuperBlock / superB.blockSize);
	superB.startBlockOfInodes = superB.startBlockOfFreeSpaceMgmt + ceil ((float) sizeFreeSpaceMgmt / superB.blockSize);
	superB.startBlockOfRootDir = superB.startBlockOfInodes + ceil( ((float) sizeInode * superB.numberOfInodes)  / superB.blockSize );
	superB.startBlockOfDatas = superB.startBlockOfRootDir + 1;
	return superB;
}

struct freeSpaceMgmt createFreeSpaceMgmt(int blockSize, int numberOfInodes, int numberOfBlocks){
	freeSpace.freeListInodes = (bool*) malloc(sizeof(bool) * numberOfInodes );
	freeSpace.freeListDataBlock = (bool*) malloc(sizeof(bool) * numberOfBlocks );
	for(int i = 0; i < numberOfInodes; i++){	
		freeSpace.freeListInodes[i] = false;		// false means empty.		
	}												// all inodes is free in start.
	for(int i = 0; i < numberOfBlocks; i++){
		freeSpace.freeListDataBlock[i] = false;
	}
	superB.sizeFreeSpaceMgmt = sizeof(bool) * numberOfInodes + sizeof(bool) * numberOfBlocks;

	freeSpace.freeListDataBlock[superB.startBlockOfSuperBlock] = true;	// true means reserved.
	freeSpace.freeListDataBlock[0] = true;
	freeSpace.freeListDataBlock[superB.startBlockOfFreeSpaceMgmt] = true;	// that blocks are reserved.
	for(int i = superB.startBlockOfInodes; i < superB.startBlockOfRootDir; i++){
		freeSpace.freeListDataBlock[i] = true;				// inodes are reserved.
	}
	freeSpace.freeListDataBlock[superB.startBlockOfRootDir] = true;
}

char * getTime(char * timeStr){		// find current date and time string
    time_t timer;
    struct tm* currenTime;
    timer = time(NULL);
    currenTime = localtime(&timer);
    strftime(timeStr, 26, "%Y-%m-%d %H:%M:%S", currenTime);
    timeStr[26] = '\0';	
}