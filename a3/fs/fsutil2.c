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

#define BUFFER_SIZE 4096

int fsutil_read_at(char *file_name, void *buffer, unsigned size, offset_t file_ofs); //copy_out helper function
bool file_is_fragmented(block_sector_t *blocks, int sector_count);
//defragment helper functions
int get_size_of_files_on_disk(int *file_count);
bool is_sector_free(int num_sector);
void reorganize_file(block_sector_t **files_sectors_buffer, int *file_size_buffer, struct file **file_buffer, char **file_name_buffer, int file_count, int total_sector_count);
void reorganize_file_sector_number(block_sector_t **files_sectors_buffer, int *file_size_buffer, struct file **file_buffer, int file_count);

struct inode_indirect_block_sector {
  block_sector_t blocks[INDIRECT_BLOCKS_PER_SECTOR];
};

//----------------------------------------------SEPARATION----------------LINE-------------------------------------//
// Copy the file on real hard dive into the shell's hard drive with the same name in the root directory
int copy_in(char *fname) {
  FILE* source_file;
  long source_file_size;
  long free_space;
  char buf[BUFFER_SIZE]; // chunk size
  size_t bytes_read;
  long total_written = 0;

  source_file = fopen(fname, "rb"); // Open the file in binary mode
  if(source_file == NULL) return handle_error(BAD_COMMAND);

  // determine total source file size
  fseek(source_file, 0, SEEK_END);
  source_file_size = ftell(source_file);
  rewind(source_file);

  // get free space (byte) on hard drive
  free_space = fsutil_freespace()*BLOCK_SECTOR_SIZE; 
  if (free_space <= 0) {
    fclose(source_file);
    return handle_error(FILE_CREATION_ERROR);
  }

 // create copied new file on shell's hard drive; give one block sector to it
  if(!fsutil_create(fname, BLOCK_SECTOR_SIZE)){
    fclose(source_file);
    return handle_error(FILE_CREATION_ERROR);
  }

  // Since problems are observed for writing large files; we write data into fs in chunks
  // Keep reading until EOF reached; 1 byte (char) each time
  while ((bytes_read = fread(buf, 1, sizeof(buf), source_file)) > 0) {
    if (free_space <= 0) break;

  // there is space but not sufficient to copy in every byte
    if (bytes_read > free_space) bytes_read = free_space;

  // Write the chunk
    if (fsutil_write(fname, buf, bytes_read) == -1) {
      fclose(source_file);
      return handle_error(FILE_WRITE_ERROR); // something wrong happened :(
    }

  // everything seems fine: accumulate the written bytes, update free space left
      total_written += bytes_read;
      free_space -= bytes_read;
  }

    fclose(source_file);

  // If not all data could be written, print a warning
  if (free_space <= 0 && total_written < source_file_size) {
    printf("Warning: could only write %ld out of %ld bytes (reached end of file)\n", total_written, source_file_size);
  }

  return 0;
}
//----------------------------------------------SEPARATION----------------LINE-------------------------------------//

int copy_out(char *fname) {
  // Copy the file on shell's hard dive to real hard drive with the same name
  char* content_buffer;
  int shell_disk_file_size;
  int read;
  FILE* real_disk_file;

  // Read from to-be-copied file
  shell_disk_file_size = fsutil_size(fname); // get file size
  if (shell_disk_file_size < 0) return handle_error(FILE_READ_ERROR);

  content_buffer = (char *)malloc((shell_disk_file_size) * sizeof(char)); // "wb" write-byte for fopen() doesn't need the "+1"
  if (content_buffer == NULL) return handle_error(FILE_READ_ERROR); 

  read = fsutil_read_at(fname, content_buffer, shell_disk_file_size, 0); // read from file offset 0
  if(read == -1) {
    free(content_buffer); 
    return handle_error(FILE_READ_ERROR);
  }

  //write to file on real hard drive
  real_disk_file = fopen(fname, "wb"); 
  if (real_disk_file == NULL) {
    free(content_buffer);
    return handle_error(FILE_CREATION_ERROR);
  }

  size_t written_bytes = fwrite(content_buffer, sizeof(char), shell_disk_file_size, real_disk_file);
  // Check if all data was written
  if (written_bytes < shell_disk_file_size) {   
    fclose(real_disk_file);
    free(content_buffer);
    return handle_error(FILE_WRITE_ERROR);
  }
  
  fclose(real_disk_file);
  free(content_buffer);
  return 0;
}

//----------------------------------------------SEPARATION----------------LINE-------------------------------------//

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

//----------------------------------------------SEPARATION----------------LINE-------------------------------------//

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
      // if(sector_count > DIRECT_BLOCKS_COUNT) sector_count++; // add one for boundary block
      // printf("size: %d\n", sector_count);
      if(sector_count > 1){
        fragmentable_file_count++;
        is_fragmentable = true;
      }  
    }

    // get number of fragmented files
    if(is_fragmentable){   
      // direct blocks
      if(sector_count < DIRECT_BLOCKS_COUNT){
        is_fragmented = file_is_fragmented(file->inode->data.direct_blocks, sector_count);
      } 
      else {
        // for(int i = 0; i < DIRECT_BLOCKS_COUNT; i++) printf("direct block: %d\n", file->inode->data.direct_blocks[i]);
        is_fragmented = file_is_fragmented(file->inode->data.direct_blocks, DIRECT_BLOCKS_COUNT);
      }
      if(is_fragmented){
        // printf("found at L0 name %s\n", fname);
        // printf("found at L0 size: %d\n", sector_count);
        done_check = true;
        fragmented_file_count++;
      } 

      // indirect blocks
      if(!done_check && sector_count > DIRECT_BLOCKS_COUNT){
        // get indirect blocks
        indirect_block_sector = calloc(1, sizeof(struct inode_indirect_block_sector));
        buffer_cache_read(file->inode->data.indirect_block, indirect_block_sector);
        // check if file is fragmented
        is_fragmented = file_is_fragmented(indirect_block_sector->blocks, sector_count-DIRECT_BLOCKS_COUNT);
        // for(int i = 0; i < sector_count - DIRECT_BLOCKS_COUNT; i++) printf("indirect block: %d\n", indirect_block_sector->blocks[i]);
        if(is_fragmented){
          // printf("found at L1 name %s\n", fname);
          // printf("found at L1 size: %d\n", sector_count);
          done_check = true;
          fragmented_file_count++;
        }
        free(indirect_block_sector);
      }

      // doubly indirect blocks
      if(!done_check && sector_count > INDIRECT_BLOCKS_PER_SECTOR + DIRECT_BLOCKS_COUNT){
        for(int i = 0; i < INDIRECT_BLOCKS_PER_SECTOR; i++){
          // get doubly indirect blocks
          doubly_indirect_block_sector = calloc(1, sizeof(struct inode_indirect_block_sector)); 
          buffer_cache_read(file->inode->data.doubly_indirect_block, indirect_block_sector);
          buffer_cache_read(indirect_block_sector->blocks[i], doubly_indirect_block_sector);
          // check if file is fragmented
          is_fragmented = file_is_fragmented(doubly_indirect_block_sector->blocks, sector_count-(INDIRECT_BLOCKS_PER_SECTOR+DIRECT_BLOCKS_COUNT));
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
  // printf("fragmentable file: %d\n", fragmentable_file_count);
  // printf("fragmented file: %d\n", fragmented_file_count);
  printf("fragmentation degree: %f\n", (float)fragmented_file_count / (float)fragmentable_file_count);
  return;
}

//----------------------------------------------SEPARATION----------------LINE-------------------------------------//

int defragment() {
  struct dir *dir;
  struct file *file; 
  char fname[NAME_MAX + 1];
  int file_count = 0;
  int *file_size_buffer; // store sector_count of each file
  int num_inode = 0;
  int total_sector_count = 0;
  int sector_count = 0;
  int sector_not_done = 0;
  int buffer_index_counter = 0;
  struct file **file_buffer; // store files
  block_sector_t **files_sectors_buffer; // store all non-empty data blocks
  struct inode_indirect_block_sector *indirect_block_sector;
  struct inode_indirect_block_sector *doubly_indirect_block_sector; 
  char **file_name_buffer; // store file names
  int *file_inode_buffer; // store inode of each file
  bool has_indirect_block = false;
  bool has_doubly_indirect_block_sector = false;

  total_sector_count = get_size_of_files_on_disk(&file_count);
  // printf("total: %d\n", total_sector_count);

  dir = dir_open_root();
  // traverse the file system
  while(dir_readdir(dir, fname)){
    // get a file
    file = get_file_by_fname(fname);
    if(file == NULL){
       file = filesys_open(fname);
       if(file == NULL) return -1; // file name does not exist
    }
  }

  // allocate space for buffers
  files_sectors_buffer = calloc(total_sector_count, sizeof(block_sector_t*));

  // copy all sectors of current file
  files_sectors_buffer = 

  // remove all files

  // reorganize files

  // copy back reorganized files to disk





  file_buffer = calloc(file_count, sizeof(struct file*));
  file_size_buffer = calloc(file_count, sizeof(int));
  file_name_buffer = calloc(file_count, sizeof(fname));
  file_inode_buffer = calloc(file_count, sizeof(int));

  dir = dir_open_root();
  // traverse the file system
  while(dir_readdir(dir, fname)){
    // get a file
    file = get_file_by_fname(fname);
    if(file == NULL){
       file = filesys_open(fname);
       if(file == NULL) return -1; // file name does not exist
    }

    if (file != NULL && file->inode != NULL){
      sector_count = bytes_to_sectors(file->inode->data.length); 
      // if(sector_count > DIRECT_BLOCKS_COUNT) sector_count++; // add one for boundary block

      sector_not_done = sector_count;
      

      has_indirect_block = false;
      has_doubly_indirect_block_sector = false;

      // release inode sector, load related file information into buffers
      if(num_inode != file_count){
        file_buffer[num_inode] = file;
        file_size_buffer[num_inode] = sector_count;
        file_name_buffer[num_inode] = fname;
        file_inode_buffer[num_inode] = file->inode->sector;
        if(!is_sector_free(file->inode->sector)) free_map_release(file->inode->sector, 1);
        num_inode++;
      } 

      // get non-empty direct blocks (include the boundary block: block saves pointes point to indirect block)  
      for(int i = 0; i <= DIRECT_BLOCKS_COUNT; i++){
        if(file->inode->data.direct_blocks[i] != 0){
          files_sectors_buffer[buffer_index_counter] = &(file->inode->data.direct_blocks[i]);
          // printf("direct block: %d\n", file->inode->data.direct_blocks[i]);
          if(!is_sector_free(file->inode->data.direct_blocks[i])) free_map_release(file->inode->data.direct_blocks[i], 1); // release data blocks for reuse
          buffer_index_counter++;
          sector_not_done--;
        }
      }
      // get non-empty indirect blocks
      if(sector_count > DIRECT_BLOCKS_COUNT && sector_not_done > 0){
        has_indirect_block = true;
        indirect_block_sector = calloc(1, sizeof(struct inode_indirect_block_sector));
        buffer_cache_read(file->inode->data.indirect_block, &indirect_block_sector);
        for(int i = 0; i < INDIRECT_BLOCKS_PER_SECTOR; i++){
          if(indirect_block_sector->blocks[i] != 0){
            files_sectors_buffer[buffer_index_counter] = &indirect_block_sector->blocks[i];
            if(!is_sector_free(indirect_block_sector->blocks[i])) free_map_release(indirect_block_sector->blocks[i], 1); // release indirect data blocks for reuse
            buffer_index_counter++;
            sector_not_done--;
          }
        }
        // free(indirect_block_sector);
      }

      // get non-empty doubly indirect block
      if(sector_count > DIRECT_BLOCKS_COUNT+INDIRECT_BLOCKS_PER_SECTOR && sector_not_done > 0){
        has_doubly_indirect_block_sector = true;
        for(int i = 0; i < INDIRECT_BLOCKS_PER_SECTOR; i++){
          doubly_indirect_block_sector = calloc(1, sizeof(struct inode_indirect_block_sector)); 
          buffer_cache_read(file->inode->data.doubly_indirect_block, indirect_block_sector);
          buffer_cache_read(indirect_block_sector->blocks[i], doubly_indirect_block_sector);

          for(int j = 0; j < INDIRECT_BLOCKS_PER_SECTOR; j++){
            if(doubly_indirect_block_sector->blocks[j] != 0){
              files_sectors_buffer[buffer_index_counter] = &(doubly_indirect_block_sector->blocks[i]);
              if(!is_sector_free(doubly_indirect_block_sector->blocks[j])) free_map_release(doubly_indirect_block_sector->blocks[j], 1); // release doubly-indirect data blocks for reuse
              buffer_index_counter++;
              sector_not_done--;
            }
          }
          // free(doubly_indirect_block_sector);
        }
      }
    } 
  }
  // re-organize: re-locate fragmented files' sectors into contiguous sectors
  reorganize_file(files_sectors_buffer, file_size_buffer, file_buffer, file_name_buffer, file_count, total_sector_count);
  
  dir_close(dir);

  // free buffers
  free(files_sectors_buffer);
  free(file_buffer);
  free(file_size_buffer);
  free(file_inode_buffer);
  free(file_name_buffer);

  if(has_indirect_block) free(indirect_block_sector);
  if(has_doubly_indirect_block_sector){
    for(int i = 0; i < INDIRECT_BLOCKS_PER_SECTOR; i++) free(doubly_indirect_block_sector);
  } 
  return 0;
}
//----------------------------------------------SEPARATION----------------LINE-------------------------------------//

void recover(int flag) {
  if (flag == 0) { // recover deleted inodes

    free_map_open();

    size_t fm_size = bitmap_size(free_map);

    for (size_t i = 0; i < fm_size; i++) {
      // If sector i is free, check if it contains an inode that can be recovered
      if (bitmap_test(free_map, i)) { 
        
      }
    }

    free_map_close();  // Closes the free map

    /**
     * 1. Traverse all sectors in block
     * 2. Check if it's inode (through magic number)
     * 3. Check if deleted
     * 4. if deleted:
     *  4.1: create a file named 'recovered0-%d'
     *  4.2: link deleted inode to 'recovered0-%d'*/ 

         
  } else if (flag == 1) { // recover all non-empty sectors

    // TODO
  } else if (flag == 2) { // data past end of file.

    // TODO
  }
}

//----------------------------------------------HELPER----FUNCTIONS----START----HERE-------------------------------------//

int fsutil_read_at(char *file_name, void *buffer, unsigned size, offset_t file_ofs) {
  struct file *file_s = get_file_by_fname(file_name);
  if (file_s == NULL) {
    file_s = filesys_open(file_name);
    if (file_s == NULL) return -1;
    add_to_file_table(file_s, file_name);
  }
  return file_read_at(file_s, buffer, size, file_ofs);
}

bool file_is_fragmented(block_sector_t *blocks, int sector_count){
  int block_index = 0;
  int sector_gap;

  // traverse through blocks
  while(block_index < sector_count){
    if(blocks[block_index] != 0 && blocks[block_index+1] != 0){
      sector_gap = blocks[block_index + 1] - blocks[block_index];
      if(sector_gap > 3 || sector_gap < -3){
        // printf("gap: %d\n", sector_gap);
        return true;
      } 
    }
    block_index++;
  }
  return false;
}

// return the number of sectors taken by all files on current disk
int get_size_of_files_on_disk(int *file_count){
  struct dir *dir;
  struct file *file; 
  char fname[NAME_MAX + 1];
  int sector_count = 0;
  int total_sectors = 0;

  dir = dir_open_root();

  while(dir_readdir(dir, fname)){
    file = get_file_by_fname(fname);

    if(file == NULL){
       file = filesys_open(fname);
       if(file == NULL) return -1; // file name does not exist
    }

    if (file != NULL && file->inode != NULL){
      (*file_count)++;
      sector_count = bytes_to_sectors(file->inode->data.length); 

      // if(sector_count > DIRECT_BLOCKS_COUNT) sector_count++; // add one for boundary block

      total_sectors += sector_count; //get total number of files on hard drive
    }
  } 

  dir_close(dir);
  return total_sectors;
}

// check if sector has already been released or not
bool is_sector_free(int num_sector){
  bool is_free = false;
  if(bitmap_count(free_map, num_sector, 1, 0) == 1) is_free = true;
  return is_free;
}

// re-organize file: content
void reorganize_file(block_sector_t **files_sectors_buffer, int *file_size_buffer, struct file **file_buffer, char **file_name_buffer, int file_count, int total_sector_count){
  struct file *file;
  int file_size = 0; // by sector
  char* read_buffer;
  char* original_read_buffer;
  bool is_allocated = false;
  int *byte_read = 0;

  // for(int k = 0; k < total_sector_count; k++){
  //   printf("buffer: %d\n", *(files_sectors_buffer[k]));
  // }

  read_buffer = calloc(file_count, BLOCK_SECTOR_SIZE*total_sector_count);
  original_read_buffer = read_buffer; // save read_buffer starting point

  byte_read = calloc(file_count, sizeof(int));

  // read file contents before reallocate sectors        
  for(int i = 0; i < file_count; i++){
    file_size = file_size_buffer[i];
    file = file_buffer[i];

    // read each file and store to read_buffer before reset their blocks and inodes
    byte_read[i] = file_read_at(file, read_buffer, file->inode->data.length, 0);
    read_buffer += byte_read[i]; // avoid overwriting the previous file
  }

  // reallocate file sectors by changing #sector into contiguous
  reorganize_file_sector_number(files_sectors_buffer, file_size_buffer, file_buffer, file_count);
  // printf()

  read_buffer = original_read_buffer; // point read_buffer back to start

  // write content to updated contiguous #sector
  for(int i = 0; i < file_count; i++){
    file = file_buffer[i];
    file_size = file_size_buffer[i];
 
    // write back to files after reorganizing their inodes and blocks
    file_write_at(file, read_buffer, byte_read[i], 0);
    read_buffer += byte_read[i]; // discard the first i-th read bytes in buffer, avoid writing previous files repetitively

    // allocate space on free-map
    if(!is_allocated){
      free_map_allocate(file_size + 1, &(file->inode->sector));
      is_allocated = true;
    }
    is_allocated = false;
  }

  free(original_read_buffer);
  return;
}

// re-organize file: change scattered sector number ot contiguous number
void reorganize_file_sector_number(block_sector_t **files_sectors_buffer, int *file_size_buffer, struct file **file_buffer, int file_count){
  struct file *file;
  struct file *pre_file; 
  int cur_file_sector_count = 0;
  int pre_file_sector_count = 0;
  int cur_file_end_index = 0;
  int cur_file_start_index = 0;
  bool reorganize_first_sector = false;
  int j = 0;

  for(int i = 0; i < file_count; i++){
    file = file_buffer[i];
    cur_file_sector_count = file_size_buffer[i];

    //reorganize inode
    if(i == 0) {
      // first file, inode does not need to be updated  
      file->inode->sector = file->inode->sector;
    } else {
      pre_file = file_buffer[i-1];
      file->inode->sector = pre_file->inode->sector + pre_file_sector_count + 1; // file sector count exclude inode, hence need to plus 1
    }

    // get start index and end index of current file's sectors
    cur_file_start_index += pre_file_sector_count;
    cur_file_end_index = cur_file_sector_count + cur_file_start_index;

    for(j = cur_file_start_index; j < cur_file_end_index; j++){
      if(!reorganize_first_sector){
        reorganize_first_sector = true;
        *(files_sectors_buffer[j]) = file->inode->sector + 1;
        continue;
      }
      // printf("cur_file_start_index: %d, cur_file_end_index: %d\n", cur_file_start_index, cur_file_end_index);
      *(files_sectors_buffer[j]) = *(files_sectors_buffer[j-1]) + 1; 
    }
    pre_file_sector_count = cur_file_sector_count; 
    reorganize_first_sector = false;
  }

  return;
}
