/*
 * cache.c
 */


#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "cache.h"
#include "debug.h"
#include "main.h"

/* cache configuration parameters */
static int cache_split = 0;
static int cache_usize = DEFAULT_CACHE_SIZE;
static int cache_isize = DEFAULT_CACHE_SIZE;
static int cache_dsize = DEFAULT_CACHE_SIZE;
static int cache_block_size = DEFAULT_CACHE_BLOCK_SIZE;
static int words_per_block = DEFAULT_CACHE_BLOCK_SIZE / WORD_SIZE;
static int cache_assoc = DEFAULT_CACHE_ASSOC;
static int cache_writeback = DEFAULT_CACHE_WRITEBACK;
static int cache_writealloc = DEFAULT_CACHE_WRITEALLOC;

/* cache model data structures */
static Pcache icache;
static Pcache dcache;
static Pcache ucache;
static cache c1;
static cache c2;
static cache_stat cache_stat_inst;
static cache_stat cache_stat_data;
static Pcache_line free_cache_lines;

/************************************************************/
void set_cache_param(int param, int value)
    {

    switch (param)
        {
        case CACHE_PARAM_BLOCK_SIZE:
            cache_block_size = value;
            words_per_block = value / WORD_SIZE;
            break;
        case CACHE_PARAM_USIZE:
            cache_split = FALSE;
            cache_usize = value;
            break;
        case CACHE_PARAM_ISIZE:
            cache_split = TRUE;
            cache_isize = value;
            break;
        case CACHE_PARAM_DSIZE:
            cache_split = TRUE;
            cache_dsize = value;
            break;
        case CACHE_PARAM_ASSOC:
            cache_assoc = value;
            break;
        case CACHE_PARAM_WRITEBACK:
            cache_writeback = TRUE;
            break;
        case CACHE_PARAM_WRITETHROUGH:
            cache_writeback = FALSE;
            break;
        case CACHE_PARAM_WRITEALLOC:
            cache_writealloc = TRUE;
            break;
        case CACHE_PARAM_NOWRITEALLOC:
            cache_writealloc = FALSE;
            break;
        default:
            dbg_printf(MODULE_CACHE, 
                "error set_cache_param: bad parameter value\n");
            exit(-1);
        }

    }
/************************************************************/

/************************************************************/
void init_cache()
    {
    int set;
    
    /* initialize the cache, and cache statistics data structures */

    if (cache_split == FALSE) /* Unified cache */
        {
        ucache = dcache = icache = &c1;

        ucache->size = cache_usize;
        ucache->associativity = cache_assoc;
        ucache->n_sets = (cache_usize * cache_assoc) / cache_block_size;

        /*
         * If you have computed the index mask and index mask offset 
         * fields properly, then you should be able to compute the 
         * index into the cache for an address, addr, in the following
         * way: 
         *
         * index = (addr & ucache->index_mask) >> ucache->index_mask_offset
         *
         * Then, check the cache line at this index, ucache->LRU_head[index]. 
         * If the cache line has a tag that matches the address tag, you have
         * a cache hit. Otherwise, you have a cache miss.
         */
         
        ucache->index_mask_offset = LOG2(cache_block_size);
        ucache->index_mask = ((ucache->n_sets - 1) << ucache->index_mask_offset);
        
        
        dbg_printf(MODULE_CACHE, 
            "index mask @%p offset %d\n", ucache->index_mask, ucache->index_mask_offset);

        ucache->tag_mask_offset = LOG2(cache_usize);
        ucache->tag_mask = (0xFFFFFFFF << (ucache->tag_mask_offset));
        
        dbg_printf(MODULE_CACHE, 
            "tag mask @%p offset %d\n", ucache->tag_mask, ucache->tag_mask_offset);

        /*
         * The LRU head array is the data structure that keeps track 
         * of all the cache lines in the cache: each element in this 
         * array is a pointer to a cache line that occupies that set, 
         * or a NULL pointer if there is no valid cache line in the 
         * set (initially, all elements in the array should be set to NULL).
         * The cache line itself is kept in the cache line data structure, 
         * also declared in cache.h.
         *
         * Your simulator, however, should never allow the number of
         * cache lines in each linked list to exceed the degree of 
         * associativity configured in the cache. To enforce this, 
         * allocate an array of integers to the set contents field in
         * the cache data structure, one integer per set. Use these 
         * integers as counters to count the number of cache lines in
         * each set, and make sure that none of the counters ever
         * exceeds the associativity of the cache.
         */
        
        ucache->LRU_head = (Pcache_line *)malloc(sizeof(Pcache_line) * ucache->n_sets);
        ucache->LRU_tail = (Pcache_line *)malloc(sizeof(Pcache_line) * ucache->n_sets);
        ucache->set_contents = (int *)malloc(sizeof(int) * ucache->n_sets);

        if ((ucache->LRU_head == NULL) || 
            (ucache->LRU_tail == NULL) ||
            (ucache->set_contents == NULL))
            {
            dbg_printf(MODULE_CACHE, 
                "error allocating cache array\n");
            exit(-1);
            }
        
        /* Initially, all elements in the array should be set to NULL */
        
        for (set = 0; set < ucache->n_sets; set++)
            {
            ucache->LRU_head[set] = NULL;
            ucache->LRU_tail[set] = NULL;
            ucache->set_contents[set]= 0;
            }

        ucache->contents = 0;

        free_cache_lines = NULL;
        }
    else                      /* Split cache */
        {
        dcache = &c1;
        icache = &c2;
        }
    }

/************************************************************/
Pcache_line get_free_cache_line(void)
    {
    Pcache_line head = free_cache_lines;
    
    if (head != NULL)
        {
        free_cache_lines = head->LRU_next;
        }
    else
        {
        head = (Pcache_line)malloc(sizeof(cache_line));

        if (head == NULL)
            return NULL;
        }
    
    memset((void*)head, 0, sizeof(cache_line));

    return head;
    }

/************************************************************/
void perform_access(cpu_addr_t addr, unsigned access_type)
    {
    int index;
    Pcache_line LRU_head;
    Pcache_line LRU_curr;
    cpu_addr_t tag = (addr & ucache->tag_mask); 

    if (access_type == TRACE_INST_LOAD)
        {
        cache_stat_inst.accesses++;
        
        dbg_printf(MODULE_CACHE, 
            "inst read @%p tag @%p ", addr, tag);
        }
    else /* TRACE_DATA_LOAD || TRACE_DATA_STORE */
        {
        cache_stat_data.accesses++;

        if (access_type == TRACE_DATA_LOAD)
            dbg_printf(MODULE_CACHE, 
            "data read @%p tag @%p ", addr, tag);
        else
            dbg_printf(MODULE_CACHE, 
            "data write @%p tag @%p ", addr, tag);
        }

    /* handle an access to the cache */

    if (cache_split == FALSE) /* Unified cache */
        {
        index = (addr & ucache->index_mask) >> ucache->index_mask_offset;
        if (index >= ucache->n_sets)
            {
            dbg_printf(MODULE_CACHE, 
                "skipping access (%d), index(%d) too big!\n", 
                    access_type, index);
            
            return;
            }

        dbg_printf(MODULE_CACHE, 
            "cache index @%d ", index);

        LRU_head = LRU_curr = ucache->LRU_head[index];
            
        while (LRU_curr != NULL)
            {
            if (LRU_curr->tag == tag)
                {
                LRU_curr->hits++;

                if (access_type == TRACE_DATA_STORE)
                    {
                    LRU_curr->dirty = TRUE;
                    }

                dbg_printf(MODULE_CACHE, 
                    "hits @%d \n", LRU_curr->hits);

                return;
                }

            LRU_curr = LRU_curr->LRU_next;
            }

        if (access_type == TRACE_INST_LOAD)
            {
            cache_stat_inst.misses++;
            cache_stat_inst.demand_fetches += words_per_block;
            }
        else /* TRACE_DATA_LOAD || TRACE_DATA_STORE */
            {
            cache_stat_data.misses++;
            cache_stat_data.demand_fetches += words_per_block;
            }

        if (ucache->set_contents[index] == ucache->associativity)
            {
            /* If it is dirty, copy it back */
            
            if (LRU_head->dirty == TRUE)
                cache_stat_data.copies_back += words_per_block;

            /* Replace this cache line with a new tag */
            
            LRU_head->tag = tag;
            
            if (access_type == TRACE_DATA_STORE)
                {
                LRU_head->dirty = TRUE;
                }

            if (access_type == TRACE_INST_LOAD)
                {
                cache_stat_inst.replacements++;

                dbg_printf(MODULE_CACHE, 
                    "miss @%d \n", cache_stat_inst.replacements);
                }
            else /* TRACE_DATA_LOAD || TRACE_DATA_STORE */
                {
                cache_stat_data.replacements++;
                dbg_printf(MODULE_CACHE, 
                    "miss @%d \n", cache_stat_data.replacements);
                }

            return;
            }

        /* Allocate a new cache line */
        
        LRU_curr = get_free_cache_line();

        if (LRU_curr != NULL)
            {
            insert(&ucache->LRU_head[index], 
                   &ucache->LRU_tail[index], 
                   LRU_curr);
            
            ucache->set_contents[index]++;
            
            LRU_curr->tag = tag;
            LRU_curr->valid = TRUE;
            
            if (access_type == TRACE_DATA_STORE)
                {
                LRU_curr->dirty = TRUE;
                }

            dbg_printf(MODULE_CACHE, 
                "new @%d tag @%p\n", ucache->set_contents[index], LRU_curr->tag);
            }
        else
            {
            dbg_printf(MODULE_CACHE, 
                "cat not get cache line for access (%d), index(%d)!\n", 
                    access_type, index);
            exit(-1);
            }
        }
    else                      /* Split cache */
        {
        }
    }
/************************************************************/

/************************************************************/
void flush()
    {
    int index;
    Pcache_line LRU_curr;

    /* flush the cache */

    if (cache_split == FALSE) /* Unified cache */
        {
        for (index = 0; index < ucache->n_sets; index++)
            {
            LRU_curr = ucache->LRU_head[index];
                
            while (LRU_curr != NULL)
                {
                if (LRU_curr->dirty == TRUE)
                    {
                    cache_stat_data.copies_back += words_per_block;

                    LRU_curr->valid = FALSE;
                    LRU_curr->dirty = FALSE;
                    
                    return;
                    }

                LRU_curr = LRU_curr->LRU_next;
                }
            }     
        }
    else                      /* Split cache */
        {
        }
    }
/************************************************************/

/************************************************************/
void delete(Pcache_line * head, Pcache_line * tail, Pcache_line item)
    {
    if (item->LRU_prev)
        {
        item->LRU_prev->LRU_next = item->LRU_next;
        }
    else
        {
        /* item at head */
        *head = item->LRU_next;
        }

    if (item->LRU_next)
        {
        item->LRU_next->LRU_prev = item->LRU_prev;
        }
    else
        {
        /* item at tail */
        *tail = item->LRU_prev;
        }
    }
/************************************************************/

/************************************************************/
/* inserts at the head of the list */
void insert(Pcache_line * head, Pcache_line * tail, Pcache_line item)
    {
    item->LRU_next = *head;
    item->LRU_prev = (Pcache_line)NULL;

    if (item->LRU_next)
        item->LRU_next->LRU_prev = item;
    else
        *tail = item;

    *head = item;
    }
/************************************************************/

/************************************************************/
void dump_settings()
    {
    printf("*** CACHE SETTINGS ***\n");
    if (cache_split)
        {
        printf("  Split I- D-cache\n");
        printf("  I-cache size: \t%d\n", cache_isize);
        printf("  D-cache size: \t%d\n", cache_dsize);
        }
    else
        {
        printf("  Unified I- D-cache\n");
        printf("  Size: \t%d\n", cache_usize);
        }
    printf("  Associativity: \t%d\n", cache_assoc);
    printf("  Block size: \t%d\n", cache_block_size);
    printf("  Write policy: \t%s\n",
           cache_writeback ? "WRITE BACK" : "WRITE THROUGH");
    printf("  Allocation policy: \t%s\n",
           cache_writealloc ? "WRITE ALLOCATE" : "WRITE NO ALLOCATE");
    }
/************************************************************/

/************************************************************/
void print_stats()
    {
    printf("\n*** CACHE STATISTICS ***\n");

    printf(" INSTRUCTIONS\n");
    printf("  accesses:  %d\n", cache_stat_inst.accesses);
    printf("  misses:    %d\n", cache_stat_inst.misses);
    if (!cache_stat_inst.accesses)
        printf("  miss rate: 0 (0)\n");
    else
        printf("  miss rate: %2.4f (hit rate %2.4f)\n",
               (float)cache_stat_inst.misses / (float)cache_stat_inst.accesses,
               1.0 - (float)cache_stat_inst.misses / (float)cache_stat_inst.accesses);
    printf("  replace:   %d\n", cache_stat_inst.replacements);

    printf(" DATA\n");
    printf("  accesses:  %d\n", cache_stat_data.accesses);
    printf("  misses:    %d\n", cache_stat_data.misses);
    if (!cache_stat_data.accesses)
        printf("  miss rate: 0 (0)\n");
    else
        printf("  miss rate: %2.4f (hit rate %2.4f)\n",
               (float)cache_stat_data.misses / (float)cache_stat_data.accesses,
               1.0 - (float)cache_stat_data.misses / (float)cache_stat_data.accesses);
    printf("  replace:   %d\n", cache_stat_data.replacements);

    printf(" TRAFFIC (in words)\n");
    printf("  demand fetch:  %d\n", cache_stat_inst.demand_fetches +
           cache_stat_data.demand_fetches);
    printf("  copies back:   %d\n", cache_stat_inst.copies_back +
           cache_stat_data.copies_back);
    }
/************************************************************/
