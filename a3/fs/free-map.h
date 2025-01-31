#ifndef FILESYS_FREE_MAP_H
#define FILESYS_FREE_MAP_H

#include "block.h"
#include "inode.h"
#include <stdbool.h>
#include <stddef.h>

extern struct bitmap *free_map; /* Free map, one bit per sector. */
// extern struct inode *inode;

void free_map_init(void);
void free_map_read(void);
void free_map_create(void);
void free_map_open(void);
void free_map_close(void);

bool free_map_allocate(size_t, block_sector_t *);
void free_map_release(block_sector_t, size_t);

int num_free_sectors(void);
bool free_map_recover_inode_and_blocks(struct inode *inode);
// bool free_map_recover(block_sector_t sector);
#endif /* fs/free-map.h */
