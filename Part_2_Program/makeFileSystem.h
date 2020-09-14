#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

using namespace std;

struct inode
{
	char mode;			// 'D' for directories, 'F' for files
	int size;
	char time[30];
	int directPointers[10];			// There are 10 direct blocks
	int singleIndirectPointer[1];	// There are 3 indirect blocks.
	int doubleIndirectPointer[1];
	int tripleIndirectPointer[1];
};

struct entry 
{
    char fileOrDirName[15]; 		// file or directory name
    int16_t inodeNum;      			// inode id
};

struct freeSpaceMgmt{				// free space management partition.
	bool * freeListInodes;    		// for i-nodes.
    bool * freeListDataBlock;		// for blocks.
};

struct superBlock 					// struct holding the superblock.
{					
	int blockSize;
	int numberOfInodes;
	int sizeSuperBlock;
	int sizeFreeSpaceMgmt;
	int sizeInode;
	int sizeEntry;
	int numberOfBlocks;
	int startBlockOfSuperBlock;
	int startBlockOfFreeSpaceMgmt;
	int startBlockOfInodes;
	int startBlockOfRootDir;
	int startBlockOfDatas;
};


