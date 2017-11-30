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
    Record rec;
    char* parentDirPath;
} Handle;

Handle handleList[10];

BootPartition bootPartition;
int isBootPartitionRead = FALSE;
int CLUSTER_SIZE;

char* currPath = NULL;

Record readCurrentRecordOfHandle(DIR2 handle);
int searchFreeHandleListIndex();
void writeCurrentRecordOfHandle(DIR2 handle, Record record);
int readFatEntry(int index);
int fatToCluster(int fatIndex);
void deleteFileOnFat(int fatIndex);
void writeRecordAtEndOfFolder(char *path, Record newRecord);
int getFreeFatEntry();
void updateRecord(Handle hd);
void changeSizeOfFolder(char *pathname, int increment);
void changeSizeOfFile(char *pathname, int increment);
void setCurrentPointerToFile(int grandFatherHandle, char *grandFatherName);


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
		char path[] = "/\0";//this should be removed to mound the absolute pass when the user inputs the relative

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
        handleList[i].parentDirPath = NULL;
	}
}

void initCurrPath() {
    currPath = malloc(2 * sizeof(char));
    currPath[0] = '/';
    currPath[1] = '\0';
}

void init() {

	if(isBootPartitionRead==FALSE) {

		readBootPartition();
		initHandleList();
        initCurrPath();
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

/*
    Verifica se arquivo de nome 'fname' existe no diretório do handle 'dirHandle'.
    Retorna número do record correspondente ao arquivo no diretório (iniciando por 1) caso exista,
    -1 caso não exista
*/
int dirHasFile(int dirHandle, char* fname)
{
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
        //printf("\nCould not find file %s\n", fname);
        return -1;
    }
    else if(dirEnt.fileType != TYPEVAL_DIRETORIO)     // Verifica tipo.
    {
        printf("\n%s is not a directory\n", fname);
        return -1;
    }

    return dirEntNum;
}

FILE2 create2 (char *filename) {
	init();

    char filepath[100];
    char fname[100];
    sepName(filename, filepath, fname);

    DIR2 dirHandle = opendir2(filepath);
    if(dirHandle < 0) // Diretório não existe.
    {
        return -1;
    }

    if(dirHasFile(dirHandle, fname) != -1)
    {
        printf("\nFile %s already exists\n", filename);
        return -1;
    }

    Record newFileRecord;
    newFileRecord.TypeVal = TYPEVAL_REGULAR;
    newFileRecord.name[0] = '\0';
    strcpy(newFileRecord.name, fname);
    newFileRecord.bytesFileSize = 0;
    newFileRecord.firstCluster = getFreeFatEntry();

	changeSizeOfFolder(filename, sizeof(Record));

	writeRecordAtEndOfFolder(filename, newFileRecord);

    closedir2(dirHandle);

	return open2(filename);
}

int delete2 (char *pathname) {
	init();

	//separating the path
	char fatherPath[MAX_FILE_NAME_SIZE];
	char name[MAX_FILE_NAME_SIZE];
	sepName(pathname, fatherPath, name);

	//opening father
	int handleFather = opendir2(fatherPath);

	DIRENT2 aux;
	//till we find the one
    // One Does Not Simply Walk into T2FS
    int dirStatus;
	while(strcmp(aux.name, name) != 0) {

		if((dirStatus = readdir2(handleFather, &aux)) != 0) {

			break;
		}
	}
	if(strcmp(aux.name, name)!=0) {

		printf("ERROR, you cant delete a file that doesn't exist.\n");
	}

	handleList[handleFather].currentPointer -= sizeof(Record);//to get back to the ONE
    int deletedCurrPointer = handleList[handleFather].currentPointer; // guard currpointer do deleteado.

	Record fatherRecord = readCurrentRecordOfHandle(handleFather);

	if(fatherRecord.TypeVal != TYPEVAL_REGULAR) {

		printf("ERROR, You can't delete %s, it's not a file.\n", name);
		return ERROR;
	}

	//Delete fat entries of the ONE
	int fatIndex = clusterToFat(fatherRecord.firstCluster);
	deleteFileOnFat(fatIndex);

	//pego o ultimo file/folder a partir do deletado
    while(dirStatus != -END_OF_DIR) {
		dirStatus = readdir2(handleFather, &aux);
	}
    handleList[handleFather].currentPointer -= 2*sizeof(Record); //to get back to the last valid
    int lastCurrPointer = handleList[handleFather].currentPointer;
    Record lastRecord = readCurrentRecordOfHandle(handleFather);

	if(handleList[handleFather].currentPointer == deletedCurrPointer)
    {
        // Deletado era o último, basta marcar como inválido.
        Record record;
    	record.TypeVal = TYPEVAL_INVALIDO;
        writeCurrentRecordOfHandle(handleFather, record);
    }
    else
    {
        // Coloca último no lugar do deleteado.
        handleList[handleFather].currentPointer = deletedCurrPointer;
        writeCurrentRecordOfHandle(handleFather, lastRecord);

        // Invalida último.
        Record record;
    	record.TypeVal = TYPEVAL_INVALIDO;
        handleList[handleFather].currentPointer = lastCurrPointer;
        writeCurrentRecordOfHandle(handleFather, record);
    }

	sepName(pathname, fatherPath, name);

	changeSizeOfFile(pathname, -sizeof(Record));

	closedir2(handleFather);

	return 0;
}

FILE2 open2 (char *filename) {
	init();

    char filepath[100];
    char fname[100];
    sepName(filename, filepath, fname);

    DIR2 dirHandle = opendir2(filepath);

	setCurrentPointerToFile(dirHandle, fname);

    // Carrega Record do arquivo.
    Record record;
    record = readCurrentRecordOfHandle(dirHandle);

    closedir2(dirHandle);

    // Cria handle para arquivo.
    int handle = searchFreeHandleListIndex();
    handleList[handle].firstFileFatEntry = clusterToFat(record.firstCluster);
    handleList[handle].currentPointer = 0;
    handleList[handle].rec = record;
    handleList[handle].parentDirPath = malloc(strlen(filepath));
    strcpy(handleList[handle].parentDirPath, filepath);

	return handle;
}

int close2 (FILE2 handle) {
	init();

    handleList[handle].firstFileFatEntry = ERROR;
    handleList[handle].currentPointer = ERROR;
    if(handleList[handle].parentDirPath != NULL)
    {
        free(handleList[handle].parentDirPath);
        handleList[handle].parentDirPath = NULL;
    }

	return 0;
}

int read2_1 (FILE2 handle, char *buffer, int size) {
    init();

    // To do: verificar se handle é valido e de arquivo.
    Handle hd = handleList[handle];

    int availabeToRead = (int)hd.rec.bytesFileSize - hd.currentPointer;

    int totalToRead = 0;
    if(size <= availabeToRead)
    {
        totalToRead = size;
    }
    else
    {
        totalToRead = availabeToRead;
    }

    int toRead = totalToRead;

    int currFat = hd.firstFileFatEntry;
    int currCluster = fatToCluster(currFat);
    int currPointerInCurrCluster = hd.currentPointer % CLUSTER_SIZE;

    // Navega pro cluster atual.
    int nClusters = hd.currentPointer / CLUSTER_SIZE;
    for(int i = 0; i != nClusters; ++i)
    {
        currFat = readFatEntry(currFat);
        currCluster = fatToCluster(currFat);
    }

    char* mainBuffer = malloc(totalToRead);
    mainBuffer[0] = '\0';

    do
    {
        int availabeInCluster = CLUSTER_SIZE - currPointerInCurrCluster;

        if(toRead > availabeInCluster)
        {
            // Lê clsuter atual, prepara próximo.
            char buffer2[CLUSTER_SIZE];
            buffer2[0] = '\0';
            readCluster(currCluster, (unsigned char*)buffer2);
            strncat(mainBuffer, &buffer2[currPointerInCurrCluster], availabeInCluster);

            // Próximo cluster.
            currFat = readFatEntry(currFat);
            currCluster = fatToCluster(currFat);
            currPointerInCurrCluster = 0;

            toRead -= availabeInCluster;
        }
        else
        {
            // Lê cluster.
            char buffer2[CLUSTER_SIZE];
            buffer2[0] = '\0';
            readCluster(currCluster, (unsigned char*)buffer2);
            strncat(mainBuffer, &buffer2[currPointerInCurrCluster], toRead);

            toRead = 0;
        }
    }while(toRead > 0);

    strncpy(buffer, mainBuffer, totalToRead);
    free(mainBuffer);

    handleList[handle].currentPointer += totalToRead;

	return totalToRead;
}

int readCurrentByte(int handle, char *byte) {

	int fatEntry = handleList[handle].firstFileFatEntry;

	int currentPointerCluster = handleList[handle].currentPointer / CLUSTER_SIZE;

	while(currentPointerCluster > 0) {

		fatEntry = readFatEntry(fatEntry);
		currentPointerCluster--;
	}

	int currentPointerOffsetInsideCluster = handleList[handle].currentPointer % CLUSTER_SIZE;

	unsigned char clusterBuffer[CLUSTER_SIZE];

	int clusterIndex = fatToCluster(fatEntry);

	readCluster(clusterIndex, clusterBuffer);

	memcpy(byte, clusterBuffer + currentPointerOffsetInsideCluster, 1);

	handleList[handle].currentPointer++;

	int totalSize = handleList[handle].rec.bytesFileSize;
	int currPointer = handleList[handle].currentPointer;

	if(totalSize < currPointer) {

		return ERROR;
	}

	return 0;
}

int read2 (FILE2 handle, char *buffer, int size) {

	int bufferOffset = 0;
	while(size>0) {

		char currentByte;
		if(readCurrentByte(handle, &currentByte)==ERROR) {

			printf("Chegou no fim do arquivo.\n");
			break;
		}
		memcpy(buffer+bufferOffset, &currentByte, 1);

		size--;
		bufferOffset++;
	}

	return bufferOffset;
}

int write2 (FILE2 handle, char *buffer, int size) {
	init();

    int totalToWrite = size;
    int written = 0;

    int initialSize = handleList[handle].rec.bytesFileSize;
    int initialPointer = handleList[handle].currentPointer;

    // Navega pro cluster atual.
    int currFat = handleList[handle].firstFileFatEntry;
    int currCluster = fatToCluster(currFat);
    int nClusters = handleList[handle].currentPointer / CLUSTER_SIZE;

    int prevFat = -1;
    for(int i = 0; i != nClusters; ++i)
    {
        prevFat = currFat;
        currFat = readFatEntry(currFat);
        currCluster = fatToCluster(currFat);
    }

    if(currFat == CLUSTER_EOF)
    {
        // Solicita novo cluster.
        int nextFat = clusterToFat(getFreeFatEntry());
        changeFatEntryToType(prevFat, nextFat);
        currFat = nextFat;
        currCluster = fatToCluster(currFat);
    }

    int toWrite_ = totalToWrite;
    while(written < totalToWrite)
    {
        int currPointerInCurrCluster = handleList[handle].currentPointer % CLUSTER_SIZE;
        int availabeInCurrCluster = CLUSTER_SIZE - currPointerInCurrCluster;

        toWrite_ = totalToWrite - written;

        if(availabeInCurrCluster >= toWrite_)
        {
            // Todos dados cabem dentro do cluster, escreve.
            unsigned char previousClusterData[CLUSTER_SIZE];
            readCluster(currCluster, previousClusterData);
            strncpy((char*)&previousClusterData[currPointerInCurrCluster], (char*)&buffer[written], toWrite_);
            writeCluster(currCluster, previousClusterData);
            written += toWrite_;

            handleList[handle].currentPointer += toWrite_;
        }
        else
        {
            // Escreve até encher cluster, solicita novo.
            unsigned char previousClusterData[CLUSTER_SIZE];
            readCluster(currCluster, previousClusterData);
            strncpy((char*)&previousClusterData[currPointerInCurrCluster], (char*)&buffer[written], availabeInCurrCluster);
            writeCluster(currCluster, previousClusterData);
            written += availabeInCurrCluster;

            handleList[handle].currentPointer += availabeInCurrCluster;

            int nextFat = readFatEntry(currFat);
            if(nextFat == CLUSTER_EOF)
            {
                // Solicita novo cluster.
                nextFat = clusterToFat(getFreeFatEntry());
                changeFatEntryToType(currFat, nextFat);
            }

            // Navega.
            currFat = nextFat;
            currCluster = fatToCluster(currFat);
        }

        currPointerInCurrCluster = handleList[handle].currentPointer % CLUSTER_SIZE;
        availabeInCurrCluster = CLUSTER_SIZE - currPointerInCurrCluster;
    }

    if(initialPointer + written > initialSize)
    {
        handleList[handle].rec.bytesFileSize = handleList[handle].currentPointer;
    }

    updateRecord(handleList[handle]);

	return written;
}

void updateRecord(Handle hd)
{
    // Atualiza record no disco.
    DIR2 dirHandle = opendir2(hd.parentDirPath);
    DIRENT2 dirEnt;
    while(readdir2(dirHandle, &dirEnt) != -END_OF_DIR)  // Vai para record.
    {
        if(strcmp(dirEnt.name, hd.rec.name) == 0)
        {
            break;
        }
    }
    handleList[dirHandle].currentPointer -= sizeof(Record);
    writeCurrentRecordOfHandle(dirHandle, hd.rec);
    closedir2(dirHandle);
}

int truncate2 (FILE2 handle) {
	init();

    handleList[handle].rec.bytesFileSize = handleList[handle].currentPointer;  // Atualiza tamanho.

    printf("\ntruncando a partir de %d\n", handleList[handle].currentPointer);

    // Navega ate fat do cluster final para o novo tamanho.
    int nClusters = handleList[handle].currentPointer / CLUSTER_SIZE;
    int currFat = handleList[handle].firstFileFatEntry;
    for(int i = 0; i != nClusters; ++i)
    {
        int nextFat = readFatEntry(currFat);
        currFat = nextFat;
    }

    // Arquivo perdeu clusters a partir de 'currFat', remove.
    if(currFat != handleList[handle].firstFileFatEntry)
    {
        deleteFileOnFat(currFat);
    }

    // Atualiza record no disco.

    /*
    DIR2 dirHandle = opendir2(handleList[handle].parentDirPath);
    DIRENT2 dirEnt;
    while(readdir2(dirHandle, &dirEnt) != -END_OF_DIR)  // Vai para record.
    {
        if(strcmp(dirEnt.name, handleList[handle].rec.name) == 0)
        {
            break;
        }
    }
    handleList[dirHandle].currentPointer -= sizeof(Record);
    writeCurrentRecordOfHandle(dirHandle, handleList[handle].rec);
    closedir2(dirHandle);
    */

    updateRecord(handleList[handle]);

	return 0;
}

int seek2 (FILE2 handle, unsigned int offset) {
	init();

    if((int)offset == -1)
    {
        handleList[handle].currentPointer = handleList[handle].rec.bytesFileSize;
    }
    else if(offset <= handleList[handle].rec.bytesFileSize)
    {
        handleList[handle].currentPointer = offset;
    }
    else
    {
        printf("\nOffset of %d bigger than file size of %d bytes", offset, handleList[handle].rec.bytesFileSize);
        return -1;
    }

	return 0;
}

int getFreeFatEntry() {

	int freeFat = searchFatEntryOfType(CLUSTER_FREE);

	changeFatEntryToType(freeFat, CLUSTER_EOF);

	return fatToCluster(freeFat);
}

Record configureNewRecord(char *name) {

	Record record;

	record.TypeVal = TYPEVAL_DIRETORIO;

	strcpy(record.name, name);

	record.firstCluster = getFreeFatEntry();

	record.bytesFileSize = 2*sizeof(record);

	return record;
}

void setHandleToFolderFirstEmptySpace(int handle) {

	DIRENT2 aux;

	while(readdir2(handle, &aux)==0);

	handleList[handle].currentPointer -= sizeof(Record);//to get back to the previous empty space
}

void writeRecordAtEndOfFolder(char *path, Record newRecord) {

	char fatherPath[MAX_FILE_NAME_SIZE];
	char name[MAX_FILE_NAME_SIZE];
	sepName(path, fatherPath, name);

	int handle = opendir2(fatherPath);

	setHandleToFolderFirstEmptySpace(handle);

	writeCurrentRecordOfHandle(handle, newRecord);

	closedir2(handle);
}

void setCurrentPointerToFile(int grandFatherHandle, char *grandFatherName) {

	DIRENT2 aux;
	while(strcmp(aux.name, grandFatherName)) {

		if(readdir2(grandFatherHandle, &aux)!=0) {

			break;
		}
	}

	handleList[grandFatherHandle].currentPointer -= sizeof(Record);
}

void incrementSizeOfRecordFileFolderAt(char *fatherPath, char *fileName, int increment) {

	//root
	if(strcmp(fatherPath, "/")==0) {

		//edit the ".."" dir
		int handle = opendir2(fatherPath);
		handleList[handle].currentPointer = sizeof(Record);//the second offset is where the ".." is located
		Record record = readCurrentRecordOfHandle(handle);
		record.bytesFileSize += increment;
		writeCurrentRecordOfHandle(handle, record);
		closedir2(handle);

	} else {

		char grandFatherPath[MAX_FILE_NAME_SIZE];
		char fatherName[MAX_FILE_NAME_SIZE];
		sepName(fatherPath, grandFatherPath, fatherName);

		int grandFatherHandle = opendir2(grandFatherPath);

		setCurrentPointerToFile(grandFatherHandle, fatherName);

		Record fatherRecord = readCurrentRecordOfHandle(grandFatherHandle);

		fatherRecord.bytesFileSize += increment;

		writeCurrentRecordOfHandle(grandFatherHandle, fatherRecord);

		closedir2(grandFatherHandle);
	}
}

void incrementDotEntryOf(char *fatherPath, int increment) {

	int handle = opendir2(fatherPath);

	Record record = readCurrentRecordOfHandle(handle);

	record.bytesFileSize += increment;

	writeCurrentRecordOfHandle(handle, record);

	closedir2(handle);
}

void incrementDotDotEntryOf(char *fatherPath, char *name, int increment) {

	char auxFatherPath[1000];
	strcpy(auxFatherPath, fatherPath);

	DIRENT2 aux;
	//the ".." dir of all of the children must be updated too
	int handleFather = opendir2(auxFatherPath);
	while(readdir2(handleFather, &aux)==0) {

		if(aux.fileType == TYPEVAL_DIRETORIO) {

			if(strcmp(aux.name, name)!=0) {

				if(strcmp(aux.name, ".")!=0 && strcmp(aux.name, "..")!=0) {

					char childPath[MAX_FILE_NAME_SIZE];
					strcpy(childPath, fatherPath);
					strcat(childPath, "/");
					strcat(childPath, aux.name);

					int childrenHandle = opendir2(childPath);
					handleList[childrenHandle].currentPointer = sizeof(Record);
					Record childRecord = readCurrentRecordOfHandle(childrenHandle);
					childRecord.bytesFileSize += increment;
					writeCurrentRecordOfHandle(childrenHandle, childRecord);

					closedir2(childrenHandle);
				}
			}
		}
	}
	closedir2(handleFather);
}

void changeSizeOfFolder(char *pathname, int increment) {

	char fatherPath[MAX_FILE_NAME_SIZE];
	char name[MAX_FILE_NAME_SIZE];

	sepName(pathname, fatherPath, name);
	incrementSizeOfRecordFileFolderAt(fatherPath, name, increment);

	sepName(pathname, fatherPath, name);
	incrementDotEntryOf(fatherPath, increment);

	sepName(pathname, fatherPath, name);
	incrementDotDotEntryOf(fatherPath, name, increment);
}

void changeSizeOfFile(char *pathname, int increment) {

	char fatherPath[MAX_FILE_NAME_SIZE];
	char name[MAX_FILE_NAME_SIZE];

	sepName(pathname, fatherPath, name);
	incrementSizeOfRecordFileFolderAt(fatherPath, name, increment);

	sepName(pathname, fatherPath, name);
	incrementDotEntryOf(fatherPath, increment);
}

void makeDotAndDotDotEntry(Record record, char *pathname) {

	Record dot = record;
	Record dotDot;

	char fatherPath[MAX_FILE_NAME_SIZE];
	char name[MAX_FILE_NAME_SIZE];

	sepName(pathname, fatherPath, name);

	//root
	if(strcmp(fatherPath, "/")==0) {

		//edit the ".."" dir
		int handle = opendir2(fatherPath);
		handleList[handle].currentPointer = sizeof(Record);//the second offset is where the ".." is located
		dotDot = readCurrentRecordOfHandle(handle);
		closedir2(handle);

	} else {

		char grandFatherPath[MAX_FILE_NAME_SIZE];
		char fatherName[MAX_FILE_NAME_SIZE];
		sepName(fatherPath, grandFatherPath, fatherName);

		int grandFatherHandle = opendir2(grandFatherPath);

		setCurrentPointerToFile(grandFatherHandle, fatherName);

		dotDot = readCurrentRecordOfHandle(grandFatherHandle);
		closedir2(grandFatherHandle);
	}

	strcpy(dot.name, ".");
	strcpy(dotDot.name, "..");

	int lastHandle = opendir2(pathname);

	writeCurrentRecordOfHandle(lastHandle, dot);
	handleList[lastHandle].currentPointer = sizeof(Record);
	writeCurrentRecordOfHandle(lastHandle, dotDot);

	closedir2(lastHandle);
}

int mkdir2 (char *pathname) {
	init();

	char fatherPath[MAX_FILE_NAME_SIZE];
	char name[MAX_FILE_NAME_SIZE];
	sepName(pathname, fatherPath, name);

	Record newRecord = configureNewRecord(name);

	writeRecordAtEndOfFolder(pathname, newRecord);

	changeSizeOfFolder(pathname, sizeof(Record));

	makeDotAndDotDotEntry(newRecord, pathname);

	return 0;
}

void deleteCluster(int clusterIndex) {

	unsigned char *buffer = calloc(1, CLUSTER_SIZE);
	writeCluster(clusterIndex, buffer);
}

void deleteFileOnFat(int fatIndex) {

	int next = readFatEntry(fatIndex);

	changeFatEntryToType(fatIndex, CLUSTER_FREE);

	int clusterIndex = fatToCluster(fatIndex);

	unsigned char buffer[CLUSTER_SIZE];

	readCluster(clusterIndex, buffer);

	deleteCluster(fatToCluster(fatIndex));

	if(next != CLUSTER_EOF) {

		deleteFileOnFat(next);
	}
}

int rmdir2 (char *pathname) {
	init();
	printf("removing %s\n", pathname);

	int recursiveHandle = opendir2(pathname);

	DIRENT2 aux;
	while(readdir2(recursiveHandle, &aux) == 0) {
		
		char *childPath = (char*)malloc(1000);
		strcpy(childPath, pathname);
		strcat(childPath, "/");
		strcat(childPath, aux.name);

		if(strcmp(aux.name, ".") != 0 && strcmp(aux.name, "..") != 0) {

			printf("indo recursao em %s\n", childPath);
			switch(aux.fileType) {

				case TYPEVAL_REGULAR: {
					delete2(childPath);
					break;
				}
				case TYPEVAL_DIRETORIO: {
					rmdir2(childPath);
					break;
				}
				default:{
					printf("DEFAULT: %s\n", aux.name);
				}
			}
		}
		free(childPath);
	}

	closedir2(recursiveHandle);


	//separating the path
	char fatherPath[MAX_FILE_NAME_SIZE];
	char name[MAX_FILE_NAME_SIZE];
	sepName(pathname, fatherPath, name);

	//opening father
	int handleFather = opendir2(fatherPath);

	//DIRENT2 aux;
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

	if(fatherRecord.TypeVal != TYPEVAL_DIRETORIO) {

		printf("ERROR, You can't delete %s, it's not a dir.\n", name);
		return ERROR;
	}

	//Delete fat entries
	int fatIndex = clusterToFat(fatherRecord.firstCluster);
	deleteFileOnFat(fatIndex);

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

	sepName(pathname, fatherPath, name);

	changeSizeOfFolder(pathname, -sizeof(Record));

	closedir2(handleFather);

	return 0;
}

int chdir2 (char *pathname) {
	init();
	return 0;
}

int getcwd2 (char *pathname, int size) {
	init();
    strncpy(pathname, currPath, size);
	return 0;
}

int searchFreeHandleListIndex() {

	int i;
	for(i=0; i<10; i++) {

		if(handleList[i].firstFileFatEntry == ERROR) {

			return i;
		}
	}

	printf("ALGUÉM ESTÁ ESQUECENDO DE FECHAR ARQUIVOS....\n");

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
