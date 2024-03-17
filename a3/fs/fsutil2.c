#include "fsutil2.h"
#include "bitmap.h"
#include "cache.h"
#include "debug.h"
#include "directory.h"
#include "file.h"
#include "filesys.h"
#include "free-map.h"
#include "fsutil.h"
#include "inode.h"
#include "off_t.h"
#include "partition.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include "../interpreter.h"

#define SECTOR_SIZE 512 // each sector is 512 bytes

int min(int sourceFileSize, int freeSpace); //copy_in helper function
int fsutil_read_at(char *file_name, void *buffer, unsigned size, offset_t file_ofs); //copy_out helper function

int copy_in(char *fname) {
  //copy the file on real hard dive into the shell's hard drive with the same name in the root directory
  FILE* sourceFile;
  char* contentBuffer;
  int sourceFileSize;
  int freeSpace;
  int destinationFileSize;
  int write;
  bool createSuccess;

  sourceFile = fopen(fname, "r");
  if(sourceFile == NULL) return handle_error(BAD_COMMAND);

  // determine the length of the file
  fseek(sourceFile, 0, SEEK_END);
  sourceFileSize = ftell(sourceFile);
  rewind(sourceFile);

  // get free space (byte) on hard drive
  freeSpace = fsutil_freespace()*SECTOR_SIZE; 
  
  // create copied new file on shell's hard drive, with new file size = source file size(if free space is enough)
  createSuccess = fsutil_create(fname, min(sourceFileSize, freeSpace));
  if(!createSuccess){
    fclose(sourceFile);
    return handle_error(FILE_CREATION_ERROR);
  }

  // read from source file, store contents in contentBuffer
  contentBuffer = (char *)malloc((sourceFileSize + 1) * sizeof(char)); 
  fread(contentBuffer, sizeof(char), sourceFileSize, sourceFile);
  contentBuffer[sourceFileSize] = '\0';   // add null terminator at the end of the buffer

  // if new file size < source file size, print warning message
  destinationFileSize = fsutil_size(fname); 
  if(destinationFileSize < sourceFileSize) printf("Warning: could only write %d out of %ld bytes (reached end of file)\n", destinationFileSize, (long int)sourceFileSize);

  // write content to the new file
  write = fsutil_write(fname, contentBuffer, fsutil_size(fname)); //write to the new file on shell's disk
  if(write == -1){
    fclose(sourceFile);
    free(contentBuffer);
    return handle_error(FILE_WRITE_ERROR);
  } 

  fclose(sourceFile);
  free(contentBuffer);

  return 0;
}

int copy_out(char *fname) {
  //copy the file on shell's hard dive to real hard drive with the same name
  char* contentBuffer;
  int shellDiskFileSize;
  int read;
  FILE* realDiskFile;

  // read from to-be-copied file
  shellDiskFileSize = fsutil_size(fname); // get file size
  contentBuffer = (char *)malloc((shellDiskFileSize + 1) * sizeof(char));
  read = fsutil_read_at(fname, contentBuffer, shellDiskFileSize, 0); // read from file offset 0
  if(read == -1) return handle_error(FILE_READ_ERROR);

  //write to file on real hard drive
  realDiskFile = fopen(fname, "w"); // create a file on real hard drive with the same name
  if(realDiskFile == NULL) return handle_error(FILE_CREATION_ERROR);
  fwrite(contentBuffer, sizeof(char), sizeof(contentBuffer)-1, realDiskFile);

  fclose(realDiskFile);
  free(contentBuffer);

  return 0;
}

void find_file(char *pattern) {
  // search for an input pattern in all files on shell's hard drive
  struct dir *dir;
  char name[NAME_MAX + 1]; // store one file name
  char* contentBuffer;
  int fileSize = 0;

  dir = dir_open_root();

  // read all files on disk, search each of them for patterns
  while (dir_readdir(dir, name)){
    // read one file, load its content into contentBuffer
    fileSize = fsutil_size(name);
    contentBuffer = (char *)malloc((fileSize + 1) * sizeof(char));
    fsutil_read_at(name, contentBuffer, (unsigned int)fileSize, 0);
    contentBuffer[fileSize] = '\0';

    // search pattern in contentBuffer, if patter matches, print out file name
    if(strstr(contentBuffer, pattern) != NULL) printf("%s\n", name);

    free(contentBuffer);
  }

  dir_close(dir);

  return;
}

void fragmentation_degree() {
  // TODO
}

int defragment() {
  // TODO
  return 0;
}

void recover(int flag) {
  if (flag == 0) { // recover deleted inodes

    // TODO
  } else if (flag == 1) { // recover all non-empty sectors

    // TODO
  } else if (flag == 2) { // data past end of file.

    // TODO
  }
}

int min(int sourceFileSize, int freeSpace) {
  return (sourceFileSize < freeSpace) ? sourceFileSize : freeSpace;
}

int fsutil_read_at(char *file_name, void *buffer, unsigned size, offset_t file_ofs) {
  struct file *file_s = get_file_by_fname(file_name);
  if (file_s == NULL) {
    file_s = filesys_open(file_name);
    if (file_s == NULL) {
      return -1;
    }
    add_to_file_table(file_s, file_name);
  }
  return file_read_at(file_s, buffer, size, file_ofs);
}