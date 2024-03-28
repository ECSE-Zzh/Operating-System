#include "free-map.h"
#include "bitmap.h"
#include "debug.h"
#include "file.h"
#include "filesys.h"
#include "inode.h"
#include <stdio.h>
#include <stdlib.h>
static struct file *free_map_file; /* Free map file. */
struct bitmap *free_map;           /* Free map, one bit per sector. */

/* Initializes the free map. */
void free_map_init(void) {
  free_map = bitmap_create(block_size(fs_device) - 1);

  if (free_map == NULL)
    PANIC("bitmap creation failed--file system device is too large");
  bitmap_mark(free_map, FREE_MAP_SECTOR);
  bitmap_mark(free_map, ROOT_DIR_SECTOR);
}

int num_free_sectors(void) {
  return bitmap_count(free_map, 0, bitmap_size(free_map), 0);
}

/* Allocates CNT consecutive sectors from the free map and stores
   the first into *SECTORP.
   Returns true if successful, false if not enough consecutive
   sectors were available or if the free_map file could not be
   written. */
bool free_map_allocate(size_t cnt, block_sector_t *sectorp) {
  block_sector_t sector = bitmap_scan_and_flip(free_map, 0, cnt, false);
  if (sector != BITMAP_ERROR && free_map_file != NULL &&
      !bitmap_write(free_map, free_map_file)) {
    bitmap_set_multiple(free_map, sector, cnt, false);
    sector = BITMAP_ERROR;
  }
  if (sector != BITMAP_ERROR)
    *sectorp = sector;
  return sector != BITMAP_ERROR;
}

/* Makes CNT sectors starting at SECTOR available for use. */
void free_map_release(block_sector_t sector, size_t cnt) {
  ASSERT(bitmap_all(free_map, sector, cnt));
  bitmap_set_multiple(free_map, sector, cnt, false);
  bitmap_write(free_map, free_map_file);
}

/* Opens the free map file and reads it from disk. */
void free_map_open(void) {
  free_map_file = file_open(inode_open(FREE_MAP_SECTOR));
  if (free_map_file == NULL)
    PANIC("can't open free map");
  if (!bitmap_read(free_map, free_map_file))
    PANIC("can't read free map");
}

/* Writes the free map to disk and closes the free map file. */
void free_map_close(void) { file_close(free_map_file); }

/* Creates a new free map file on disk and writes the free map to
   it. */
void free_map_create(void) {
  /* Create inode. */
  if (!inode_create(FREE_MAP_SECTOR, bitmap_file_size(free_map), false))
    PANIC("free map creation failed");

  /* Write bitmap to file. */
  struct inode *free_map_inode = inode_open(FREE_MAP_SECTOR);

  free_map_file = file_open(free_map_inode);
  if (free_map_file == NULL)
    PANIC("can't open free map");
  if (!bitmap_write(free_map, free_map_file))
    PANIC("can't write free map");
}

/*User defined*/
/* DEPRECATED: Mark a sector at sector as allocated in free bit map; used in the context of file recovery; prevent overwriting */
// bool free_map_recover(block_sector_t sector) {
//   bool success = true;
//   if (!bitmap_test(free_map, sector)) {
//     bitmap_set(free_map, sector, true);
//     success = bitmap_write(free_map, free_map_file);
//   }
//   return success;
// }

/* Recover the inode sector but also all data sectors associated with it */
bool free_map_recover_inode_and_blocks(struct inode *inode){
  bool success = true;

  // First, recover the inode sector itself
  block_sector_t inode_sector = inode_get_inumber(inode);
  if (!bitmap_test(free_map, inode_sector)) {
      bitmap_set(free_map, inode_sector, true);
      if (!bitmap_write(free_map, free_map_file)) {
          return false; // Failure to write the bitmap
      }
  }

  // // Next, recover all data sectors associated with the inode
  // block_sector_t *data_sectors = get_inode_data_sectors(inode);
  // size_t num_sectors = bytes_to_sectors(inode_length(inode));
  // for (size_t i = 0; i < num_sectors; i++) {
  //     if (!bitmap_test(free_map, data_sectors[i])) {
  //         bitmap_set(free_map, data_sectors[i], true);
  //     }
  // }
  // if (!bitmap_write(free_map, free_map_file)) {
  //     success = false; // Failure to write the bitmap
  // }

  // free(data_sectors); // free the malloc
  return success;
}