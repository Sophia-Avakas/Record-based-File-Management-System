#include "rbfm.h"
#include "math.h"
#include "stdlib.h"
#include "string.h"
#include "stdio.h"
#include "string.h"

PagedFileManager* _pf_manager = PagedFileManager:: instance();
RecordBasedFileManager* RecordBasedFileManager::_rbf_manager = 0;

RecordBasedFileManager* RecordBasedFileManager::instance()
{
    if(!_rbf_manager)
        _rbf_manager = new RecordBasedFileManager();

    return _rbf_manager;
}

RecordBasedFileManager::RecordBasedFileManager()
{
}

RecordBasedFileManager::~RecordBasedFileManager()
{
}

RC RecordBasedFileManager::createFile(const string &fileName) {
	return _pf_manager->createFile(fileName);
}

RC RecordBasedFileManager::destroyFile(const string &fileName) {
	return _pf_manager->destroyFile(fileName);
}

RC RecordBasedFileManager::openFile(const string &fileName, FileHandle &fileHandle) {
	return _pf_manager->openFile(fileName, fileHandle);
}

RC RecordBasedFileManager::closeFile(FileHandle &fileHandle) {
		return _pf_manager->closeFile(fileHandle);
}

RC RecordBasedFileManager::insertRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const void *data, RID &rid) {
	int fieldNumber = recordDescriptor.size();//Number of fields
	printf("%d", fieldNumber);
	int nullFieldsIndicatorActualSize = ceil((double) fieldNumber / CHAR_BIT);//Size of null indicators
	int dataOffset = nullFieldsIndicatorActualSize; //The beginning address of fields in the input data

	int directoryOffset = nullFieldsIndicatorActualSize + sizeof(int); //The beginning address of directory address in the record memory
	int valueOffset = directoryOffset + fieldNumber * sizeof(int); //The beginning address of fields in the record memory

	int maxDataCount = 0;
	for (int i = 0; i < fieldNumber; i++) {
		maxDataCount += recordDescriptor[i].length;
		if(recordDescriptor[i].type == TypeVarChar) {
			maxDataCount += sizeof(int); //add sizeof(int) to store the number of chars
		}
	}

	unsigned char* record = (unsigned char *) malloc(maxDataCount + valueOffset);

	const char* dataNew = (char*)data;
	const char* nullsIndicator = (char *) data; //directly points to the data memory, as the head of data is nullIndicator

	//Copy filedNumber and nullIndicator
	memcpy(record, &fieldNumber, sizeof(int));
	memcpy(record + sizeof(int), dataNew, nullFieldsIndicatorActualSize);

	for (int i = 0; i < fieldNumber; i++) {
		if (!(nullsIndicator[i / 8] & (1 << (7 - i % 8)))) {
			switch(recordDescriptor[i].type) {
			case TypeInt:
				memcpy(record + valueOffset, &dataNew[dataOffset],sizeof(int));
				dataOffset += sizeof(int);
				valueOffset += sizeof(int);
				memcpy(record + directoryOffset, &valueOffset, sizeof(int));
				directoryOffset += sizeof(int);
				break;
			case TypeReal:
				memcpy(record + valueOffset, &dataNew[dataOffset],sizeof(float));
				dataOffset += sizeof(float);
				valueOffset += sizeof(float);
				memcpy(record + directoryOffset, &valueOffset, sizeof(int));
				directoryOffset += sizeof(int);
				break;
			case TypeVarChar:
				int length;
				memcpy(&length, &dataNew[dataOffset],sizeof(int));
				memcpy(record + valueOffset, &dataNew[dataOffset + sizeof(int)],length);
				valueOffset += length;
				dataOffset += sizeof(int) + length;
				memcpy(record + directoryOffset, &valueOffset, sizeof(int));
				directoryOffset += sizeof(int);
				break;
			default: break;
			}
		}
		else {
			memcpy(record + directoryOffset, &valueOffset, sizeof(int));
			directoryOffset += sizeof(int);
		}
	}

	int pageIndex = fileHandle.getNumberOfPages() - 1;
	int numberOfSlots;
	int freeSpace;
	char* page = (char*)malloc(PAGE_SIZE * sizeof(char));
	int visit = 0;

	if (pageIndex < 0) {
		memcpy(page, record, valueOffset);
		numberOfSlots = 1;
		int pageDirectory = PAGE_SIZE- sizeof(int) * 2 - sizeof(int) * 2;
		int startAddress = 0;
		memcpy(page + pageDirectory, &startAddress, sizeof(int));
		memcpy(page + pageDirectory + sizeof(int), &valueOffset, sizeof(int));
		freeSpace = valueOffset;
		memcpy(page + PAGE_SIZE - sizeof(int) * 2, &numberOfSlots, sizeof(int));
		memcpy(page + PAGE_SIZE - sizeof(int), &freeSpace, sizeof(int));
		fileHandle.appendPage(page);
		pageIndex++;
	}
	else {
		while (pageIndex < fileHandle.getNumberOfPages()) { //
			fileHandle.readPage(pageIndex, page);

			memcpy(&freeSpace, &page[PAGE_SIZE - sizeof(int)], sizeof(int));
			memcpy(&numberOfSlots, &page[PAGE_SIZE - sizeof(int) * 2], sizeof(int));

			int pageDirectory = PAGE_SIZE - sizeof(int) * 2 - numberOfSlots * sizeof(int) * 2;
			if (pageDirectory - freeSpace >= valueOffset + sizeof(int) * 2) {
				memcpy(page + freeSpace, record, valueOffset);
				//update directory
				numberOfSlots++;
				memcpy(page + pageDirectory - sizeof(int)*2, & freeSpace, sizeof(int));
				memcpy(page + pageDirectory - sizeof(int), & valueOffset, sizeof(int));

				//update free space and number of slots
				freeSpace += valueOffset;
				memcpy(page + PAGE_SIZE - sizeof(int) * 2, &numberOfSlots, sizeof(int));
				memcpy(page + PAGE_SIZE - sizeof(int), &freeSpace, sizeof(int));
				fileHandle.writePage(pageIndex, page);
				break;
			}
			else {
				//if no enough space in last page, go to first page
				if (pageIndex == (fileHandle.getNumberOfPages() - 1) && visit == 0) {
					pageIndex = 0;
					visit +=1;
				}
				//no space in all pages, append a new page
				else if (pageIndex == (fileHandle.getNumberOfPages() - 1) && visit == 1){
					memcpy(page, record, valueOffset);
					numberOfSlots = 1;
					int pageDirectory = PAGE_SIZE- sizeof(int) * 2 - sizeof(int) * 2;
					int startAddress = 0;
					memcpy(page + pageDirectory, &startAddress, sizeof(int));
					memcpy(page + pageDirectory + sizeof(int), &valueOffset, sizeof(int));
					freeSpace = valueOffset;
					memcpy(page + PAGE_SIZE - sizeof(int) * 2, &numberOfSlots, sizeof(int));
					memcpy(page + PAGE_SIZE - sizeof(int), &freeSpace, sizeof(int));
					fileHandle.appendPage(page);
					pageIndex++;
					break;

				}

				else pageIndex++;
			}
		}
	}

	rid.slotNum = numberOfSlots;
	rid.pageNum = pageIndex;
	free(record);
	free(page);
	return 0;
}

RC RecordBasedFileManager::readRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const RID &rid, void *data) {
	char* page = (char*)malloc(PAGE_SIZE * sizeof(char));
	fileHandle.readPage(rid.pageNum, page);
	int fieldNumber = recordDescriptor.size();

	int slotDir = 0;
	memcpy(&slotDir,page+PAGE_SIZE - sizeof(int) * 2 - (2 * sizeof(int) * rid.slotNum),sizeof(int));
	char* record = page + slotDir;

	//Copy filedNumber and nullIndicator
	int nullFieldsIndicatorActualSize = ceil((double) fieldNumber / CHAR_BIT);
	memcpy(data, record + sizeof(int), nullFieldsIndicatorActualSize);
	char* nullsIndicator = record + sizeof(int);
	int dataOffset = nullFieldsIndicatorActualSize;
	int valueOffset = sizeof(int) + nullFieldsIndicatorActualSize + sizeof(int) * fieldNumber;
	int directoryOffset = sizeof(int) + nullFieldsIndicatorActualSize;
	char* dataNew = (char*)data;

	int prevDir = 0; //the last address of previous field
	int curDir = 0; //the last address of current field
	int length = 0; //the length of vchar
	for (int i = 0; i < fieldNumber; i++) {
		if (!(nullsIndicator[i / 8] & (1 << (7 - i % 8)))) {
			switch(recordDescriptor[i].type) {
			case TypeInt:
				memcpy(&dataNew[dataOffset], record + valueOffset,sizeof(int));
				dataOffset += sizeof(int);
				valueOffset += sizeof(int);
				break;
			case TypeReal:
				memcpy(&dataNew[dataOffset], record + valueOffset,sizeof(float));
				dataOffset += sizeof(float);
				valueOffset += sizeof(float);
				break;
			case TypeVarChar:
				if(i == 0) prevDir = valueOffset;
				else memcpy(&prevDir,record + directoryOffset + (i-1) * sizeof(int), sizeof(int));
				memcpy(&curDir,record + directoryOffset + i * sizeof(int), sizeof(int));
				length = curDir - prevDir; //get the length of vchar
				memcpy(&dataNew[dataOffset],&length,sizeof(int));
				memcpy(&dataNew[dataOffset + sizeof(int)],record + valueOffset,length);
				valueOffset += length;
				dataOffset += sizeof(int) + length;
				break;
			default: break;
			}
		}
	}

	free(page);
	return 0;
}


RC RecordBasedFileManager::printRecord(const vector<Attribute> &recordDescriptor, const void *data) {
	int fieldNumber = recordDescriptor.size();
	int dataOffset = ceil((double) fieldNumber / CHAR_BIT);//Pointer to field data
	const char* dataNew = (char*)data;

	for (int i = 0; i < fieldNumber; i++) {
		if (!(dataNew[i / 8] & (1 << (7 - i % 8)))) {

			switch(recordDescriptor[i].type) {
			case TypeVarChar: {
				int length;
				memcpy(&length, &dataNew[dataOffset], sizeof(int));
				char* content = new char[length + 1];
				memcpy(content, &dataNew[dataOffset + sizeof(int)], length);
				content[length] = '\0';//make the last char 0, indicating end of string
				printf("%s: %s\n", recordDescriptor[i].name.c_str(), content);

				dataOffset += length + sizeof(int);
				break;
			}
			case TypeInt: {
				int intNumber;
				memcpy(&intNumber, &dataNew[dataOffset], sizeof(int));
				printf("%s: %d\n", recordDescriptor[i].name.c_str(), intNumber);

				dataOffset += sizeof(int);
				break;
			}
			case TypeReal: {
				float realNumber;
				memcpy(&realNumber, &dataNew[dataOffset], sizeof(float));
				printf("%s: %f\n", recordDescriptor[i].name.c_str(), realNumber);
				dataOffset += sizeof(float);
				break;
			}
			default: break;
			}
		}
		else {
			printf("%s: NULL\n", recordDescriptor[i].name.c_str());

		}
	}
	return 0;
}
