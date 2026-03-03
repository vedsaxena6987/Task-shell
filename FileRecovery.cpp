#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <windows.h>
#include <bits/stdc++.h>
#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#pragma pack(push, 1)
struct BootSector
{
    uint8_t jump[3];
    char name[8];
    uint16_t bytesPerSector;
    uint8_t sectorsPerCluster;
    uint16_t reservedSectors;
    uint8_t unused0[3];
    uint16_t unused1;
    uint8_t media;
    uint16_t unused2;
    uint16_t sectorsPerTrack;
    uint16_t headsPerCylinder;
    uint32_t hiddenSectors;
    uint32_t unused3;
    uint32_t unused4;
    uint64_t totalSectors;
    uint64_t mftStart;
    uint64_t mftMirrorStart;
    uint32_t clustersPerFileRecord;
    uint32_t clustersPerIndexBlock;
    uint64_t serialNumber;
    uint32_t checksum;
    uint8_t bootloader[426];
    uint16_t bootSignature;
};

struct FileRecordHeader
{
    uint32_t magic;
    uint16_t updateSequenceOffset;
    uint16_t updateSequenceSize;
    uint64_t logSequence;
    uint16_t sequenceNumber;
    uint16_t hardLinkCount;
    uint16_t firstAttributeOffset;
    uint16_t inUse : 1;
    uint16_t isDirectory : 1;
    uint32_t usedSize;
    uint32_t allocatedSize;
    uint64_t fileReference;
    uint16_t nextAttributeID;
    uint16_t unused;
    uint32_t recordNumber;
};

struct AttributeHeader
{
    uint32_t attributeType;
    uint32_t length;
    uint8_t nonResident;
    uint8_t nameLength;
    uint16_t nameOffset;
    uint16_t flags;
    uint16_t attributeID;
};

struct ResidentAttributeHeader : AttributeHeader
{
    uint32_t attributeLength;
    uint16_t attributeOffset;
    uint8_t indexed;
    uint8_t unused;
};

struct FileNameAttributeHeader : ResidentAttributeHeader
{
    uint64_t parentRecordNumber : 48;
    uint64_t sequenceNumber : 16;
    uint64_t creationTime;
    uint64_t modificationTime;
    uint64_t metadataModificationTime;
    uint64_t readTime;
    uint64_t allocatedSize;
    uint64_t realSize;
    uint32_t flags;
    uint32_t repase;
    uint8_t fileNameLength;
    uint8_t namespaceType;
    wchar_t fileName[1];
};

struct NonResidentAttributeHeader : AttributeHeader
{
    uint64_t firstCluster;
    uint64_t lastCluster;
    uint16_t dataRunsOffset;
    uint16_t compressionUnit;
    uint32_t unused;
    uint64_t attributeAllocated;
    uint64_t attributeSize;
    uint64_t streamDataSize;
};

struct RunHeader
{
    uint8_t lengthFieldBytes : 4;
    uint8_t offsetFieldBytes : 4;
};
#pragma pack(pop)

struct File
{
    uint64_t parent;
    char *name;
};

File *files;

DWORD bytesAccessed;
HANDLE drive;

BootSector bootSector;

#define MFT_FILE_SIZE (1024)
uint8_t mftFile[MFT_FILE_SIZE];

#define MFT_FILES_PER_BUFFER (65536)
uint8_t mftBuffer[MFT_FILES_PER_BUFFER * MFT_FILE_SIZE];

char *DuplicateName(wchar_t *name, size_t nameLength)
{
    static char *allocationBlock = nullptr;
    static size_t bytesRemaining = 0;

    size_t bytesNeeded = WideCharToMultiByte(CP_UTF8, 0, name, nameLength, NULL, 0, NULL, NULL) + 1;

    if (bytesRemaining < bytesNeeded)
    {
        allocationBlock = (char *)malloc((bytesRemaining = 16 * 1024 * 1024));
    }

    char *buffer = allocationBlock;
    buffer[bytesNeeded - 1] = 0;
    WideCharToMultiByte(CP_UTF8, 0, name, nameLength, allocationBlock, bytesNeeded, NULL, NULL);

    bytesRemaining -= bytesNeeded;
    allocationBlock += bytesNeeded;

    return buffer;
}

void Read(void *buffer, uint64_t from, uint64_t count)
{
    LONG high = from >> 32;
    SetFilePointer(drive, from & 0xFFFFFFFF, &high, FILE_BEGIN);
    ReadFile(drive, buffer, count, &bytesAccessed, NULL);
  
}

#define MAX_FILES 10000
int count = 0;
char *deletedFiles[MAX_FILES];
uint64_t deletedFileReferences[MAX_FILES];

struct DataAttributeHeader : AttributeHeader
{
    uint64_t dataStartCluster;
    uint32_t dataLength;
    uint8_t padding[8];
};

void RecoverFile(uint64_t fileReference)
{

    printf("Attempting to recover file with reference: %llu\n", fileReference);

    uint64_t bytesPerCluster = bootSector.bytesPerSector * bootSector.sectorsPerCluster;

    Read(&mftFile, bootSector.mftStart * bytesPerCluster + (fileReference * MFT_FILE_SIZE), MFT_FILE_SIZE);

    FileRecordHeader *fileRecord = (FileRecordHeader *)mftFile;
    if (fileRecord->magic != 0x454C4946)
    {
        fprintf(stderr, "Error: Invalid MFT record for reference: %llu\n", fileReference);
        return;
    }

    AttributeHeader *attribute = (AttributeHeader *)((uint8_t *)fileRecord + fileRecord->firstAttributeOffset);
    while ((uint8_t *)attribute - (uint8_t *)fileRecord < MFT_FILE_SIZE)
    {
        if (attribute->attributeType == 0x80)
        {
            DataAttributeHeader *dataAttr = (DataAttributeHeader *)attribute;
            uint64_t dataStartCluster = dataAttr->dataStartCluster;
            uint32_t dataLength = dataAttr->dataLength;

            printf("File data found, starting from cluster: %llu, length: %u bytes\n", dataStartCluster, dataLength);

            // Recover the file's data by reading its clusters
            FILE *recoveredFile = fopen("recovered_file.txt", "wb");
            if (!recoveredFile)
            {
                fprintf(stderr, "Error: Could not open file to save recovered data.\n");
                return;
            }

            uint8_t *fileData = (uint8_t *)malloc(dataLength);
            if (!fileData)
            {
                fprintf(stderr, "Error: Memory allocation failed.\n");
                return;
            }

            // Read data clusters and recover the file content
            for (uint64_t cluster = dataStartCluster; dataLength > 0; cluster++)
            {
                uint64_t clusterOffset = cluster * bytesPerCluster;
                uint32_t bytesToRead = (dataLength > bytesPerCluster) ? bytesPerCluster : dataLength;

                Read(fileData, clusterOffset, bytesToRead);
                fwrite(fileData, 1, bytesToRead, recoveredFile);
                dataLength -= bytesToRead;
            }

            free(fileData);
            fclose(recoveredFile);
            printf("File recovery successful. Data saved to 'recovered_file.txt'.\n");
            return;
        }
        else if (attribute->attributeType == 0xFFFFFFFF)
        {
            break; // No more attributes
        }

        attribute = (AttributeHeader *)((uint8_t *)attribute + attribute->length);
    }

    fprintf(stderr, "Error: No data attribute found for file reference: %llu\n", fileReference);
}

int main()
{
    drive = CreateFile("\\\\.\\C:", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

    Read(&bootSector, 0, 512);

    uint64_t bytesPerCluster = bootSector.bytesPerSector * bootSector.sectorsPerCluster;

    Read(&mftFile, bootSector.mftStart * bytesPerCluster, MFT_FILE_SIZE);

    FileRecordHeader *fileRecord = (FileRecordHeader *)mftFile;
    AttributeHeader *attribute = (AttributeHeader *)(mftFile + fileRecord->firstAttributeOffset);
    NonResidentAttributeHeader *dataAttribute = nullptr;
    uint64_t approximateRecordCount = 0;
    assert(fileRecord->magic == 0x454C4946);

    while (true)
    {
        if (attribute->attributeType == 0x80)
        {
            dataAttribute = (NonResidentAttributeHeader *)attribute;
        }
        else if (attribute->attributeType == 0xB0)
        {
            approximateRecordCount = ((NonResidentAttributeHeader *)attribute)->attributeSize * 8;
        }
        else if (attribute->attributeType == 0xFFFFFFFF)
        {
            break;
        }

        attribute = (AttributeHeader *)((uint8_t *)attribute + attribute->length);
    }

    assert(dataAttribute);
    RunHeader *dataRun = (RunHeader *)((uint8_t *)dataAttribute + dataAttribute->dataRunsOffset);
    uint64_t clusterNumber = 0, recordsProcessed = 0;

    while (((uint8_t *)dataRun - (uint8_t *)dataAttribute) < dataAttribute->length && dataRun->lengthFieldBytes)
    {
        uint64_t length = 0, offset = 0;

        for (int i = 0; i < dataRun->lengthFieldBytes; i++)
        {
            length |= (uint64_t)(((uint8_t *)dataRun)[1 + i]) << (i * 8);
        }

        for (int i = 0; i < dataRun->offsetFieldBytes; i++)
        {
            offset |= (uint64_t)(((uint8_t *)dataRun)[1 + dataRun->lengthFieldBytes + i]) << (i * 8);
        }

        if (offset & ((uint64_t)1 << (dataRun->offsetFieldBytes * 8 - 1)))
        {
            for (int i = dataRun->offsetFieldBytes; i < 8; i++)
            {
                offset |= (uint64_t)0xFF << (i * 8);
            }
        }

        clusterNumber += offset;
        dataRun = (RunHeader *)((uint8_t *)dataRun + 1 + dataRun->lengthFieldBytes + dataRun->offsetFieldBytes);

        uint64_t filesRemaining = length * bytesPerCluster / MFT_FILE_SIZE;
        uint64_t positionInBlock = 0;

        while (filesRemaining)
        {
            fprintf(stderr, "%d%% ", (int)(recordsProcessed * 100 / approximateRecordCount));

            uint64_t filesToLoad = MFT_FILES_PER_BUFFER;
            if (filesRemaining < MFT_FILES_PER_BUFFER)
                filesToLoad = filesRemaining;
            Read(&mftBuffer, clusterNumber * bytesPerCluster + positionInBlock, filesToLoad * MFT_FILE_SIZE);
            positionInBlock += filesToLoad * MFT_FILE_SIZE;
            filesRemaining -= filesToLoad;

            for (int i = 0; i < filesToLoad; i++)
            {

                FileRecordHeader *fileRecord = (FileRecordHeader *)(mftBuffer + MFT_FILE_SIZE * i);
                recordsProcessed++;

                if (fileRecord->inUse)
                    continue;

                AttributeHeader *attribute = (AttributeHeader *)((uint8_t *)fileRecord + fileRecord->firstAttributeOffset);
                if (fileRecord->magic != 0x454C4946)
                    continue;

                while ((uint8_t *)attribute - (uint8_t *)fileRecord < MFT_FILE_SIZE)
                {
                    if (attribute->attributeType == 0x30)
                    {
                        FileNameAttributeHeader *fileNameAttr = (FileNameAttributeHeader *)attribute;
                        char *fileName = DuplicateName(fileNameAttr->fileName, fileNameAttr->fileNameLength);

                        if (strstr(fileName, ".txt") != NULL)
                        {
                            if (count < MAX_FILES)
                            {
                                deletedFiles[count] = fileName;
                                deletedFileReferences[count] = fileRecord->fileReference;
                                count++;
                            }
                        }
                    }
                    else if (attribute->attributeType == 0xFFFFFFFF)
                    {
                        break;
                    }

                    attribute = (AttributeHeader *)((uint8_t *)attribute + attribute->length);
                }
            }
        }
    }

    if (count > 0)
    {
        int choice;
        printf("Enter the number of the file you want to recover (1 to %d) from these: \n", count);
        for (int i = 0; i < count; i++)
        {
            printf("(%d )  %s\n", i, deletedFiles[i]);
        }
        printf("Enter the your choice ");
        scanf("%d", &choice);

        if (choice >= 1 && choice <= count)
        {
            RecoverFile(deletedFileReferences[choice]);
        }
        else
        {
            printf("Invalid choice.\n");
        }
    }
    else
    {
        printf("No deleted .txt files found.\n");
    }
    printf("\nFound %d files.\n", count);

    return 0;
}