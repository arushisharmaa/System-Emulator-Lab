/**************************************************************************
  * C S 429 system emulator
 * 
 * cache.c - A cache simulator that can replay traces from Valgrind
 *     and output statistics such as number of hits, misses, and
 *     evictions, both dirty and clean.  The replacement policy is LRU. 
 *     The cache is a writeback cache. 
 * 
 * Copyright (c) 2021, 2023. 
 * Authors: M. Hinton, Z. Leeper.
 * All rights reserved.
 * May not be used, modified, or copied without permission.
 **************************************************************************/ 
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include "cache.h"

#define ADDRESS_LENGTH 64

/* Counters used to record cache statistics in printSummary().
   test-cache uses these numbers to verify correctness of the cache. */

//Increment when a miss occurs
int miss_count = 0;

//Increment when a hit occurs
int hit_count = 0;

//Increment when a dirty eviction occurs
int dirty_eviction_count = 0;

//Increment when a clean eviction occurs
int clean_eviction_count = 0;

/* STUDENT TO-DO: add more globals, structs, macros if necessary */
uword_t next_lru;

// log base 2 of a number.
// Useful for getting certain cache parameters
static size_t _log(size_t x) {
  size_t result = 0;
  while(x>>=1)  {
    result++;
  }
  return result;
}

/*
 * Initialize the cache according to specified arguments
 * Called by cache-runner so do not modify the function signature
 *
 * The code provided here shows you how to initialize a cache structure
 * defined above. It's not complete and feel free to modify/add code.
 */
cache_t *create_cache(int A_in, int B_in, int C_in, int d_in) {
    /* see cache-runner for the meaning of each argument */
    cache_t *cache = malloc(sizeof(cache_t));
    cache->A = A_in;
    cache->B = B_in;
    cache->C = C_in;
    cache->d = d_in;
    unsigned int S = cache->C / (cache->A * cache->B);

    cache->sets = (cache_set_t*) calloc(S, sizeof(cache_set_t));
    for (unsigned int i = 0; i < S; i++){
        cache->sets[i].lines = (cache_line_t*) calloc(cache->A, sizeof(cache_line_t));
        for (unsigned int j = 0; j < cache->A; j++){
            cache->sets[i].lines[j].valid = 0;
            cache->sets[i].lines[j].tag   = 0;
            cache->sets[i].lines[j].lru   = 0;
            cache->sets[i].lines[j].dirty = 0;
            cache->sets[i].lines[j].data  = calloc(cache->B, sizeof(byte_t));
        }
    }

    // make gcc happy, replace this with zero and comment out _log if you want
    next_lru = _log(0);
    return cache;
}

cache_t *create_checkpoint(cache_t *cache) {
    unsigned int S = (unsigned int) cache->C / (cache->A * cache->B);
    cache_t *copy_cache = malloc(sizeof(cache_t));
    memcpy(copy_cache, cache, sizeof(cache_t));
    copy_cache->sets = (cache_set_t*) calloc(S, sizeof(cache_set_t));
    for (unsigned int i = 0; i < S; i++) {
        copy_cache->sets[i].lines = (cache_line_t*) calloc(cache->A, sizeof(cache_line_t));
        for (unsigned int j = 0; j < cache->A; j++) {
            memcpy(&copy_cache->sets[i].lines[j], &cache->sets[i].lines[j], sizeof(cache_line_t));
            copy_cache->sets[i].lines[j].data = calloc(cache->B, sizeof(byte_t));
            memcpy(copy_cache->sets[i].lines[j].data, cache->sets[i].lines[j].data, sizeof(byte_t));
        }
    }
    
    return copy_cache;
}

void display_set(cache_t *cache, unsigned int set_index) {
    unsigned int S = (unsigned int) cache->C / (cache->A * cache->B);
    if (set_index < S) {
        cache_set_t *set = &cache->sets[set_index];
        for (unsigned int i = 0; i < cache->A; i++) {
            printf ("Valid: %d Tag: %llx Lru: %lld Dirty: %d\n", set->lines[i].valid, 
                set->lines[i].tag, set->lines[i].lru, set->lines[i].dirty);
        }
    } else {
        printf ("Invalid Set %d. 0 <= Set < %d\n", set_index, S);
    }
}

/*
 * Free allocated memory. Feel free to modify it
 */
void free_cache(cache_t *cache) {
    unsigned int S = (unsigned int) cache->C / (cache->A * cache->B);
    for (unsigned int i = 0; i < S; i++){
        for (unsigned int j = 0; j < cache->A; j++) {
            free(cache->sets[i].lines[j].data);
        }
        free(cache->sets[i].lines);
    }
    free(cache->sets);
    free(cache);
}

/* STUDENT TO-DO:
 * Get the line for address contained in the cache
 * On hit, return the cache line holding the address
 * On miss, returns NULL
 */
// Define a function named "get_line" that takes in a pointer to a cache and an address
cache_line_t *get_line(cache_t *cache, uword_t addr) {
    // Calculate the number of bits in a block offset
    size_t bSize = _log(cache->B); 
    // Calculate the number of bits in a set index
    size_t sSize = _log(cache->C / (cache->A * cache->B)); 
    // Extract the set index from the address using a bitmask
    uword_t setIndex = (addr >> bSize) & ((1 << sSize) -1); 
    // Extract the tag value from the address using a bitmask
    uword_t tag = (addr >> (bSize+sSize)) & ((1 << (ADDRESS_LENGTH - sSize - bSize)) -1); 
     // Iterate over each line in the set
    for(int i = 0; i < cache->A; i++){ 
         // Check if the tag value matches and the line is valid
        if(cache->sets[setIndex].lines[i].tag == tag && cache->sets[setIndex].lines[i].valid) { 
             // Return a pointer to the matching line
            return &(cache->sets[setIndex].lines[i]); 
        }
    }
    // If no matching line is found, return NULL
    return NULL; 
}

/* STUDENT TO-DO:
 * Select the line to fill with the new cache line
 * Return the cache line selected to filled in by addr
 */
// Define a function named "select_line" that takes in a pointer to a cache and an address
cache_line_t *select_line(cache_t *cache, uword_t addr) {
    // Calculate the number of bits in a block offset
    size_t bSize = _log(cache->B); 
    // Calculate the number of bits in a set index
    size_t sSize = _log(cache->C / (cache->A * cache->B)); 
    // Extract the set index from the address using a bitmask
    uword_t setIndex = (addr >> bSize) & ((1 << sSize) -1); 
    // Initialize current least-recently-used value to be the maximum possible value
    uword_t currLRU = 0xfffffffffffff;
    // Create a pointer to the cache line that will be replaced
    cache_line_t *lineToReplace; 
    // Iterate over each line in the set
    for(int i = 0; i < cache->A; i++){ 
        // Otherwise, find the least-recently-used line
        if(!cache->sets[setIndex].lines[i].valid) { 
            return &(cache->sets[setIndex].lines[i]); 
        }
        // If there is an invalid line, return a pointer to it
        if(cache->sets[setIndex].lines[i].lru < currLRU) { 
            lineToReplace = &(cache->sets[setIndex].lines[i]); 
            currLRU = cache->sets[setIndex].lines[i].lru; 
        }
    }
     // Return a pointer to the least-recently-used line
    return lineToReplace; 
}

/*  STUDENT TO-DO:
 *  Check if the address is hit in the cache, updating hit and miss data.
 *  Return true if pos hits in the cache.
 */
// Define a function named "check_hit" that takes in a pointer to a cache, an address, and an operati
bool check_hit(cache_t *cache, uword_t addr, operation_t operation) {
     // Get a pointer to the cache line containing the address
    cache_line_t *cacheLine = get_line(cache, addr); 
     // If the cache line exists
    if(cacheLine != NULL) { 
        //increment the counter
        hit_count++; 
        if(operation == WRITE) { 
            //make sure to set it to dirty if the operation is WRITE
            cacheLine ->dirty = 1; 
        }
        // Increment the least-recently-used value and update the cache line's LRU value
        next_lru++; 
        cacheLine ->lru = next_lru;
        //return true indicating a hit 
        return true; 
    }
    else{ 
        // If the cache line does not exist, increment the miss count and return false, indicating a miss
        miss_count++; 
        return false; 
    }
}

/*  STUDENT TO-DO:
 *  Handles Misses, evicting from the cache if necessary.
 *  Fill out the evicted_line_t struct with info regarding the evicted line.
 */
evicted_line_t *handle_miss(cache_t *cache, uword_t addr, operation_t operation, byte_t *incoming_data)
{
    evicted_line_t *evicted_line = malloc(sizeof(evicted_line_t));
    evicted_line->data = (byte_t *)calloc(cache->B, sizeof(byte_t));
    /* your implementation */
    // Calculate the number of bits required for the block offset and set index
    size_t bSize = _log(cache->B);
    size_t sSize = _log(cache->C / (cache->A * cache->B));

    // Extract tag value from the address
    uword_t tagVal = (addr >> (bSize + sSize)) & ((1 << (ADDRESS_LENGTH - sSize - bSize)) - 1);

    // Select a cache line for eviction or replacement
    cache_line_t *selectedLine = select_line(cache, addr);

    // Save evicted line data and metadata in the provided pointer
    evicted_line->addr = addr;
    memcpy(evicted_line->data, selectedLine->data, 8);
    evicted_line->dirty = selectedLine->dirty;
    evicted_line->valid = selectedLine->valid;

    // If incoming data is provided, update selected line's data with incoming data
    if (incoming_data != NULL)
    {
        memcpy(selectedLine->data, incoming_data, 8);
    }

    // Update selected line's metadata and LRU count
    selectedLine->dirty = 0;
    next_lru++;
    selectedLine->lru = next_lru;
    selectedLine->tag = tagVal;
    selectedLine->valid = 1;

    // If the operation is a write, set the selected line's dirty bit
    if (operation == WRITE)
    {
        selectedLine->dirty = 1;        
    }

    // Check if the evicted line was clean or dirty and update respective counters
    if (evicted_line->valid)
    {
        if (evicted_line->dirty)
        {
            dirty_eviction_count++;
        }
        else
        {
            clean_eviction_count++;
        }
    }

    // Return NULL since the evicted line has already been saved in evicted_line pointer
    return NULL;
}
/* STUDENT TO-DO:
 * Get 8 bytes from the cache and write it to dest.
 * Preconditon: addr is contained within the cache.
 */
void get_word_cache(cache_t *cache, uword_t addr, word_t *dest) {
    /* Your implementation */
    unsigned int block_size = cache -> B;
    byte_t *res = (byte_t*) dest;
    cache_line_t *selected_line = get_line(cache, addr);
    for (int i = 0; i < 8; i++) {
        unsigned int offset = (addr + i) % block_size;
        res[i] = selected_line -> data[offset];
    }
}

/* STUDENT TO-DO:
 * Set 8 bytes in the cache to val at pos.
 * Preconditon: addr is contained within the cache.
 */
void set_word_cache(cache_t *cache, uword_t addr, word_t val) {
    /* Your implementation */
    unsigned int block_size = cache -> B;
    byte_t *val_byte = (byte_t*) &val;
    cache_line_t *selected_line = get_line(cache, addr);
    for (int i = 0; i < 8; i++) {
        unsigned int offset = (addr + i) % block_size;
        selected_line -> data[offset] = val_byte[i];
    }
}
/*
 * Access data at memory address addr
 * If it is already in cache, increase hit_count
 * If it is not in cache, bring it in cache, increase miss count
 * Also increase eviction_count if a line is evicted
 *
 * Called by cache-runner; no need to modify it if you implement
 * check_hit() and handle_miss()
 */
void access_data(cache_t *cache, uword_t addr, operation_t operation)
{
    if(!check_hit(cache, addr, operation))
        free(handle_miss(cache, addr, operation, NULL));
}