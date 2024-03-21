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

int min(int source_file_size, int free_space); //copy_in helper function
int fsutil_read_at(char *file_name, void *buffer, unsigned size, offset_t file_ofs); //copy_out helper function
bool file_is_fragmented(block_sector_t blocks[DIRECT_BLOCKS_COUNT]);

struct inode_indirect_block_sector {
  block_sector_t blocks[INDIRECT_BLOCKS_PER_SECTOR];
};

int copy_in(char *fname) {
  //copy the file on real hard dive into the shell's hard drive with the same name in the root directory
  FILE* source_file;
  char* content_buffer;
  int source_file_size;
  int free_space;
  int destination_file_size;
  int write;
  bool create_success;

  source_file = fopen(fname, "r");
  if(source_file == NULL) return handle_error(BAD_COMMAND);

  // determine the length of the file
  fseek(source_file, 0, SEEK_END);
  source_file_size = ftell(source_file);
  // printf("size: %d\n", source_file_size);
  rewind(source_file);

  // get free space (byte) on hard drive
  free_space = fsutil_freespace()*BLOCK_SECTOR_SIZE; 
  
  // create copied new file on shell's hard drive, with new file size = source file size(if free space is enough)
  create_success = fsutil_create(fname, min(source_file_size, free_space));
  // printf("min size: %d\n", min(source_file_size, free_space));
  if(!create_success){
    fclose(source_file);
    return handle_error(FILE_CREATION_ERROR);
  }

  // read from source file, store contents in content_buffer
  content_buffer = (char *)malloc((source_file_size + 1) * sizeof(char)); 
  fread(content_buffer, sizeof(char), source_file_size, source_file);
  content_buffer[source_file_size] = '\0';   // add null terminator at the end of the buffer

  // if new file size < source file size, print warning message
  destination_file_size = fsutil_size(fname); 
  if(destination_file_size < source_file_size) printf("Warning: could only write %d out of %ld bytes (reached end of file)\n", destination_file_size, (long int)source_file_size);

  // write content to the new file
  write = fsutil_write(fname, content_buffer, fsutil_size(fname)); //write to the new file on shell's disk
  if(write == -1){
    fclose(source_file);
    free(content_buffer);
    return handle_error(FILE_WRITE_ERROR);
  } 

  fclose(source_file);
  free(content_buffer);

  return 0;
}

int copy_out(char *fname) {
  //copy the file on shell's hard dive to real hard drive with the same name
  char* content_buffer;
  int shell_disk_file_size;
  int read;
  FILE* real_disk_file;

  // read from to-be-copied file
  shell_disk_file_size = fsutil_size(fname); // get file size
  content_buffer = (char *)malloc((shell_disk_file_size + 1) * sizeof(char));
  read = fsutil_read_at(fname, content_buffer, shell_disk_file_size, 0); // read from file offset 0
  if(read == -1) return handle_error(FILE_READ_ERROR);

  //write to file on real hard drive
  real_disk_file = fopen(fname, "w");// create a file on real hard drive with the same name
  if (real_disk_file == NULL) {
    free(content_buffer);
    return handle_error(FILE_CREATION_ERROR);
  }
  fwrite(content_buffer, sizeof(char), shell_disk_file_size, real_disk_file);

  fclose(real_disk_file);
  free(content_buffer);

  return 0;
}


void find_file(char *pattern) {
  // search for an input pattern in all files on shell's hard drive
  struct dir *dir;
  char name[NAME_MAX + 1]; // store one file name
  char* content_buffer;
  int file_size = 0;

  dir = dir_open_root();

  // read all files on disk, search each of them for patterns
  while (dir_readdir(dir, name)){
    // read one file, load its content into content_buffer
    file_size = fsutil_size(name);
    content_buffer = (char *)malloc((file_size + 1) * sizeof(char));
    fsutil_read_at(name, content_buffer, (unsigned int)file_size, 0);
    content_buffer[file_size] = '\0';

    // search pattern in content_buffer, if patter matches, print out file name
    if(strstr(content_buffer, pattern) != NULL) printf("%s\n", name);

    free(content_buffer);
  }

  dir_close(dir);

  return;
}

void fragmentation_degree() {
  struct dir *dir;
  struct file *file; 
  char fname[NAME_MAX + 1];
  int fragmentable_file_count = 0;
  bool is_fragmentable = false;
  int sector_count = 0; // number of sectors that one file taken
  struct inode_indirect_block_sector *indirect_block_sector;
  struct inode_indirect_block_sector *doubly_indirect_block_sector;
  int fragmented_file_count = 0;
  bool done_check = false;
  bool is_fragmented = false;

  dir = dir_open_root();

  // traverse the file system
  while(dir_readdir(dir, fname)){
    
    // get a file
    file = get_file_by_fname(fname);
    if(file == NULL){
       file = filesys_open(fname);
       if(file == NULL) return; // file name does not exist
    }

    // get number of fragmentable files
    if (file != NULL && file->inode != NULL) {
      sector_count = bytes_to_sectors(file->inode->data.length);
      if(sector_count > 1){
        fragmentable_file_count++;
        is_fragmentable = true;
      }  
    }
    
    // get number of fragmented files
    if(is_fragmentable){   
      // direct blocks
      is_fragmented = file_is_fragmented(file->inode->data.direct_blocks);
      if(is_fragmented){
        done_check = true;
        fragmented_file_count++;
      } 

      // indirect blocks
      if(!done_check && sector_count > DIRECT_BLOCKS_COUNT){
        // get indirect blocks
        indirect_block_sector = calloc(1, sizeof(struct inode_indirect_block_sector));
        buffer_cache_read(file->inode->data.indirect_block, indirect_block_sector);
        is_fragmented = file_is_fragmented(indirect_block_sector->blocks);
        if(is_fragmented){
          done_check = true;
          fragmented_file_count++;
        }

        free(indirect_block_sector);
      }

      // doubly indirect blocks
      if(!done_check && sector_count > INDIRECT_BLOCKS_PER_SECTOR * DIRECT_BLOCKS_COUNT){
        for(int i = 0; i < INDIRECT_BLOCKS_PER_SECTOR; i++){
          // get doubly indirect blocks
          doubly_indirect_block_sector = calloc(1, sizeof(struct inode_indirect_block_sector)); 
          buffer_cache_read(file->inode->data.doubly_indirect_block, indirect_block_sector);
          buffer_cache_read(indirect_block_sector->blocks[i], doubly_indirect_block_sector);

          is_fragmented = file_is_fragmented(doubly_indirect_block_sector->blocks);
          if(is_fragmented){
            done_check = true;
            fragmented_file_count++;
            free(doubly_indirect_block_sector);
            break;
          }

          free(doubly_indirect_block_sector);
        }
      }
    }  
  }

  dir_close(dir);

  printf("fragmentable file: %d\n", fragmentable_file_count);
  printf("fragmented file: %d\n", fragmented_file_count);

  printf("fragmentation degree: %f\n", (float)fragmented_file_count / (float)fragmentable_file_count);
  return;
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

int min(int source_file_size, int free_space) {
  return (source_file_size < free_space) ? source_file_size : free_space;
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

bool file_is_fragmented(block_sector_t blocks[DIRECT_BLOCKS_COUNT]){
  int blockIndex = 0;
  int sectorGap;

  // traverse through blocks
  while(blockIndex < DIRECT_BLOCKS_COUNT){
    if(blocks[blockIndex] != 0){
      sectorGap = blocks[blockIndex + 1] - blocks[blockIndex];
      if(sectorGap > 3){
        printf("b1: %d, b2: %d\n", blocks[blockIndex], blocks[blockIndex + 1]);
        return true;
      } 
    }
    blockIndex++;
  }
  return false;
}