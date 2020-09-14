#include "makeFileSystem.h"		

#define KRED  "\x1B[31m"			// some output color constants. 
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"
#define RESET "\x1B[0m"

struct superBlock superB;
struct freeSpaceMgmt freeSpace;

int findSlashNumb(char * str);							// There are many auxiliary functions.
bool deleteUntilFirstSlash(char * str);

int selectOneFreePlaceInodes();
int selectOneFreePlaceBlocks();

void updateFreeSpaceOnDisk(FILE *fp);
bool getFirstPathPartition(char * str, char * firstPartition);

void printNameOfInode(FILE * fp, int inodeNum);

bool searchEntryInBlock( FILE *fpread, int searchingBlock, char * path );

char * getTime(char * timeStr);

void deleteTheBlock(FILE * fp, int blockAdress);
void deleteTheInode(FILE * fp, int searchInodeNum);
void deleteTheEntry(FILE * fp, int searchingBlock, int searchInodeNum);

void mkdirCommandRun(FILE * fp, char * fileSystemData, char * operation, char * parameters);
void rmdirCommandRun(FILE * fp, char * fileSystemData, char * operation, char * parameters);
void writeCommandRun(FILE * fp, FILE *fpFile, char * fileSystemData, char * operation, char * parameters, char * parametersFile);
void readCommandRun(FILE * fp, FILE *fpFile, char * fileSystemData, char * operation, char * parameters, char * parametersFile);
void delCommandRun(FILE * fp, char * fileSystemData, char * operation, char * parameters);
void listCommandRun(FILE * fp, char * fileSystemData, char * operation, char * parameters);
void dumpe2fsCommandRun(FILE * fp, char * fileSystemData, char * operation);

int main(int argc, char const *argv[])
{
	if(!( argc == 3 || argc == 4 || argc == 5 )){
		printf("%s\n", "Wrong number of arguments!!!");
		return 0;	
	}

	char * fileSystemData;							// take arguments.
    fileSystemData = strdup(argv[1]);  

	char * operation;
    operation = strdup(argv[2]);   

	char * parameters;
    char * parametersFile;

    if( argc == 4 ){
    	parameters = strdup(argv[3]);
	    if( findSlashNumb(parameters) == 0  ){
	    	printf("%s\n", "Error. Try using slash (/) to understand the path.");
	    	return 0;
	    }
    }
    else if( argc == 5){
    	parameters = strdup(argv[3]);
	    parametersFile = strdup(argv[4]);
	    if( findSlashNumb(parameters) == 0  ){
	    	printf("%s\n", "Error. Please try using slash (/) to understand the path.");
	    	return 0;
	    }   
    }

	FILE *fp = fopen(fileSystemData, "rb+");
	if( fp == NULL){
		perror("Error!");
		return 0;
	}
	fseek(fp, 0, SEEK_SET);	
	fread(&superB, sizeof(struct superBlock), 1, fp);		// read the super-block

    fseek(fp, superB.startBlockOfFreeSpaceMgmt * superB.blockSize, SEEK_SET);				// read the free space management partition on the disk.
    freeSpace.freeListInodes = (bool*) malloc(sizeof(bool) * superB.numberOfInodes );
	freeSpace.freeListDataBlock = (bool*) malloc(sizeof(bool) * superB.numberOfBlocks );    
    fread(freeSpace.freeListInodes, sizeof(bool), superB.numberOfInodes, fp);
    fread(freeSpace.freeListDataBlock, sizeof(bool), superB.numberOfBlocks, fp);

	if( strcmp(operation, "mkdir") == 0 ){								// run the commands.
		mkdirCommandRun(fp, fileSystemData, operation, parameters);
	}
	else if( strcmp(operation, "rmdir") == 0 ){
		rmdirCommandRun(fp, fileSystemData, operation, parameters);
	}
	else if( strcmp(operation, "write") == 0 ){
		FILE *fpFile;
	    fpFile = fopen(parametersFile, "r");
		if( fpFile == NULL){
			perror("Error!");
			return 0;
		}
		writeCommandRun(fp, fpFile, fileSystemData, operation, parameters, parametersFile);
		fclose(fpFile);
	}
	else if( strcmp(operation, "read") == 0 ){
		FILE *fpFile;			// file should close in the function.
		readCommandRun(fp, fpFile, fileSystemData, operation, parameters, parametersFile);
	}
	else if( strcmp(operation, "del") == 0 ){
		delCommandRun(fp, fileSystemData, operation, parameters);
	}
	else if( strcmp(operation, "list") == 0 ){
		listCommandRun(fp, fileSystemData, operation, parameters);
	}
	else if( strcmp(operation, "dumpe2fs") == 0 ){
		dumpe2fsCommandRun(fp, fileSystemData, operation);
	}
	else{
		printf("%s\n", "You entered wrong command!!!!");
		printf("%s", "Your option: ");
		printf("%s\n", "mkdir, rmdir, write, read, del, list and dumpe2fs commands.");
	}
	fclose(fp);			// close the file system data.
	return 0;
}

void mkdirCommandRun(FILE * fp, char * fileSystemData, char * operation, char * parameters){
	char path[1024] = ".\0";
	strcat(path, parameters);		
	deleteUntilFirstSlash(path);						
	int slashNum = findSlashNumb( path );
	int searchingBlock = superB.startBlockOfRootDir;
	int rootInodeNum = 0;
	while(slashNum > 0){
		if( slashNum == 1){
			if( strlen(path) > 14 ){
				printf("%s\n", "File or dir name length can be maximum 14.");
				return;
			}
			if( searchEntryInBlock( fp, searchingBlock, path ) == true ){		// if this directory exist, pass the operation.
				printf("%s\n", "This directory already exists!!!");
				break;
			}		
			struct inode inodeT;							// create an inode.
			inodeT.mode = 'D';
			inodeT.size = superB.blockSize;
			char timeStr[30];
			getTime(timeStr);
			strcpy(inodeT.time, timeStr);

			int dataBlockAdress = selectOneFreePlaceBlocks();	// free data block from free space management.
			int inodeNum =  selectOneFreePlaceInodes();			// i-node id from free space management.
			if( dataBlockAdress == -1 || inodeNum == -1 ){
				printf("%s\n", "There is not enough space!!");
				return;
			}
			inodeT.directPointers[0] = dataBlockAdress;
	
			fseek(fp, (superB.startBlockOfInodes * superB.blockSize ) + sizeof(struct inode) * inodeNum, SEEK_SET );	// write inode to disk.
			fwrite(&inodeT, sizeof(struct inode), 1, fp);

			updateFreeSpaceOnDisk(fp);					// update free-space management on the disk

			fseek(fp, (dataBlockAdress * superB.blockSize), SEEK_SET );
			struct entry entryEmpty;
			entryEmpty.inodeNum = -1;

			for( int i = 0; i < ceil( (float) superB.blockSize / sizeof(struct entry)  ); i++){
				fwrite(&entryEmpty, sizeof(struct entry), 1, fp);
			}

			struct entry entryCurrent;
			strcpy(entryCurrent.fileOrDirName, ".");
			entryCurrent.inodeNum = inodeNum;
					
			struct entry entryParent;
			strcpy(entryParent.fileOrDirName, "..");
			entryParent.inodeNum = rootInodeNum;

			fseek(fp, (dataBlockAdress * superB.blockSize), SEEK_SET );		
			fwrite(&entryCurrent, sizeof(struct entry), 1, fp);
			fwrite(&entryParent, sizeof(struct entry), 1, fp);

			struct entry entryT;					// create an entry 
			strcpy(entryT.fileOrDirName, path);
			entryT.inodeNum = inodeNum;

			fseek(fp, (searchingBlock * superB.blockSize), SEEK_SET );
			struct entry entryRead;
			for( int i = 0; i < ceil( (float) superB.blockSize / sizeof(struct entry) ); i++){
				fread(&entryRead, sizeof(struct entry), 1, fp);				// this loop find the correct place of disk block partition(if data was previously written to that block?)
				if( entryRead.inodeNum == -1 ){
					fseek(fp, (searchingBlock * superB.blockSize) + sizeof(struct entry) * i, SEEK_SET );
					fwrite(&entryT, sizeof(struct entry), 1, fp);
					break;
				}
			}				
		}

		else if( slashNum > 1 ){
			char firstPartition[strlen(path)];
			if( getFirstPathPartition(path, firstPartition) == false){
				printf("%s\n", "ERROR!! Control the path.");
				break;
			} 
			if( searchEntryInBlock(fp, searchingBlock, firstPartition) == false ){		// if slash num bigger than 1, so first part must be in the block
				printf("%s\n", "ERROR!!!!!CONTROL THE PATH!!!");						// ex: /usr/dir1 	---> usr must be exist in the disk, if not print error.
				break;
			} 

			fseek(fp, (searchingBlock * superB.blockSize), SEEK_SET );
			struct entry entryRead;
			for( int i = 0; i < ceil( (float) superB.blockSize / sizeof(struct entry)  ); i++){		// continue searching.
				fread(&entryRead, sizeof(struct entry), 1, fp);				
				if( strcmp( entryRead.fileOrDirName, firstPartition )   == 0 ){
					int searchInodeNum = entryRead.inodeNum;
					rootInodeNum = searchInodeNum;

					fseek(fp, (superB.startBlockOfInodes * superB.blockSize) + sizeof(struct inode) * searchInodeNum, SEEK_SET );
					struct inode inodeRead;
					fread(&inodeRead, sizeof(struct inode), 1, fp);
					int blockAdress = inodeRead.directPointers[0];
					searchingBlock = blockAdress;
				}
			}
		}
		slashNum--;
	}
}

void rmdirCommandRun(FILE * fp, char * fileSystemData, char * operation, char *parameters){
	char path[1024] = ".\0";
	strcat(path, parameters);
	deleteUntilFirstSlash(path);							
	int slashNum = findSlashNumb( path );
	int searchingBlock = superB.startBlockOfRootDir;
	int rootInodeNum = 0;

	while(slashNum > 0){
		if( slashNum == 1){
			if( strlen(path) > 14 ){
				printf("%s\n", "File or dir name length can be maximum 14.");
				return;
			}
			if( searchEntryInBlock( fp, searchingBlock, path ) == true ){		// if this directory exist, pass the operation.
				struct entry entryRead;
				fseek(fp, searchingBlock * superB.blockSize, SEEK_SET);
				
				for(int i = 0; i < ceil( (float) superB.blockSize / sizeof(struct entry)) ; i++){			
					fread(&entryRead, sizeof(struct entry), 1, fp);

					if( strcmp( entryRead.fileOrDirName, path ) ==  0  ){	// find the correct entry.
					    int searchInodeNum = entryRead.inodeNum;
					    fseek(fp, (superB.startBlockOfInodes * superB.blockSize) + sizeof(struct inode) * searchInodeNum, SEEK_SET );	// go to inode.
					    struct inode inodeRead;
					    fread(&inodeRead, sizeof(struct inode), 1, fp);

					    int searchDataBlock = inodeRead.directPointers[0];
					    fseek(fp, searchDataBlock * superB.blockSize, SEEK_SET);
					    struct entry entryCurrent;		// in the data block '.' , '..', and childs are there. 
					    struct entry entryParent;
					    struct entry entryNext;

					    fread(&entryCurrent, sizeof(struct entry), 1, fp);
					    fread(&entryParent, sizeof(struct entry), 1, fp);
					    fread(&entryNext, sizeof(struct entry), 1, fp);

					    if( entryNext.inodeNum != -1 ){
					    	printf("%s\n", "This directory has child directory or files, so it can not removed!!!");
					    	break;
					    }

					    int blockAdressOfInode = inodeRead.directPointers[0];
					    fseek(fp, searchingBlock * superB.blockSize, SEEK_SET);

			    		deleteTheBlock(fp, blockAdressOfInode);
			    		deleteTheInode(fp, searchInodeNum);
			    		deleteTheEntry(fp, searchingBlock, searchInodeNum);

			    		updateFreeSpaceOnDisk(fp);
					    break;
					   }
				}
				break;
			}		
			else{
				printf("%s\n", "This directory is not exist, so it can not remove!!!");
				break;
			}	
		}

		else if( slashNum > 1 ){
			char firstPartition[strlen(path)];
			if( getFirstPathPartition(path, firstPartition) == false){
				printf("%s\n", "ERROR!! Control the path.");
				break;
			} 
			if( searchEntryInBlock(fp, searchingBlock, firstPartition) == false ){		// if slash num bigger than 1, so first part must be in the block
				printf("%s\n", "ERROR!!!!!CONTROL THE PATH!!!");							// ex: /usr/dir1 	---> usr must be exist in the disk, if not print error.
				break;
			} 

			fseek(fp, (searchingBlock * superB.blockSize), SEEK_SET );					// continue searcing.
			struct entry entryRead;
			for( int i = 0; i < ceil( (float) superB.blockSize / sizeof(struct entry) ); i++){
				fread(&entryRead, sizeof(struct entry), 1, fp);				
				if( strcmp( entryRead.fileOrDirName, firstPartition )   == 0 ){
					int searchInodeNum = entryRead.inodeNum;
					rootInodeNum = searchInodeNum;

					fseek(fp, (superB.startBlockOfInodes * superB.blockSize) + sizeof(struct inode) * searchInodeNum, SEEK_SET );
					struct inode inodeRead;
					fread(&inodeRead, sizeof(struct inode), 1, fp);
					int blockAdress = inodeRead.directPointers[0];
					searchingBlock = blockAdress;
				}
			}
		}
		slashNum--;
	}
}

void readCommandRun(FILE * fp, FILE *fpFile, char * fileSystemData, char * operation, char * parameters, char * parametersFile){
	char path[1024] = ".\0";
	strcat(path, parameters);		
	deleteUntilFirstSlash(path);						
	int slashNum = findSlashNumb( path );
	int searchingBlock = superB.startBlockOfRootDir;
	int rootInodeNum = 0;

	while(slashNum > 0){
		if( slashNum == 1){
			if( strlen(path) > 14 ){
				printf("%s\n", "File or dir name length can be maximum 14.");
				return;
			}
			if( searchEntryInBlock( fp, searchingBlock, path ) == true ){		// if this directory exist, pass the operation.
				fpFile = fopen(parametersFile, "w");
				struct entry entryRead;
				fseek(fp, searchingBlock * superB.blockSize, SEEK_SET);

				for(int i = 0; i < ceil( (float) superB.blockSize / sizeof(struct entry)) ; i++){			
					fread(&entryRead, sizeof(struct entry), 1, fp);
					if( strcmp( entryRead.fileOrDirName, path ) ==  0  ){	// find the correct entry.
					    int searchInodeNum = entryRead.inodeNum;
					    fseek(fp, (superB.startBlockOfInodes * superB.blockSize) + sizeof(struct inode) * searchInodeNum, SEEK_SET );	// go to inode.
					    struct inode inodeRead;
					    fread(&inodeRead, sizeof(struct inode), 1, fp);

					    int inodeSize = inodeRead.size;
					    int searchDataBlock;
					    char buffer[superB.blockSize+1];
					    buffer[superB.blockSize] = '\0';
					    int sizeOfRead = 0;

					    fseek(fpFile, 0, SEEK_SET);
					    int readingByte;
					    for( int i = 0; i < 10; i++ ){									// read direct blocks.
					    	searchDataBlock = inodeRead.directPointers[i];
					    	fseek(fp, searchDataBlock * superB.blockSize, SEEK_SET);
					    	if( inodeRead.size < superB.blockSize ){
					    		readingByte = fread(&buffer, sizeof(char), inodeRead.size, fp);
					    		fwrite(&buffer, sizeof(char), inodeRead.size, fpFile);
					    		fclose(fpFile);
					    		return;
					    	}
					    	readingByte = fread(&buffer, sizeof(char), superB.blockSize, fp);
					    	sizeOfRead += readingByte;
					    			
						    if( sizeOfRead >= inodeRead.size ){
						    	fwrite(&buffer, sizeof(char), (inodeRead.size - ( sizeOfRead - readingByte)), fpFile);
						    	fclose(fpFile);
						    	return;
						    }
						    else{
						    	fwrite(&buffer, sizeof(char), readingByte, fpFile);
						    }
					    }

					    if( sizeOfRead < inodeRead.size ){								// read indirect blocks.
					    	searchDataBlock = inodeRead.singleIndirectPointer[0];
					    	int16_t searchIndirectBlockAdress = 0;

					    	for (int i = 0;  searchIndirectBlockAdress != -1; i++){
					    		fseek(fp, (searchDataBlock * superB.blockSize) + sizeof(int16_t) * i, SEEK_SET);
					    		fread(&searchIndirectBlockAdress, sizeof(int16_t), 1, fp);

					    		fseek(fp, searchIndirectBlockAdress * superB.blockSize, SEEK_SET); 	
					    		readingByte = fread(&buffer, sizeof(char), superB.blockSize, fp);
					    		sizeOfRead += readingByte;
						   		if( sizeOfRead >= inodeRead.size ){
						    		fwrite(&buffer, sizeof(char), (inodeRead.size - ( sizeOfRead - readingByte)), fpFile);
						    		fclose(fpFile);
						    		return;
						    	}	
						    	else{
						    		fwrite(&buffer, sizeof(char), readingByte, fpFile);
						    	}				    				
					    	}
					    }
					    fclose(fpFile);
					    break;
					}
				}
				fclose(fpFile);
				break;
			}		
			else{
				printf("%s\n", "This file is not exist, so it can not read!!!");
				break;
			}	
		}

		else if( slashNum > 1 ){
			char firstPartition[strlen(path)];
			if( getFirstPathPartition(path, firstPartition) == false){
				printf("%s\n", "ERROR!! Control the path.");
				break;
			} 
			if( searchEntryInBlock(fp, searchingBlock, firstPartition) == false ){		// if slash num bigger than 1, so first part must be in the block
				printf("%s\n", "ERROR!!!!!CONTROL THE PATH!!!");							// ex: /usr/dir1 	---> usr must be exist in the disk, if not print error.
				break;
			} 

			fseek(fp, (searchingBlock * superB.blockSize), SEEK_SET );
			struct entry entryRead;
			for( int i = 0; i < ceil( (float) superB.blockSize / sizeof(struct entry) ); i++){
				fread(&entryRead, sizeof(struct entry), 1, fp);				
				if( strcmp( entryRead.fileOrDirName, firstPartition )   == 0 ){
					int searchInodeNum = entryRead.inodeNum;
					rootInodeNum = searchInodeNum;

					fseek(fp, (superB.startBlockOfInodes * superB.blockSize) + sizeof(struct inode) * searchInodeNum, SEEK_SET );
					struct inode inodeRead;
					fread(&inodeRead, sizeof(struct inode), 1, fp);
					int blockAdress = inodeRead.directPointers[0];
					searchingBlock = blockAdress;
				}
			}
		}
		slashNum--;
	}
}

void writeCommandRun(FILE * fp, FILE *fpFile, char * fileSystemData, char * operation, char * parameters, char * parametersFile){
	char path[1024] = ".\0";
	strcat(path, parameters);
			
	deleteUntilFirstSlash(path);							
	int slashNum = findSlashNumb( path );
	int searchingBlock = superB.startBlockOfRootDir;
	int rootInodeNum = 0;

	while(slashNum > 0){
		if( slashNum == 1){
			if( strlen(path) > 14 ){
				printf("%s\n", "File or dir name length can be maximum 14.");
				return;
			}			
			struct inode inodeT;
			inodeT.mode = 'F';
			inodeT.size = superB.blockSize;		

			char timeStr[30];
			getTime(timeStr);
			strcpy(inodeT.time, timeStr);

			int readingByte = superB.blockSize;
			int sizeOfFile = 0;
			int dataBlockAdress;
			int inodeNum;
			int16_t indirectBlockAdress[superB.blockSize / sizeof(int16_t)];
			fseek(fpFile, 0, SEEK_SET);
			char buffer[superB.blockSize+1];
			buffer[superB.blockSize] = '\0';
			int j = 0;
			for(int i = 0; readingByte == superB.blockSize; i++  ){
				readingByte = fread(&buffer, sizeof(char), superB.blockSize, fpFile);		// read as block size
				sizeOfFile += readingByte;
				if( i < 10){
					dataBlockAdress = selectOneFreePlaceBlocks();
					if( dataBlockAdress == -1 ){
						printf("%s\n", "There is not enough space!!");
						return;
					}
					inodeT.directPointers[i] = dataBlockAdress;
					fseek(fp, (dataBlockAdress * superB.blockSize), SEEK_SET);		// write file partitions to the disk.
					fwrite(&buffer, sizeof(char), readingByte, fp);
				}
				else{			// if indirect adresses is required.
					dataBlockAdress = selectOneFreePlaceBlocks();
					if( dataBlockAdress == -1 ){
						printf("%s\n", "There is not enough space!!");
						return;
					}
					indirectBlockAdress[j] = dataBlockAdress;
					j++;
					if( j > superB.blockSize / sizeof(int16_t) ){
						printf("%s\n", "There is not enough space!!!");
						break;
					}
					fseek(fp, (dataBlockAdress * superB.blockSize), SEEK_SET);		// write file partitions to the disk.
					fwrite(&buffer, sizeof(char), readingByte, fp);
				}
			}
			indirectBlockAdress[j] = -1;
			dataBlockAdress = selectOneFreePlaceBlocks();
			if( dataBlockAdress == -1 ){
				printf("%s\n", "There is not enough space!!");
				return;
			}			

			fseek(fp, dataBlockAdress * superB.blockSize, SEEK_SET);
			fwrite(&indirectBlockAdress, sizeof(int16_t), j+2, fp);

			inodeT.singleIndirectPointer[0] = dataBlockAdress;			// only single indirect bloks are used.
			inodeT.size = sizeOfFile;

			inodeNum =  selectOneFreePlaceInodes();
			if( inodeNum == -1 ){
				printf("%s\n", "There is not enough space!!");
				return;
			}		

			fseek(fp, (superB.startBlockOfInodes * superB.blockSize ) + sizeof(struct inode) * inodeNum, SEEK_SET );	// write inode to disk.
			fwrite(&inodeT, sizeof(struct inode), 1, fp);

			updateFreeSpaceOnDisk(fp);					// update free-space management on the disk

			struct entry entryT;					// create an entry 
			strcpy(entryT.fileOrDirName, path);
			entryT.inodeNum = inodeNum;

			fseek(fp, (searchingBlock * superB.blockSize), SEEK_SET );
			struct entry entryRead;
			for( int i = 0; i < ceil( (float) superB.blockSize / sizeof(struct entry) ); i++){
				fread(&entryRead, sizeof(struct entry), 1, fp);				// this loop find the correct place of disk block partition(if data was previously written to that block?)
				if( entryRead.inodeNum == -1 ){
					fseek(fp, (searchingBlock * superB.blockSize) + sizeof(struct entry) * i, SEEK_SET );
					fwrite(&entryT, sizeof(struct entry), 1, fp);
					break;
				}
			}			
		}

		else if( slashNum > 1 ){
			char firstPartition[strlen(path)];
			if( getFirstPathPartition(path, firstPartition) == false){
				printf("%s\n", "ERROR!! Control the path.");
				break;
			} 
			if( searchEntryInBlock(fp, searchingBlock, firstPartition) == false ){		// if slash num bigger than 1, so first part must be in the block
				printf("%s\n", "ERROR!!!!!CONTROL THE PATH!!!");						// ex: /usr/dir1 	---> usr must be exist in the disk, if not print error.
				break;
			} 

			fseek(fp, (searchingBlock * superB.blockSize), SEEK_SET );
			struct entry entryRead;
			for( int i = 0; i < ceil( (float) superB.blockSize / sizeof(struct entry)  ); i++){
				fread(&entryRead, sizeof(struct entry), 1, fp);				
				if( strcmp( entryRead.fileOrDirName, firstPartition )   == 0 ){
					int searchInodeNum = entryRead.inodeNum;
					rootInodeNum = searchInodeNum;

					fseek(fp, (superB.startBlockOfInodes * superB.blockSize) + sizeof(struct inode) * searchInodeNum, SEEK_SET );
					struct inode inodeRead;
					fread(&inodeRead, sizeof(struct inode), 1, fp);
					int blockAdress = inodeRead.directPointers[0];
					searchingBlock = blockAdress;
				}
			}
		}
		slashNum--;
	}
}

void dumpe2fsCommandRun(FILE * fp, char * fileSystemData, char * operation){
	printf("%-50s %d(byte) \n", "Block Size : ", superB.blockSize);
	printf("%-50s %d \n", "Number of i-nodes : ", superB.numberOfInodes);
	printf("%-50s %d \n", "Number of blocks : ", superB.numberOfBlocks);
	int numberOfFreeInodes = superB.numberOfInodes;
	int numberOfFreeBlocks = superB.numberOfBlocks;
	for(int i = 0; i < superB.numberOfInodes; i++){
		if( freeSpace.freeListInodes[i] == true ){
			numberOfFreeInodes--;
		} 
	}
	for(int i = 0; i < superB.numberOfBlocks; i++){
		if( freeSpace.freeListDataBlock[i] == true ){
			numberOfFreeBlocks--;
		} 
	}
	printf("%-50s %d \n", "Number of free i-nodes : ", numberOfFreeInodes);
	printf("%-50s %d \n", "Number of free blocks : ", numberOfFreeBlocks);
	printf("%-50s %d(byte) \n", "Size of super block : ", superB.sizeSuperBlock);
	printf("%-50s %d(byte) \n", "Size of free-space management : ", superB.sizeFreeSpaceMgmt);
	printf("%-50s %d(byte) \n", "Size of an i-node : ", superB.sizeInode);
	printf("%-50s %d(byte) \n", "Size of an entry : ", superB.sizeEntry);
	printf("%-50s %d \n", "Start block adress of super block : ", superB.startBlockOfSuperBlock);
	printf("%-50s %d \n", "Start block adress of free-space management : ", superB.startBlockOfFreeSpaceMgmt);
	printf("%-50s %d \n", "Start block adress of i-nodes : ", superB.startBlockOfInodes);
	printf("%-50s %d \n", "Start block adress of root directory : ", superB.startBlockOfRootDir);
	printf("%-50s %d \n", "Start block adress of data blocks : ", superB.startBlockOfDatas);
	printf("-----------------------------------------------------------------------------\n");
	struct inode inodeRead;
	for( int i = 0; i < superB.numberOfInodes ; i++ ){
		fseek(fp, (superB.startBlockOfInodes * superB.blockSize) + sizeof(struct inode) * i, SEEK_SET );
		fread(&inodeRead, sizeof(struct inode), 1, fp);
		if(inodeRead.size != -1 && freeSpace.freeListInodes[i] == true){
			printNameOfInode(fp, i);
			if( inodeRead.mode == 'F' ){
				printf("Mode: %-15s ", "FILE");
			}
			else{
				printf("Mode: %-15s ", "DIRECTORY");
			}
			printf("I-node id: %-6d ", i);
			printf("size: %-5d \n", inodeRead.size);
			printf("%s", "Direct Pointers Adresses: ");
			for(int i = 0; i < 10; i++){
				int blockAdress = inodeRead.directPointers[i];
				if( blockAdress > 0 &&  (blockAdress >= superB.startBlockOfDatas || blockAdress == superB.startBlockOfRootDir) && blockAdress < superB.numberOfBlocks ){
					if( freeSpace.freeListDataBlock[blockAdress] == true ){
						printf("%d ", blockAdress);
					}		
				}
			}
			int indirectBlockAdress = inodeRead.singleIndirectPointer[0];
			if( indirectBlockAdress > 0 &&  (indirectBlockAdress >= superB.startBlockOfDatas) && indirectBlockAdress < superB.numberOfBlocks ){
				if( freeSpace.freeListDataBlock[indirectBlockAdress] == true ){
					printf("\n%s", "Indirect Pointers Adresses: ");
					fseek(fp, (indirectBlockAdress * superB.blockSize), SEEK_SET);
					int16_t indirectBlocks = 0;
					for( int i = 0; indirectBlocks != -1; i++ ){
						fread(&indirectBlocks, sizeof(int16_t), 1, fp);
						if( indirectBlocks == -1 ){
							break;
						}
						if( freeSpace.freeListDataBlock[indirectBlocks] == true ){
							printf("%d ", indirectBlocks);
						}
					}
				}
			}
			printf("\n-----------------------------------------------------------------------------\n");
		}
	}
}

void listCommandRun(FILE * fp, char * fileSystemData, char * operation, char * parameters){
	int size = 0;
	size = strlen(parameters);
	char path[size+1];
	strcpy(path, parameters);
	path[size] = '\0';

	int slashNum;
	if( strcmp(parameters, "/") == 0 || strcmp(parameters, ".") == 0 || strcmp(parameters, "..") == 0 || 
		strcmp(parameters, "/root") == 0 || strcmp(parameters, "root") == 0 || strcmp(parameters, "./") == 0 ){
		strcpy(path, ".");
		path[1] = '\0';
		slashNum = 1;
	}
	else{
		slashNum = findSlashNumb( path );
	}
	int searchingBlock = superB.startBlockOfRootDir;
	int rootInodeNum = 0;
	while(slashNum > 0){
		if( slashNum == 1){
			if( strlen(path) > 14 ){
				printf("%s\n", "File or dir name length can be maximum 14.");
				return;
			}			
			if( searchEntryInBlock( fp, searchingBlock, path ) == true ){		
				struct entry entryRead;
				fseek(fp, searchingBlock * superB.blockSize, SEEK_SET);
				for(int i = 0; i < ceil( (float) superB.blockSize / sizeof(struct entry)) ; i++){			
					fread(&entryRead, sizeof(struct entry), 1, fp);

					if( strcmp( entryRead.fileOrDirName, path ) ==  0  ){	// find the correct entry.
					    int searchInodeNum = entryRead.inodeNum;
					    fseek(fp, (superB.startBlockOfInodes * superB.blockSize) + sizeof(struct inode) * searchInodeNum, SEEK_SET );	// go to inode.
					    struct inode inodeRead;
					    fread(&inodeRead, sizeof(struct inode), 1, fp);

					    int searchingPath = inodeRead.directPointers[0];
					    int i = 0;
					    int listCount = 0;
					    for( i = 0; entryRead.inodeNum != -1; i++){
						    fseek(fp,  (searchingPath * superB.blockSize) + sizeof(struct entry) * i , SEEK_SET);
						    fread(&entryRead, sizeof(struct entry), 1, fp);		
						    if( entryRead.inodeNum == -1 ){
						    	break;
						    }
						    if( !(strcmp(entryRead.fileOrDirName, ".") == 0 ||  strcmp(entryRead.fileOrDirName, "..") == 0)    ){
								int searchingInodeBlock = entryRead.inodeNum;
								struct inode inodeTemp;
								fseek(fp,  (superB.startBlockOfInodes * superB.blockSize) + sizeof(struct inode) * searchingInodeBlock, SEEK_SET);
								fread(&inodeTemp, sizeof(struct inode), 1, fp);

					    		if( inodeTemp.mode == 'F' ){					// print colorfuly
						    		printf("%s%-10s%s  ", KBLU, "FILE", RESET);
						    	}
						    	else{
						    		printf("%s%-10s%s  ", KBLU, "DIRECTORY", RESET);
						    	}
						    	printf("%-5d   ", inodeTemp.size);
						    	printf("%-20s  ", inodeTemp.time);
						    	char name[15];
						    	int sizeName = strlen(entryRead.fileOrDirName);
						    	strncpy(name, entryRead.fileOrDirName + 1, sizeName);
						    	name[sizeName] = '\0';
						    	printf("%s%-15s %s\n", KCYN, name, RESET);
						    } 				  		
					    }
					    if( i == 2){
					    	printf("There are no child directories or files to list in the given path!!!!! \n");
					    	break;
					    }
					    break;
					}
				}
				break;
			}		
			else{
				printf("%s\n", "This directory is not exist, so it can not list!!!");
				break;
			}	
		}

		else if( slashNum > 1 ){
			char firstPartition[strlen(path)];
			if( getFirstPathPartition(path, firstPartition) == false){
				printf("%s\n", "ERROR!! Control the path.");
				break;
			} 

			if( searchEntryInBlock(fp, searchingBlock, firstPartition) == false ){		// if slash num bigger than 1, so first part must be in the block
				printf("%s\n", "ERROR!!!!!CONTROL THE PATH!!!");							// ex: /usr/dir1 	---> usr must be exist in the disk, if not print error.
				break;
			} 

			fseek(fp, (searchingBlock * superB.blockSize), SEEK_SET );
			struct entry entryRead;
			for( int i = 0; i < ceil( (float) superB.blockSize / sizeof(struct entry) ); i++){
				fread(&entryRead, sizeof(struct entry), 1, fp);				
				if( strcmp( entryRead.fileOrDirName, firstPartition )   == 0 ){
					int searchInodeNum = entryRead.inodeNum;
					rootInodeNum = searchInodeNum;

					fseek(fp, (superB.startBlockOfInodes * superB.blockSize) + sizeof(struct inode) * searchInodeNum, SEEK_SET );
					struct inode inodeRead;
					fread(&inodeRead, sizeof(struct inode), 1, fp);
					int blockAdress = inodeRead.directPointers[0];
					searchingBlock = blockAdress;
				}
			}
		}
		slashNum--;
	}
}

void delCommandRun(FILE * fp, char * fileSystemData, char * operation, char * parameters){
	char path[1024] = ".\0";
	strcat(path, parameters);
	deleteUntilFirstSlash(path);							
	int slashNum = findSlashNumb( path );
	int searchingBlock = superB.startBlockOfRootDir;
	int rootInodeNum = 0;

	while(slashNum > 0){
		if( slashNum == 1){
			if( strlen(path) > 14 ){
				printf("%s\n", "File or dir name length can be maximum 14.");
				return;
			}			
			if( searchEntryInBlock( fp, searchingBlock, path ) == true ){		
				struct entry entryRead;
				fseek(fp, searchingBlock * superB.blockSize, SEEK_SET);
				for(int i = 0; i < ceil( (float) superB.blockSize / sizeof(struct entry)) ; i++){			
					fread(&entryRead, sizeof(struct entry), 1, fp);
					if( strcmp( entryRead.fileOrDirName, path ) ==  0  ){	// find the correct entry.
					    int searchInodeNum = entryRead.inodeNum;
					    fseek(fp, (superB.startBlockOfInodes * superB.blockSize) + sizeof(struct inode) * searchInodeNum, SEEK_SET );	// go to inode.
					    struct inode inodeRead;
					    fread(&inodeRead, sizeof(struct inode), 1, fp);

					    int blockAdressOfInode;
					    for( int i = 0; i < 10; i++){									// delete direct pointers.
						    blockAdressOfInode = inodeRead.directPointers[i];
						    fseek(fp, searchingBlock * superB.blockSize, SEEK_SET);
						    deleteTheBlock(fp, blockAdressOfInode);
					    }

					    blockAdressOfInode = inodeRead.singleIndirectPointer[0];
					    int16_t searchIndirectBlockAdress = 0;
					    for( int i = 0; searchIndirectBlockAdress != -1; i++){
					    	fseek(fp, (blockAdressOfInode * superB.blockSize) + sizeof(int16_t) * i, SEEK_SET);
					    	fread(&searchIndirectBlockAdress, sizeof(int16_t), 1, fp);
					    	if( searchIndirectBlockAdress != -1 ){
					    		deleteTheBlock(fp, searchIndirectBlockAdress);		// delete indirect pointers 
					    	}	
					    }
					    deleteTheBlock(fp, blockAdressOfInode);		// delete the block that indirect pointers.
					    deleteTheInode(fp, searchInodeNum);
					    deleteTheEntry(fp, searchingBlock, searchInodeNum);

					    updateFreeSpaceOnDisk(fp);
					    break;
					}
				}
				break;
			}		
			else{
				printf("%s\n", "This file is not exist, so it can not delete!!!");
				break;
			}	
		}

		else if( slashNum > 1 ){
			char firstPartition[strlen(path)];
			if( getFirstPathPartition(path, firstPartition) == false){
				printf("%s\n", "ERROR!! Control the path.");
				break;
			} 

			if( searchEntryInBlock(fp, searchingBlock, firstPartition) == false ){		// if slash num bigger than 1, so first part must be in the block
				printf("%s\n", "ERROR!!!!!CONTROL THE PATH!!!");							// ex: /usr/dir1 	---> usr must be exist in the disk, if not print error.
				break;
			} 

			fseek(fp, (searchingBlock * superB.blockSize), SEEK_SET );
			struct entry entryRead;
			for( int i = 0; i < ceil( (float) superB.blockSize / sizeof(struct entry) ); i++){
				fread(&entryRead, sizeof(struct entry), 1, fp);				
				if( strcmp( entryRead.fileOrDirName, firstPartition )   == 0 ){
					int searchInodeNum = entryRead.inodeNum;
					rootInodeNum = searchInodeNum;

					fseek(fp, (superB.startBlockOfInodes * superB.blockSize) + sizeof(struct inode) * searchInodeNum, SEEK_SET );
					struct inode inodeRead;
					fread(&inodeRead, sizeof(struct inode), 1, fp);
					int blockAdress = inodeRead.directPointers[0];
					searchingBlock = blockAdress;
				}
			}
		}
		slashNum--;
	}
}

void printNameOfInode(FILE * fp, int inodeNum){				// find name by inode id.
	struct entry entryRead;
	if( inodeNum == 0 ){
		printf("Name:  %-15s ", "root");
		return;
	}
	for( int i = 0; i < superB.numberOfBlocks; i++){
		if( freeSpace.freeListDataBlock[i] == true && ( i >= superB.startBlockOfDatas || i == superB.startBlockOfRootDir) ){
			fseek(fp, (i * superB.blockSize), SEEK_SET );
			for( int j = 0; j < ceil( superB.blockSize / sizeof(struct entry) )  ; j++ ){
				fseek(fp, (i * superB.blockSize) + (j * sizeof(struct entry)), SEEK_SET );
				fread(&entryRead, sizeof(struct entry), 1, fp);
				if( entryRead.inodeNum == inodeNum  ){
					char name[15];
					int sizeName = strlen(entryRead.fileOrDirName);
					strncpy(name, entryRead.fileOrDirName + 1, sizeName);
					name[sizeName] = '\0';
					printf("Name:  %-15s ", name);
					return;
				}		
			}
		}  
	}
}

void deleteTheBlock(FILE * fp, int blockAdress){
	bool oneByte = false;
	fseek(fp, (blockAdress * superB.blockSize), SEEK_SET );
	for( int i = 0; i < superB.blockSize; i++){				// remove the block(means reseting)	
   		fwrite(&oneByte, 1, sizeof(oneByte), fp);
	}
	freeSpace.freeListDataBlock[blockAdress] = false;
}
void deleteTheInode(FILE * fp, int searchInodeNum){
	fseek(fp, (superB.startBlockOfInodes * superB.blockSize) + sizeof(struct inode) * searchInodeNum, SEEK_SET );		// find inode that will be removed.
	struct inode inodeEmtpy;		// empty inode.
	inodeEmtpy.size = -1;
	fwrite(&inodeEmtpy, sizeof(struct inode), 1, fp);
	freeSpace.freeListInodes[searchInodeNum] = false;
}
void deleteTheEntry(FILE * fp, int searchingBlock, int searchInodeNum){
	fseek(fp, (searchingBlock * superB.blockSize), SEEK_SET );
	struct entry entryRead;
	for( int i = 0; i < ceil( (float) superB.blockSize / sizeof(struct entry) ); i++){
		fread(&entryRead, sizeof(struct entry), 1, fp);		
		if( entryRead.inodeNum == searchInodeNum ){
			struct entry entryEmpty;			// empty entry.
			entryEmpty.inodeNum = -1;

			fseek(fp, (searchingBlock * superB.blockSize) + sizeof(struct entry) * i, SEEK_SET );
			fwrite(&entryEmpty, sizeof(struct entry), 1, fp);

			break;
		}		
	}
}

int findSlashNumb(char * str){			// find numbers of slash.
	int count = 0;
	for(int i = 0; str[i] != '\0'; i++){
		if(str[i] == '/')
			count++;
	}
	return count;
}

bool deleteUntilFirstSlash(char * str){		
	for(int i = 0; str[i] != '\0'; i++){
		if( str[i] == '/'){
			strncpy(str, str + i, strlen(str) - i + 1);
			return true;
		}
	}
	return false;
}

bool getFirstPathPartition(char * str, char * firstPartition){		
	int count = 0;
	int firstSlash;
	for(int i = 0; str[i] != '\0'; i++){
		if( str[i] == '/' ){
			if( str[i+1] == '/' ){
				return false;
			}
			count++;
			if( count == 2 ){
				if( strlen(str) <= i+1 ){
					return false;
				}
				for(int j = 0; j < i; j++){
					firstPartition[j] = str[j];
				}
				firstPartition[i] = '\0';
				strncpy(str, str + i, strlen(str) - i + 1);				// delete first partition from whole path.
				if( strlen(firstPartition) > 14 ){
					printf("%s\n", "File or dir name length can be maximum 14.");
					return false;
				}
				return true;
			}
			firstSlash = i;
		}
	}
	return false;
}

int selectOneFreePlaceInodes(){					// return a free i-node id.
	for(int i = 0; i < superB.numberOfInodes; i++){
		if( freeSpace.freeListInodes[i] == false ){
			freeSpace.freeListInodes[i] = true;
			return i;
		}
	}
	return -1;									// if there is not, return -1.
}

int selectOneFreePlaceBlocks(){					// return a data block id.
	for(int i = superB.startBlockOfDatas; i < superB.numberOfBlocks; i++){
		if( freeSpace.freeListDataBlock[i] == false ){
			freeSpace.freeListDataBlock[i] = true;
			return i;
		}
	}
	return -1;								// if there is not, return -1.
}

void updateFreeSpaceOnDisk(FILE *fp){
	fseek(fp, (superB.startBlockOfFreeSpaceMgmt * superB.blockSize), SEEK_SET);		// update free-space in disk.
	fwrite(freeSpace.freeListInodes, sizeof(bool), superB.numberOfInodes, fp);
	fwrite(freeSpace.freeListDataBlock, sizeof(bool), superB.numberOfBlocks, fp);	
}


bool searchEntryInBlock( FILE *fpread, int searchingBlock, char * path ){
    struct entry entryRead;
    fseek(fpread, searchingBlock * superB.blockSize, SEEK_SET);
    for(int i = 0; i < ceil( (float) superB.blockSize / sizeof(struct entry)) ; i++){			
    	fread(&entryRead, sizeof(struct entry), 1, fpread);
    	if( strcmp( entryRead.fileOrDirName, path ) ==  0 ){			// if file or dir name already exist, return true.
    		return true;
    	}
    }
    return false;
}

char * getTime(char * timeStr){			// find the current time
    time_t timer;
    struct tm* currentTime;
    timer = time(NULL);
    currentTime = localtime(&timer);
    strftime(timeStr, 26, "%Y-%m-%d %H:%M:%S", currentTime);
    timeStr[26] = '\0';	
}
