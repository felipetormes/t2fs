#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/t2fs.h"
#include "../include/apidisk.h"

#define CLUSTER_FREE 0x00000000
#define CLUSTER_CAN_NOT_EXIST 0x00000001
#define CLUSTER_DEFECTC 0xFFFFFFFE
#define CLUSTER_EOF 0xFFFFFFFF

#define ERROR -666

typedef struct t2fs_superbloco BootPartition;

BootPartition bootPartition;

int identify2(char *name, int size){

  char* str ="\nAlfeu Uzai Tavares\nEduardo Bassani Chandelier 261591\nFelipe Barbosa Tormes\n\n";

  memcpy(name, str, size);
	if (size == 0)
		name[0] = '\0';
	else
		name[size-1] = '\0';
	return 0;
}

void readSector(int index, unsigned char *buffer) {
	
	if(read_sector(index, buffer) != 0) {

		printf("ERROR: Could not read sector.");
	}
}

void writeSector(int index, unsigned char *buffer) {
	
	if(write_sector(index, buffer) != 0) {

		printf("ERROR: Could not write sector.");
	}
}


//we could use the readFatEntry function to make this cleaner, but it would be slower
int searchFatEntryOfType(int type) {

	int startFatSector = bootPartition.pFATSectorStart;
	int endFatSector = bootPartition.DataSectorStart -1;
	int sector;
	for(sector = startFatSector; sector <= endFatSector; sector++) {

		unsigned char buffer[SECTOR_SIZE];
		readSector(sector, buffer);
		
		int sectorOffset;
		int numberOfFatEntries = SECTOR_SIZE/4;
		for(sectorOffset = 0; sectorOffset < numberOfFatEntries; sectorOffset++) {

			int fatEntry;
			memcpy(&fatEntry, buffer+sectorOffset*4, sizeof(fatEntry));
			if(fatEntry==type) {

				int returnIndex = (sector-startFatSector)*SECTOR_SIZE/4 + sectorOffset;
				return returnIndex;
			}
		}
	}

	return ERROR;
}

int readFatEntry(int index) {

	int startFatSector = bootPartition.pFATSectorStart;
	int sector = index / (SECTOR_SIZE/4) + startFatSector;
	int offSet = index % (SECTOR_SIZE/4);

	unsigned char buffer[SECTOR_SIZE];
	readSector(sector, buffer);

	int fatEntry;
	memcpy(&fatEntry, buffer+offSet*4, sizeof(int));
	
	return fatEntry;
}

void changeFatEntryToType(int index, int type) {

	int startFatSector = bootPartition.pFATSectorStart;
	int sector = index / (SECTOR_SIZE/4) + startFatSector;
	int offSet = index % (SECTOR_SIZE/4);

	unsigned char buffer[SECTOR_SIZE];
	readSector(sector, buffer);

	memcpy(buffer+offSet*4, &type, sizeof(int));
	
	writeSector(sector, buffer);
}

int fatToCluster(int fatIndex) {

	if(readFatEntry(fatIndex)==CLUSTER_DEFECTC) {

		printf("ERROR, Can not convert defect fat entry to cluster.\n");
	}

	int i, defectedFatEntry=0;
	
	for(i=0; i<fatIndex; i++) {

		if(readFatEntry(i)==CLUSTER_DEFECTC) {

			defectedFatEntry++;
		}
	}

	return fatIndex - defectedFatEntry;
}

int clusterToFat(int clusterIndex) {

	int fatIndex = 0;

	while(clusterIndex >= 0) {

		if(readFatEntry(fatIndex++) != CLUSTER_DEFECTC) {

			clusterIndex--;
		}
	}
	
	fatIndex--;
	
	return fatIndex;
}

void readBootPartition() {

	unsigned char bootBuffer[SECTOR_SIZE];

	if(read_sector(0, bootBuffer) != 0) {

		printf("ERROR: Could not read boot partition.");

	} else {

		memcpy(&bootPartition, bootBuffer, sizeof(bootPartition));
	}
}

void printBootPartition() {

	printf("Boot ID:%.4s,Version:%d, Sectos/Superblock:%d, TotalSize:%d(bytes), %d(sectors), LogicalSector/Cluster:%d, StartFatSector:%d, StartClusterRoorDir:%d, FirstLogicalSectorOfDataBlock: %d \n",
		bootPartition.id, bootPartition.version, bootPartition.SuperBlockSize, bootPartition.DiskSize, bootPartition.NofSectors, 
		bootPartition.SectorsPerCluster, bootPartition.pFATSectorStart, bootPartition.RootDirCluster, bootPartition.DataSectorStart);
}

void printFatEntry(int entryValue) {

	switch(entryValue) {

		case CLUSTER_FREE:				printf("CLUSTER_FREE\n");break;
		case CLUSTER_CAN_NOT_EXIST:		printf("CLUSTER_CAN_NOT_EXIST\n");break;
		case CLUSTER_DEFECTC:			printf("CLUSTER_DEFECTC\n");break;
		case CLUSTER_EOF:				printf("CLUSTER_EOF\n");break;
	}
}

FILE2 create2 (char *filename) {

	readBootPartition();
	printBootPartition();

	return 0;
}

int delete2 (char *filename) {

	return 0;	
}

FILE2 open2 (char *filename) {

	return 0;
}

int close2 (FILE2 handle) {
	
	return 0;
}

int read2 (FILE2 handle, char *buffer, int size) {
	
	return 0;
}

int write2 (FILE2 handle, char *buffer, int size) {
	
	return 0;
}

int truncate2 (FILE2 handle) {

	return 0;
}

int seek2 (FILE2 handle, unsigned int offset) {
	
	return 0;
}

int mkdir2 (char *pathname) {
	
	return 0;
}

int rmdir2 (char *pathname) {
	
	return 0;
}

int chdir2 (char *pathname) {
	
	return 0;
}

int getcwd2 (char *pathname, int size) {
	
	return 0;
}

DIR2 opendir2 (char *pathname) {
	
	return 0;
}

int readdir2 (DIR2 handle, DIRENT2 *dentry) {
	
	return 0;
}

int closedir2 (DIR2 handle) {
	
	return 0;
}