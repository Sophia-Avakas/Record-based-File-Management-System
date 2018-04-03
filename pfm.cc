#include "pfm.h"
#include "stdio.h"
#include "unistd.h"
#include <stdlib.h>
#include <cstring>

PagedFileManager* PagedFileManager::_pf_manager = 0;

PagedFileManager* PagedFileManager::instance()
{
    if(!_pf_manager)
        _pf_manager = new PagedFileManager();

    return _pf_manager;
}


PagedFileManager::PagedFileManager()
{
}


PagedFileManager::~PagedFileManager()
{
}


RC PagedFileManager::createFile(const string &fileName)
{
	//if the file already exists, print error
	if ((access(fileName.c_str(), F_OK)) == 0) {
		perror("File already exists and cannot be created!");
		return -1;
	}
	else {
		FILE * pFile;
		pFile = fopen(fileName.c_str(), "wb");
		if (pFile == NULL)
			perror("Open file failed!");


		//create a hidden page in the file here
		FileHandle fileHandle;
		fileHandle.fileOpened = pFile;
		RC rc = fileHandle.createHiddenPage();
		fclose(pFile);
		return rc;
	}
	return 0;
}


RC PagedFileManager::destroyFile(const string &fileName)
{
	if ((access(fileName.c_str(), F_OK)) == 0) {
		FILE * pFile;
		pFile = fopen(fileName.c_str(), "wb");
		if (pFile == NULL)
			printf("Open file failed!");
		if (remove(fileName.c_str()) != 0)
			perror("Error destroying file!");
		else {
			puts("File successfully destroyed!");
			return 0;
		}
	}
	else
		perror("File does not exist!");
    return -1;
}


RC PagedFileManager::openFile(const string &fileName, FileHandle &fileHandle)
{
	if ((access(fileName.c_str(), F_OK)) != 0) {
		perror("File does not exist!");
	}

	if (fileHandle.fileOpened != NULL)
		perror("FileHandle is already a handle for some open file!");
	FILE* pFile = fopen(fileName.c_str(), "rb+");
	fileHandle.setFileOpened(pFile);
	if (fileHandle.fileOpened != NULL) {

		return 0;
	}
	else {
		perror("Open file failed!");
	}



    return -1;
}


RC PagedFileManager::closeFile(FileHandle &fileHandle)
{
	if (fileHandle.fileOpened == NULL) {
		perror("File is not open!");
		return -1;
	}
	fclose(fileHandle.fileOpened);
	fileHandle.fileOpened = NULL;
	return 0;


}


FileHandle::FileHandle()
{
    readPageCounter = 0;
    writePageCounter = 0;
    appendPageCounter = 0;
    fileOpened = NULL;
}


FileHandle::~FileHandle()
{
}


RC FileHandle::readPage(PageNum pageNum, void *data)
{
	PageNum totalPages = getNumberOfPages();
	if (pageNum > totalPages - 1) {
		perror("Page does not exist!");
		return -1;
	}

	fseek(fileOpened, pageNum * PAGE_SIZE + PAGE_SIZE, SEEK_SET); //skip the hidden page
	size_t result = fread(data, sizeof(char), PAGE_SIZE, fileOpened);
	if (result != PAGE_SIZE) {
		perror("Reading error!");
		return -1;
	}
	//update readPageCounter
	readPageCounter = getReadPageCnt();
	readPageCounter += 1;
	writeReadPageCnt(readPageCounter);
    return 0;
}


RC FileHandle::writePage(PageNum pageNum, const void *data)
{
	PageNum totalPages = getNumberOfPages();
	if (totalPages == 0 && pageNum == 0) {
		appendPage(data);
	}
	else if (pageNum > totalPages - 1) {
		perror("Page does not exist!");
		return -1;
	}
	else {
		fseek(fileOpened, sizeof(char) * PAGE_SIZE * pageNum + PAGE_SIZE, SEEK_SET);
		size_t result = fwrite(data, sizeof(char), PAGE_SIZE, fileOpened);
		if (result != PAGE_SIZE) {
			perror("Writing error!");
			return -1;
		}

		if (fflush(fileOpened) != 0) {
			perror("Cannot flush file.");
			return -1;
		}

		//update writePageCounter
		writePageCounter = getWritePageCnt();
		writePageCounter += 1;
		writeWritePageCnt(writePageCounter);
	}

    return 0;
}


RC FileHandle::appendPage(const void *data)
{
	fseek(fileOpened, 0, SEEK_END);
	size_t result = fwrite(data, sizeof(char), PAGE_SIZE, fileOpened);
	if (result != PAGE_SIZE) {
		perror("Appending page error!");
		return -1;
	}
	fflush(fileOpened);

	//update appendPageCounter
	appendPageCounter = getAppendPageCnt();
	appendPageCounter += 1;
	writeAppendPageCnt(appendPageCounter);

	//update the number of pages
	unsigned pageNumber = getNumberOfPages();
	pageNumber++;
	writeNumberOfPages(pageNumber);
	return 0;
}


unsigned FileHandle::getNumberOfPages()
{
	fseek(fileOpened, 0, SEEK_SET);
	unsigned numPages = 0;
	size_t result = fread(&numPages, sizeof(unsigned), 1, fileOpened);
	if (result != 1) {
		perror("Reading number of pages error!");
		return 0; //need to change later
	}
	return numPages;
}


RC FileHandle::writeNumberOfPages(unsigned numOfPages)
{
	fseek(fileOpened, 0, SEEK_SET);

	size_t result = fwrite(&numOfPages, sizeof(unsigned), 1, fileOpened);
	if (result != 1) {
		perror("writing number of pages error!");
		return -1;
	}
	fflush(fileOpened);
	return 0;
}

int FileHandle::getReadPageCnt()
{
	fseek(fileOpened, sizeof(unsigned), SEEK_SET);
	unsigned readPageCnt = 0;
	size_t result = fread(&readPageCnt, sizeof(unsigned), 1, fileOpened);
	if (result != 1) {
		perror("Reading readPageCount error!");
		return 0; //need to change later
	}
	return readPageCnt;
}

RC FileHandle::writeReadPageCnt(unsigned readPageCnt)
{
	fseek(fileOpened, sizeof(unsigned), SEEK_SET);

	size_t result = fwrite(&readPageCnt, sizeof(unsigned), 1, fileOpened);
	if (result != 1) {
		perror("Writing readPageCount error!");
		return -1;
	}
	fflush(fileOpened);
	return 0;
}

int FileHandle::getWritePageCnt()
{
	fseek(fileOpened, sizeof(unsigned)*2, SEEK_SET);
	unsigned writePageCnt = 0;
	size_t result = fread(&writePageCnt, sizeof(unsigned), 1, fileOpened);
	if (result != 1) {
		perror("Reading writePageCount error!");
		return 0; //need to change later
	}
	return writePageCnt;
}

RC FileHandle::writeWritePageCnt(unsigned writePageCnt)
{
	fseek(fileOpened, sizeof(unsigned)*2, SEEK_SET);

	size_t result = fwrite(&writePageCnt, sizeof(unsigned), 1, fileOpened);
	if (result != 1) {
		perror("Writing writePageCount error!");
		return -1;
	}
	fflush(fileOpened);
	return 0;
}

int FileHandle::getAppendPageCnt()
{
	fseek(fileOpened, sizeof(unsigned)*3, SEEK_SET);
	unsigned appendPageCnt = 0;
	size_t result = fread(&appendPageCnt, sizeof(unsigned), 1, fileOpened);
	if (result != 1) {
		perror("Reading appendPageCount error!");
		return 0; //need to change later
	}
	return appendPageCnt;
}

RC FileHandle::writeAppendPageCnt(unsigned appendPageCnt)
{
	fseek(fileOpened, sizeof(unsigned)*3, SEEK_SET);

	size_t result = fwrite(&appendPageCnt, sizeof(unsigned), 1, fileOpened);
	if (result != 1) {
		perror("Writing appendPageCount error!");
		return -1;
	}
	fflush(fileOpened);
	return 0;
}

RC FileHandle::collectCounterValues(unsigned &readPageCount, unsigned &writePageCount, unsigned &appendPageCount)
{
	readPageCount = readPageCounter;
	writePageCount = writePageCounter;
	appendPageCount = appendPageCounter;
    return 0;
}


RC FileHandle::createHiddenPage()
{
	void *data = malloc(PAGE_SIZE);
	unsigned numPages = 0;
	unsigned readPageCount = 0;
	unsigned writePageCount = 0;
	unsigned appendPageCount = 0;
	int offset = 0;
	memcpy(data+offset,&numPages,sizeof(unsigned));
	offset += sizeof(unsigned);
	memcpy(data+offset,&readPageCount,sizeof(unsigned));
	offset += sizeof(unsigned);
	memcpy(data+offset,&writePageCount,sizeof(unsigned));
	offset += sizeof(unsigned);
	memcpy(data+offset,&appendPageCount,sizeof(unsigned));

	//write the hidden page
	size_t result = fwrite(data, sizeof(char), PAGE_SIZE, fileOpened);
	if (result != PAGE_SIZE) {
		perror("Writing error!");
		return -1;
	}

	if (fflush(fileOpened) != 0) {
		perror("Cannot flush file.");
		return -1;
	}

	free(data);

	return 0;
}

void FileHandle::setFileOpened(FILE * pFile)
{
	fileOpened = pFile;
	readPageCounter = getReadPageCnt();
	writePageCounter = getWritePageCnt();
	appendPageCounter = getAppendPageCnt();
}
