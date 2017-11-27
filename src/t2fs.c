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

#define TRUE 1
#define FALSE 0

int searchFreeHandleListIndex();

typedef struct t2fs_superbloco BootPartition;

typedef struct t2fs_record Record;

typedef struct handle_struct {

	int firstFileFatEntry;
	int currentPointer;
} Handle;

Handle handleList[10];

BootPartition bootPartition;
int isBootPartitionRead = FALSE;
int CLUSTER_SIZE;

Record readCurrentRecordOfHandle(DIR2 handle);
int searchFreeHandleListIndex();
void writeCurrentRecordOfHandle(DIR2 handle, Record record);
int readFatEntry(int index);
int fatToCluster(int fatIndex);

/*
    Dado que pathname é o caminho de um arquivo, separa o mesmo em caminho para
    o diretório e nome do arquivo.
*/
void sepName(char* pathname, char* filepath, char* filename)
{

	if(strcmp(pathname, "/")==0){

		strcpy(filepath, "/");
    	strcpy(filename, "/");
	
	} else {

		char pathCopy[10000];
		strcpy(pathCopy, pathname);
		char* fName = strtok(pathCopy, "/");
		char path[] = "/\0";

		char* lastToken = fName;
		do
		{
			char* currToken = strtok(NULL, "/");
			if(currToken != NULL)
			{
				strcat(path, lastToken);
				strcat(path, "/");
				fName = currToken;
			}
			lastToken = currToken;
		}while(lastToken != NULL);

		strcpy(filepath, path);
		strcpy(filename, fName);
	}
}

// Dado um currentPointer retorna o indice do cluster correspondente.
int currentPointerToClusterIndex(Handle handle) {

    // Computa total de clusters necessários para chegar ao ponteiro atual.
    int nClusters = handle.currentPointer / CLUSTER_SIZE;

    // Navega FAT até encontrar índice do cluster atual.
    int currFat = handle.firstFileFatEntry;
    for(int nCluster = 0; nCluster != nClusters; ++nCluster)
    {
        currFat = readFatEntry(currFat);
    }

    return fatToCluster(currFat);
}

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

		printf("ERROR: Could not read sector %d.\n", index);
	}
}

void writeSector(int index, unsigned char *buffer) {

	if(write_sector(index, buffer) != 0) {

		printf("ERROR: Could not write sector %d.\n", index);
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

void readCluster(int clusterIndex, unsigned char *buffer) {

	int startClusterSector = bootPartition.DataSectorStart;
	int sectorPerCluster = bootPartition.SectorsPerCluster;
	int sector = clusterIndex * sectorPerCluster + startClusterSector;
	int i;

	for(i=0; i<sectorPerCluster; i++) {

		//unsigned char bufferAux[SECTOR_SIZE];
		//readSector(sector+i, bufferAux);
		//memcpy(buffer+i*SECTOR_SIZE, bufferAux, SECTOR_SIZE);
		readSector(sector+i, buffer+i*SECTOR_SIZE);
	}
}

void writeCluster(int clusterIndex, unsigned char *buffer) {

	int startClusterSector = bootPartition.DataSectorStart;
	int sectorPerCluster = bootPartition.SectorsPerCluster;
	int sector = clusterIndex * sectorPerCluster + startClusterSector;
	int i;

	for(i=0; i<sectorPerCluster; i++) {

		writeSector(sector+i, buffer+i*SECTOR_SIZE);
	}
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
		CLUSTER_SIZE = SECTOR_SIZE * bootPartition.SectorsPerCluster;
	}
}

void printRecord(Record r)
{
    printf("\nPrinting record...\n");
    printf("\nTypeVal: %d\n", r.TypeVal);
    printf("\nname: %s\n", r.name);
    printf("\nbytesFileSize: %d", r.bytesFileSize);
    printf("\nfirstCluster: %d\n\n", r.firstCluster);
}

void initHandleList() {

	int i;
	for(i=0; i<10; i++) {

		handleList[i].firstFileFatEntry = ERROR;
		handleList[i].currentPointer = ERROR;
	}
}

void init() {

	if(isBootPartitionRead==FALSE) {

		readBootPartition();
		initHandleList();
		isBootPartitionRead=TRUE;
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
	init();

	return 0;
}

int delete2 (char *filename) {
	init();
	return 0;
}

FILE2 open2 (char *filename) {
	init();

    char filepath[100];
    char fname[100];
    sepName(filename, filepath, fname);

    // Confere diretório pai.
    DIR2 dirHandle = opendir2(filepath);
    if(dirHandle < 0)
    {
        printf("\nInvalid file path: %s\n", filename);
        closedir2(dirHandle);
        return -1;
    }

    // Procura arquivo.
    DIRENT2 dirEnt;
    int fileFound = 0;
    int dirEntNum = 0;  // Número do record do arquivo, inicia por 1.
    while(readdir2(dirHandle, &dirEnt) != -END_OF_DIR)
    {
        ++dirEntNum;
        if(strcmp(dirEnt.name, fname) == 0)
        {
            fileFound = 1;
            break;
        }
    }
    if(fileFound == 0)
    {
        printf("\nCould not find file %s in path %s\n", fname, filepath);
        closedir2(dirHandle);
        return -1;
    }

    // Verifica tipo.
    if(dirEnt.fileType != 0x01)
    {
        printf("\nFile %s is a directory\n", filename);
        closedir2(dirHandle);
        return -1;
    }

    // Carrega Record do arquivo.
    Record record;
    handleList[dirHandle].currentPointer -= sizeof(Record);
    for(int i = 0; i < dirEntNum - 1; ++i)
    {
        record = readCurrentRecordOfHandle(dirHandle);
    }
    closedir2(dirHandle);

    // Cria handle para arquivo.
    int handle = searchFreeHandleListIndex();
    handleList[handle].firstFileFatEntry = clusterToFat(record.firstCluster);
    handleList[handle].currentPointer = 0;
	return handle;
}

int close2 (FILE2 handle) {
	init();

    handleList[handle].firstFileFatEntry = ERROR;
    handleList[handle].currentPointer = ERROR;

	return 0;
}

int read2 (FILE2 handle, char *buffer, int size) {
	init();
	return 0;
}

int write2 (FILE2 handle, char *buffer, int size) {
	init();
	return 0;
}

int truncate2 (FILE2 handle) {
	init();
	return 0;
}

int seek2 (FILE2 handle, unsigned int offset) {
	init();
	return 0;
}



int mkdir2 (char *pathname) {
	init();

	//separating the path
	char fatherPath[MAX_FILE_NAME_SIZE];
	char name[MAX_FILE_NAME_SIZE];
	sepName(pathname, fatherPath, name);

	//opening father
	int handleFather = opendir2(fatherPath);

	DIRENT2 aux;
	//till it find an empty space
	while(readdir2(handleFather, &aux)==0);
	handleList[handleFather].currentPointer -= sizeof(Record);//to get back to the previous empty space

	//configure record of new dir
	Record record;
	record.TypeVal = TYPEVAL_DIRETORIO;
	strcpy(record.name, name); 
    
	
	//and the new fat entries
	int freeFat = searchFatEntryOfType(CLUSTER_FREE);
	changeFatEntryToType(freeFat, CLUSTER_EOF);
	record.firstCluster = fatToCluster(freeFat);
    
	//size of the father and itself
	record.bytesFileSize = 2*sizeof(record);
	
	//editing the father cluster
	unsigned char clusterBuffer[CLUSTER_SIZE];
	int fatEntryIndex = handleList[handleFather].firstFileFatEntry;
	int clusterIndex = fatToCluster(fatEntryIndex);
	readCluster(clusterIndex, clusterBuffer);
	memcpy(clusterBuffer+handleList[handleFather].currentPointer, &record, sizeof(record));
	writeCluster(clusterIndex, clusterBuffer);

	closedir2(handleFather);

	//and the "." and ".." dir
	Record itSelf = record;
	strcpy(itSelf.name, ".");

	handleFather = opendir2(fatherPath);
	Record fatherRecord = readCurrentRecordOfHandle(handleFather);//we know that the "." dir is the first, possible problem?
	//add the size of the new record
	fatherRecord.bytesFileSize += sizeof(record);
	handleList[handleFather].currentPointer = 0;
	//writeCurrentRecordOfHandle(handleFather, fatherRecord);//AINDA FALTA MUDAR O TAMANHO DOS DIRETÒRIOS ALTERADOS


	strcpy(fatherRecord.name, "..");

	unsigned char newCluster[CLUSTER_SIZE];
	int newClusterIndex = record.firstCluster;
	memcpy(newCluster+	0*sizeof(record), &itSelf, sizeof(record));
	memcpy(newCluster+	1*sizeof(record), &fatherRecord, sizeof(record));
	writeCluster(newClusterIndex, newCluster);
	
	closedir2(handleFather);

	return 0;
}

int rmdir2 (char *pathname) {
	init();

	//separating the path
	char fatherPath[MAX_FILE_NAME_SIZE];
	char name[MAX_FILE_NAME_SIZE];
	sepName(pathname, fatherPath, name);

	//opening father
	int handleFather = opendir2(fatherPath);

	DIRENT2 aux;
	//till we find the one
	while(strcmp(aux.name, name)) {

		if(readdir2(handleFather, &aux)!=0) {
			
			break;
		}
	}
	if(strcmp(aux.name, name)!=0) {

		printf("ERROR, you cant delete a folder that doesn't exist.\n");
	}

	handleList[handleFather].currentPointer -= sizeof(Record);//to get back to the previous empty space

	Record fatherRecord = readCurrentRecordOfHandle(handleFather);
	//fatherRecord.firstCluster use this to delete fat entriess!!!!!!!!!!!!!!!!!!!!!!!!!!!!!1

	//guarda currentPointer
	int middlePointer = handleList[handleFather].currentPointer;

	//pego o ultimo file/folder
	handleList[handleFather].currentPointer = 0;
	while(readdir2(handleFather, &aux)==0);
	handleList[handleFather].currentPointer -= 2*sizeof(Record);//to get back to the previous NOT empty space
	Record lastRecord = readCurrentRecordOfHandle(handleFather);

	Record record;
	record.TypeVal = TYPEVAL_INVALIDO;
	
	if(handleList[handleFather].currentPointer == middlePointer) {

		//seto ele pra free/invalido
		writeCurrentRecordOfHandle(handleFather, record);
	
	} else {

		//seto ele pra free/invalido
		writeCurrentRecordOfHandle(handleFather, record);

		//volto pro index e seto o ultim, pra n deixar falhas
		handleList[handleFather].currentPointer = middlePointer;
		writeCurrentRecordOfHandle(handleFather, lastRecord);
	}

	return 0;
}

int chdir2 (char *pathname) {
	init();
	return 0;
}

int getcwd2 (char *pathname, int size) {
	init();
	return 0;
}

int searchFreeHandleListIndex() {

	int i;
	for(i=0; i<10; i++) {

		if(handleList[i].firstFileFatEntry == ERROR) {

			return i;
		}
	}

	return ERROR;
}

DIR2 opendir2 (char *pathname) {
	init();

	char *token = strtok(pathname, "/");

	int handle = searchFreeHandleListIndex();
	handleList[handle].firstFileFatEntry = clusterToFat(bootPartition.RootDirCluster);
	handleList[handle].currentPointer = 0;

	while(token != NULL) {

		DIRENT2 aux;
		while(strcmp(aux.name, token)!=0) {

			if(readdir2(handle, &aux)!=0) {

				break;
			}
		}

		if(strcmp(aux.name, token)!=0) {

			printf("Could not find path on %s folder.\n", token);
			return ERROR;

		} else {

			Record record;
			handleList[handle].currentPointer -= sizeof(record);
			record = readCurrentRecordOfHandle(handle);

			if(record.TypeVal != TYPEVAL_DIRETORIO) {

				printf("Error, %s is not a folder.\n", token);
				return ERROR;
			}

			closedir2(handle);
			handle = searchFreeHandleListIndex();
			handleList[handle].firstFileFatEntry = clusterToFat(record.firstCluster);
			handleList[handle].currentPointer = 0;
		}

		token = strtok(NULL, "/");
	}
	
	return handle;
}

Record readCurrentRecordOfHandle(DIR2 handle) {

	int fatIndex = handleList[handle].firstFileFatEntry;

	int cluster = fatToCluster(fatIndex);

	unsigned char clusterBuffer[CLUSTER_SIZE];
	readCluster(cluster, clusterBuffer);

	Record rec;
	memcpy(&rec, clusterBuffer+handleList[handle].currentPointer, sizeof(rec));

	return rec;
}

//I should have done it earlier
void writeCurrentRecordOfHandle(DIR2 handle, Record record) {

	int fatIndex = handleList[handle].firstFileFatEntry;

	int cluster = fatToCluster(fatIndex);

	unsigned char clusterBuffer[CLUSTER_SIZE];
	readCluster(cluster, clusterBuffer);

	memcpy(clusterBuffer+handleList[handle].currentPointer, &record, sizeof(record));

	writeCluster(cluster, clusterBuffer);
}

int readdir2 (DIR2 handle, DIRENT2 *dentry) {
	init();

	Record rec = readCurrentRecordOfHandle(handle);

	handleList[handle].currentPointer += sizeof(rec);

	if(rec.TypeVal == TYPEVAL_REGULAR || rec.TypeVal == TYPEVAL_DIRETORIO) {

		dentry->fileSize = rec.bytesFileSize;
		dentry->fileType = rec.TypeVal;
		strcpy(dentry->name, rec.name);
		return 0;
	}

	return -END_OF_DIR;
}

int closedir2 (DIR2 handle) {
	init();

	handleList[handle].firstFileFatEntry = ERROR;
	handleList[handle].currentPointer = ERROR;

	return 0;
}
