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
int copy_out_defragment(char *fname);
//defragment helper functions
int get_size_of_files_on_disk(int *file_count);
bool is_sector_free(int num_sector);
void reorganize_file(block_sector_t ** disk_sectors_buffer, int *file_size_buffer, struct file **file_buffer, char **file_name_buffer, int file_count, int total_sector_count);
void reorganize_file_sector_number(block_sector_t ** disk_sectors_buffer, int *file_size_buffer, struct file **file_buffer, int file_count);
void create_recovered_filename(char *buffer, int bufferSize, int flag, int sectorOrFileName);
struct inode_indirect_block_sector {
  block_sector_t blocks[INDIRECT_BLOCKS_PER_SECTOR];
};

//----------------------------------------------SEPARATION----------------LINE-------------------------------------//
// Copy the file on real hard dive into the shell's hard drive with the same name in the root directory
int copy_in(char *fname) {
  FILE* source_file;
  long source_file_size;
  long free_space;
  char buf[BUFFER_SIZE];  // chunk size
  size_t bytes_read = 0;
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

  // create copied new file on shell's hard drive; give size = file size on real hard drive
  if(!fsutil_create(fname, source_file_size)){
    fclose(source_file);
    return handle_error(FILE_CREATION_ERROR);
  }

  // if(!fsutil_create(fname, BLOCK_SECTOR_SIZE)){
  //   fclose(source_file);
  //   return handle_error(FILE_CREATION_ERROR);
  // }

  // Since problems are observed for writing large files; we write data into fs in chunks
  // Keep reading until EOF reached; 1 byte (char) each time
  while ((bytes_read = fread(buf, 1, BUFFER_SIZE-2, source_file)) > 0) {
    buf[bytes_read] = '\0';
    if (free_space <= 0) break;

    // there is space but not sufficient to copy in every byte
    if (bytes_read > free_space) bytes_read = free_space;

    // Write the chunk; +1 for null terminator
    // int byte_write = fsutil_write(fname, buf, bytes_read + 2);
    // struct file *file = get_file_by_fname(fname);
    // printf("file size: %d\n", file->inode->data.length);

    if (fsutil_write(fname, buf, bytes_read + 1) == -1) {
      fclose(source_file);
      return handle_error(FILE_WRITE_ERROR);  // something wrong happened :(
    }

    // everything seems fine: accumulate the written bytes, update free space left
    total_written += bytes_read;
    free_space -= bytes_read;
  }

  fclose(source_file);

  // If not all data could be written, print a warning
  if (free_space <= 0 && total_written < source_file_size) {
    printf("Warning: could only write %ld out of %ld bytes (reached end of disk space)\n", total_written, source_file_size);
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
  if (shell_disk_file_size < 0) return handle_error(FILE_READ_ERROR); //2

  content_buffer = (char *)malloc((shell_disk_file_size) * sizeof(char)); // "wb" write-byte for fopen() doesn't need the "+1"
  if (content_buffer == NULL) return handle_error(FILE_READ_ERROR); 

  read = fsutil_read_at(fname, content_buffer, shell_disk_file_size, 0); // read from file offset 0
  if(read == -1) {
    free(content_buffer); 
    return handle_error(FILE_READ_ERROR);
  }

  //write to file on real hard drive
  real_disk_file = fopen(fname, "ab"); 
  if (real_disk_file == NULL) {
    free(content_buffer);
    return handle_error(FILE_CREATION_ERROR);
  }

  size_t written_bytes = fwrite(content_buffer, sizeof(char), strlen(content_buffer), real_disk_file);
  // Check if all data was written
  if (written_bytes <  strlen(content_buffer)) {   
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
  int sector_count = 0; // number of sectors that one file taken
  int fragmentable_file_count = 0;
  bool is_fragmentable = false;
  int fragmented_file_count = 0;

  dir = dir_open_root();

  // traverse the file system
  while(dir_readdir(dir, fname)){
    
    // get a file
    file = get_file_by_fname(fname);
    if(file == NULL){
       file = filesys_open(fname);
       if(file == NULL) return; // file name does not exist
    }

    if (file != NULL && file->inode != NULL) {
      sector_count = bytes_to_sectors(file->inode->data.length);

      // check if file is fragmentable
      if(sector_count > 1){
        fragmentable_file_count++;
        is_fragmentable = true;
      }  

      // get number of fragmented files
      if(is_fragmentable){ 
        is_fragmentable = false; 
        if(file_is_fragmented(get_inode_data_sectors(file->inode), sector_count)) fragmented_file_count++;
      }
    }
  }

  dir_close(dir);

  printf("Num fragmentable files: %d\n", fragmentable_file_count);
  printf("Num fragmented files: %d\n", fragmented_file_count);
  printf("Fragmentation pct: %f\n", (float)fragmented_file_count / (float)fragmentable_file_count);
  return;
  }

//----------------------------------------------SEPARATION----------------LINE-------------------------------------//

int defragment() {
  struct dir *dir; 
  struct file *file;
  char cwd[1024];
  int file_count = 0;
  int file_index = 0;
  char fname[NAME_MAX + 1];
  
  getcwd(cwd, 1024);
  system("mkdir ./temp_file_storage");
  chdir("./temp_file_storage");

  // allocate space for buffers
  get_size_of_files_on_disk(&file_count);
  char file_name_buffer[file_count][NAME_MAX+1];

  dir = dir_open_root();
  // traverse the file system
  while(dir_readdir(dir, fname)){ 

    file = get_file_by_fname(fname);
    if(file == NULL){
      file = filesys_open(fname);
      if(file == NULL) return -1; // file name does not exist
    }

    if (file != NULL && file->inode != NULL) {
      copy_out_defragment(fname);

      // add file names to file_name_buffer
      if(file_index < file_count){
        strcpy(file_name_buffer[file_index], fname);
        file_index++;
      }

      // remove all files
      remove_from_file_table(fname);
      dir_remove(dir, fname);
    }
  }
  dir_close(dir);


  // copy_in files; remove copied_out files on real disk
  chdir("./temp_file_storage");
  for(int i  = 0; i < file_count; i++){
    memset(fname, 0, sizeof(fname));
    strcpy(fname, file_name_buffer[i]);
    copy_in(fname);
  } 

  chdir(cwd);
  system("rm -rf ./temp_file_storage");
  return 0;
}

//----------------------------------------------SEPARATION----------------LINE-------------------------------------//
void recover(int flag) {
  char filenameBuffer[100]; // recovered file name
  bool recovery_performed = false; // Track if recovery was performed
  
  if (flag == 0) { // Recover deleted inodes
    struct block* hd = block_get_hd();
    if (hd == NULL) {
      printf("ERROR: Hard drive access failed.\n");
      return;
    }
    char* buf = malloc(BLOCK_SECTOR_SIZE);
    if (!buf) {
      printf("Memory allocation failed in recover.\n");
      return;
    }
    struct dir *root = dir_open_root();
    // Start checking from sector 2; 0 for free map, 1 for root dir; ; -1 because last sector left for partition? (not sure, without -1 there is Kernel PANIC)
    for (int sector = 2; sector < block_size(hd)-1; sector++) {
      block_read(hd, sector, buf);    // read sector content
      struct inode_disk* potential_inode = (struct inode_disk*) buf;  // cast to inode_disk type
      if (potential_inode->magic == INODE_MAGIC) {
        // Found a sector with inode data, now verify it's not part of active directories
        if (!is_inode_referenced_in_directory(root, sector)) {
          // printf("sector: %d\n", sector);
          // Inode is not referenced; Recover the inode by creating a new directory entry
          // Creating a file is copied from filesys_create()
          create_recovered_filename(filenameBuffer, sizeof(filenameBuffer), flag, sector);
          struct inode *recovered_inode = inode_open(sector);
          if (recovered_inode == NULL) {
            printf("ERROR: Could not open inode at sector %d.\n",sector);
            continue;
          }
          // if (!free_map_recover_inode_and_blocks(recovered_inode)) {
          //   printf("ERROR: Failed to recover inode and its blocks at sector %d.\n", sector);
          //   inode_close(recovered_inode);
          //   continue;
          // }
          inode_close(recovered_inode);
          // Add the recovered file to the directory
          if (!dir_add(root, filenameBuffer, sector, potential_inode->is_dir)) {
            printf("ERROR: Could not add recovered file '%s' to root directory.\n", filenameBuffer);
            continue;
          }
          printf("Recovered inode in sector %d\n", sector);
          recovery_performed = true;
        }
      }
    }
    dir_close(root);
    free(buf);
  } else if (flag == 1) {
      // Recover all non-empty sectors
  } else if (flag == 2) {
      // Recover data past end of file
  }

  if (!recovery_performed) {
    printf("No recovery is performed.\n");
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
  int sector_gap = 0;

  // traverse through blocks
  while(block_index + 1 < sector_count){
    if(blocks[block_index] != 0 && blocks[block_index+1] != 0){
      sector_gap = blocks[block_index + 1] - blocks[block_index];
      if(sector_gap > 3 || sector_gap < -3) return true;
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
void reorganize_file(block_sector_t ** disk_sectors_buffer, int *file_size_buffer, struct file **file_buffer, char **file_name_buffer, int file_count, int total_sector_count){
  struct file *file;
  int file_size = 0; // by sector
  char* read_buffer;
  char* original_read_buffer;
  bool is_allocated = false;
  int *byte_read = 0;

  // for(int k = 0; k < total_sector_count; k++){
  //   printf("buffer: %d\n", *( disk_sectors_buffer[k]));
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
  reorganize_file_sector_number( disk_sectors_buffer, file_size_buffer, file_buffer, file_count);
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
void reorganize_file_sector_number(block_sector_t ** disk_sectors_buffer, int *file_size_buffer, struct file **file_buffer, int file_count){
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
        *( disk_sectors_buffer[j]) = file->inode->sector + 1;
        continue;
      }
      // printf("cur_file_start_index: %d, cur_file_end_index: %d\n", cur_file_start_index, cur_file_end_index);
      *( disk_sectors_buffer[j]) = *( disk_sectors_buffer[j-1]) + 1; 
    }
    pre_file_sector_count = cur_file_sector_count; 
    reorganize_first_sector = false;
  }

  return;
}

/* Helps recover() formulate the recovered filename */
void create_recovered_filename(char *buffer, int bufferSize, int flag, int sectorOrFileName) {
  switch (flag) {
      case 0: // recovered inode
          snprintf(buffer, bufferSize, "recovered0-%d", sectorOrFileName);
          break;
      case 1: // recovered data block
          snprintf(buffer, bufferSize, "recovered1-%d.txt", sectorOrFileName);
          break;
      case 2: // recovered hidden data
          // WARNING: Assuming sectorOrFileName is actually a pointer to a string (file name) in this case
          // snprintf(buffer, bufferSize, "recovered2-%s.txt", (char*)sectorOrFileName);
          break;
      default:
          printf("Invalid recovery type\n");
          buffer[0] = '\0'; // Set the buffer to an empty string to indicate error
  }
}

int copy_out_defragment(char *fname) {
  // Copy the file on shell's hard dive to real hard drive with the same name
  char* content_buffer;
  int shell_disk_file_size;
  int read;
  FILE* real_disk_file;

  // Read from to-be-copied file
  shell_disk_file_size = fsutil_size(fname); // get file size
  if (shell_disk_file_size < 0) return handle_error(FILE_READ_ERROR); //2

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

  size_t written_bytes = fwrite(content_buffer, sizeof(char), read, real_disk_file);
  // Check if all data was written
  if (written_bytes <  strlen(content_buffer)) {   
    fclose(real_disk_file);
    free(content_buffer);
    return handle_error(FILE_WRITE_ERROR);
  }
  
  fclose(real_disk_file);
  free(content_buffer);
  return 0;
}
