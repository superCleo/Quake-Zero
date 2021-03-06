#include <stdio.h>

#include "q_platform.h"
#include "q_common.h"

//=================================
// String related operations
//=================================

/*
 * str is null terminated string
 */
int StringLength(const char *str)
{
    int length = 0;

    while (*str != '\0')
    {
        length++;
        str++;
    }
    return length;
}

/*
 * dest: points to the memory that will store the string, and will always be 
 *       appended a char of '\0'
 * destSize: is the total memory size in byte, including '\0'
 * src: is null terminated string
 * count: is the amount of characters to copy, not including '\0'
 *        if count is zero, try to copy all characters from src
 * return: The amount of characters copied, not including '\0'. The actual 
 *         amount of characters copied might not be "count", if '\0' * is met 
 *         ealier or destSize - 1 is smaller than count. Otherwise return the 
 *         amount of characters copied
 */
int StringCopy(char *dest, int destSize, const char *src, int count = 0)
{
    ASSERT(destSize > 0);
    ASSERT(count >= 0);

    int amount = 0;

    if (count == 0)
    {
        count = destSize - 1;
    }
    
    for (;;)
    {
        if (destSize == 1 || amount == count)
        {
            *dest = '\0';
            return amount;
        }

        *dest = *src;
        amount++;
       
        if (*dest == '\0') 
        {
            return amount - 1;
        }

        destSize--;

        dest++;
        src++;
    }
}

/*
 * lhs, rhs: are null terminated strings
 * count: number of characters to compare. 
 * return: 0 is strings are equal 
 */
int StringNCompare(const char *lhs, const char *rhs, int count)
{
    ASSERT(count >= 0);

    for (int i = 0; i < count; ++i)
    {
        if (lhs[i] != rhs[i])
        {
            return lhs[i] - rhs[i];
        }
        else if (lhs[i] == '\0')
        {
            return 0;
        }
    }

    return 0;
}

int StringCompare(const char *lhs, const char *rhs)
{
    for (;;)
    {
        if(*lhs != *rhs)
        {
            return -1;
        }
        else if (*lhs == '\0')
        {
            return 0;
        }

        lhs++;
        rhs++;
    }
}

void CatString(char *src0, int src0Count, 
               char *src1, int src1Count,
               char *dest, int destCount)
{
    int destIndex = 0;

    for (int i = 0; i < src0Count; ++i)
    {
        if (destIndex == destCount - 1 || src0[i] == '\0')
        {
            dest[destIndex] = '\0';
            return ;
        }
        else 
        {
            dest[destIndex++] = src0[i];
        }
    }

    for (int i = 0; i < src1Count; ++i)
    {
        if (destIndex == destCount - 1 || src1[i] == '\0')
        {
            dest[destIndex] = '\0';
            return ;
        }
        else
        {
            dest[destIndex++] = src1[i];
        }
    }

	dest[destIndex] = '\0';
}

/*
 * str is null terminated string
 */
void IntToString(int number, char *str, int size)
{
    int sign = 1;
    int index = 0;

    if (number < 0)
    {
        sign = -1;
        str[index++] = '-';
        number *= -1;
    }

    int digit;
    
    for (;;)
    {
        digit = number % 10;
        str[index++] = (char)digit + '0';
        number /= 10;

        if (index >= size - 1 || number == 0)
        {
            str[index] = '\0';
            break ;
        }
    }

    // rotate digits
    int start_i = sign == 1 ? 0 : 1; 
    int end_i = index - 1;
    while (start_i < end_i)
    {
        char tmp = str[start_i];
        str[start_i] = str[end_i];
        str[end_i] = tmp;

        start_i++;
        end_i--;
    }
}

/*
 * str is null terminated string
 * only handle decimal number for the moment
 */
int StringToInt(char *str)
{
    int sign = 1;
    int number = 0;
    int index = 0;

    char c = str[index];
    if (c == '-')
    {
        sign = -1;
        index++;
    }
    else if (c == '+')
    {
        index++;
    }

    for (;;)
    {
        c = str[index++];
        if (c == '0' && number == 0)
        {
            continue;
        }
        else if (c >= '0' && c <= '9')
        {
            number *= 10;
            number += c - '0'; 
        }
        else
        {
            break ;
        }
    }

    number *= sign;

    return number;
}

//==========================
// Memory Operations
//==========================

void MemSet(void *dest, U8 value, int count)
{
    // if dest is 4-byte aligned and count is multiple of 4
    if ((((size_t)dest | count) & 3) == 0)
    {
        int fill = value;
        fill = fill | (fill << 8) | (fill << 16) | (fill << 24);
        count = count >> 2; // divided by 4
        for (int i = 0; i < count; ++i)
        {
            ((U32 *)dest)[i] = fill;
        }
    }
    else
    {
        for (int i = 0; i < count; ++i)
        {
            ((U8 *)dest)[i] = value;
        }
    }
}

void MemCpy(void *dest, void *src, int count)
{
    // if both dest and src are 4-byte aligned and count if multiple of 4
    if ((((size_t)dest | (size_t)src | count) & 3) == 0)
    {
        count = count >> 2;
        for (int i = 0; i < count; ++i)
        {
            ((U32 *)dest)[i] = ((U32 *)src)[i];
        }
    }
    else
    {
        for (int i = 0; i < count; ++i)
        {
            ((U8 *)dest)[i] = ((U8 *)src)[i];
        }
    }
}

//=================================
// Memory Management
//=================================

/*
 * Zone Memory
 *
 * Zone memory serves as heap memory, is mainly used for small, dynamic 
 * allocations like strings. All big objects are allocated on Hunk.
 *
 * Zone memory is consisted of memory blocks, free or being used. There won't be 
 * any 2 consecutive free memory blocks
 *
 * Memory blocks are 8-byte aligned.
 */

#define DYNAMIC_ZONE_SIZE 128 * 1024
#define ZONE_ID 0x1d4a11
#define MIN_FRAGMENT 64

struct MemoryBlock
{
    // including sizeof(MemoryBlock)
    I32 size; 
    // a tag of 0 is a free block
    I32 tag;
    // memory guard, should always be ZONE_ID
    I32 id;

    /*
     * If padding was at the end of this struct, the size of this struct would
     * have been 40 instead of 32. The reason is self-alignment for typed data 
     * except char. For example, by default 2-byte short will sit on memory 
     * address that's multiple of 2, 4-byte int on memory address multiple of 4, 
     * and pointer on x64 machine on memory address multiple of 8. 
     * Self-alignment facilitates memory access.
     * TODO lw: figure out why self-alignment helps memory access
     * */
    int padding;

    MemoryBlock *next, *prev;
};

struct MemoryZone
{
    // points to a free memory block most of time, could potentially speed up
    // searching of free memory block
    MemoryBlock *rover;
    // the same as g_cache_head, 
    // serves as reference node in the circular linked list
    MemoryBlock tailhead; 
    // total bytes allocated including sizoef(MemoryZone)
    I32 size;
    // structure needs to be aligned to the size of the largest primitive type 
    // data in the structure, in this case it's be 8-byte pointer "rover"
    I32 padding;
};

MemoryZone *g_main_zone;

void ZoneCheckHeap()
{
    MemoryBlock *block = g_main_zone->tailhead.next;

    while (block->next != &g_main_zone->tailhead)
    {
        if ((U8 *)block + block->size != (U8 *)block->next)
        {
            g_platformAPI.SysError("ZoneCheckHeap: block size is erroneous");
        }

        if (block->next->prev != block)
        {
            g_platformAPI.SysError("ZoneCheckHeap: memory block linked list is broken");
        }

        if (block->tag == 0 && block->next->tag == 0)
        {
            g_platformAPI.SysError("2 consecutive free memory blocks!");
        }

        block = block->next;
    }
}

// set all memory blocks as one free block
void ZoneClearAll(MemoryZone *zone)
{
    zone->tailhead.size = 0; // so it won't store any actual data
    zone->tailhead.tag = 1; // not a free block, a reference node
    zone->tailhead.id = ZONE_ID;

    zone->rover = (MemoryBlock *)((U8 *)zone + sizeof(*zone));
    zone->rover->size = zone->size - sizeof(*zone);
    zone->rover->tag = 0;
    zone->rover->id = ZONE_ID;
    zone->rover->next = &zone->tailhead;
    zone->rover->prev = &zone->tailhead;

    zone->tailhead.next = zone->rover;
    zone->tailhead.prev = zone->rover;
}

void ZoneFree(void *ptr)
{
    if (!ptr)
    {
        g_platformAPI.SysError("ZoneFree: free NULL pointer");
    }

    MemoryBlock *block = (MemoryBlock *)((U8 *)ptr - sizeof(MemoryBlock));

    if (block->id != ZONE_ID)
    {
        g_platformAPI.SysError("ZoneFree: free memory block without zone id");
    }

    if (block->tag == 0)
    {
        g_platformAPI.SysError("ZoneFree: free a free memory block");
    }

    block->tag = 0;

    MemoryBlock *other = block->prev;
    if (other->tag == 0)
    {
        // merge with previous free block
        other->next = block->next;
        block->next->prev = other;
        other->size = other->size + block->size;
        
        if (g_main_zone->rover == block)
        {
            g_main_zone->rover = other;
        }

        block = other;
    }

    other = block->next;
    if (other->tag == 0)
    {
        // merge with next free block
        block->next = other->next;
        other->prev = block;
        block->size = block->size + other->size;

        if (g_main_zone->rover == other)
        {
            g_main_zone->rover = block;
        }
    }
}

void *ZoneTagMalloc(int size, int tag)
{
    if (tag == 0)
    {
        g_platformAPI.SysError("ZoneTagAlloc: using a 0 tag");
    }

    size += sizeof(MemoryBlock);
    size += 4; // space at the end of memory block for trash tester
    size = (size + 7) & ~7;

    MemoryBlock *candidate = g_main_zone->rover;

    // Walk through all memory blocks and try to find one that's free and have 
    // enough space.
    for (;;)
    {
        if (candidate == g_main_zone->rover->prev)
        {
            return NULL; // scanned all memory block, couldn't find one
        }

        if (candidate->tag == 0 && candidate->size >= size)
        {
            break ;
        }
        else
        {
            candidate = candidate->next;
        }
    }

    // if the extra space is large that MIN_FRAGMENT, merge it back into free 
    // memory block
    int extra = candidate->size - size;
    if (extra > MIN_FRAGMENT)
    {
        MemoryBlock *newBlock = (MemoryBlock *)((U8 *)candidate + size);
        newBlock->size = extra;
        newBlock->tag = 0;
        newBlock->id = ZONE_ID;
        
        // insert new block into linked list
        candidate->next->prev = newBlock;
        newBlock->next = candidate->next;
        candidate->next = newBlock;
        newBlock->prev = candidate;

        candidate->size = size;
    }

    candidate->tag = tag;
    candidate->id = ZONE_ID;

    // next allocation will start looking here
    g_main_zone->rover = candidate->next;

    // marker for memory trash testing
    // TODO lw: ????
    *(int *)((U8 *)candidate + (candidate->size - 4)) = ZONE_ID;

    void *result = (U8 *)candidate + sizeof(*candidate);

    return result;
}

void *ZoneMalloc(int size)
{
#ifdef QUAKEREMAKE_SLOW
    ZoneCheckHeap();
#endif

    void *result = ZoneTagMalloc(size, 1);

    if (!result)
    {
        g_platformAPI.SysError("ZoneMalloc: failed on allocation of size bytes");
    }

    // TODO lw: zero out result

    return result;
}



/* 
 * Hunk memory
 *
 * Hunk memory is the continuous memory block pre-allocated for the entire game. 
 * Memory can be allocated at both end in stack fashion.  
 * Hunk allocation is 16-byte aligned.
 *
 * Zone memory is allocated at the botton of the Hunk.
 *
 * Cache Memory is allocated inbetween low hunk and high hunk.
 *
 */

#define	HUNK_SENTINEL	0x1df001ed

struct HunkHeader
{
    int sentinel;
    int size; // including sizeof(HunkHeader)
    char name[16]; // at most 15 characters, and a '\0'
};

U8 *g_hunk_base;
int g_hunk_total_size;

int g_hunk_low_used;
int g_hunk_high_used;
int g_hunk_temp_used;
bool g_hunk_temp_active;

inline int Align16(int v)
{
    int result = (v + 15) & ~15;
    return result;
}

inline int Align8(int v)
{
    int result = (v + 7) & ~7;
    return result;
}

void *HunkLowAlloc(int size, char *name)
{
    if (size < 0)
    {
        g_platformAPI.SysError("HunkLowAlloc: negative size");
    }

    // TODO lw: find out what's the benefit of 16-byte alignment. cache line coherent?
    // align to 16 bytes
    size = Align16(size + sizeof(HunkHeader));

    if (g_hunk_total_size - g_hunk_low_used - g_hunk_high_used < size)
    {
        g_platformAPI.SysError("HunkLowAlloc: out of memory");
    }

    HunkHeader *hheader = (HunkHeader *)(g_hunk_base + g_hunk_low_used);
    g_hunk_low_used += size;

    // TODO lw: free cache memory if necessary

    MemSet(hheader, 0, size);

    hheader->sentinel = HUNK_SENTINEL;
    hheader->size = size;
    StringCopy(hheader->name, 16, name);

    return (void *)(hheader + 1);
}

void *HunkLowAlloc(int size)
{
    void *result = HunkLowAlloc(size, "unknown");
    return result;
}

void *HunkHighAlloc(int size, char *name)
{
    if (size < 0)
    {
        g_platformAPI.SysError("HunkHighAlloc: negative size");
    }

    // free temp hunk

    size = Align16(size + sizeof(HunkHeader));

    if (g_hunk_total_size - g_hunk_low_used - g_hunk_high_used < size)
    {
        g_platformAPI.SysError("HunkHighAlloc: out of memory");
    }

    g_hunk_high_used += size;
    HunkHeader *hh = (HunkHeader *)(g_hunk_base + g_hunk_total_size - g_hunk_high_used);

    // TODO lw: free cache memory if necessary

    MemSet(hh, 0, size);

    hh->sentinel = HUNK_SENTINEL;
    hh->size = size;
    StringCopy(hh->name, 16, name);

    return (void *)(hh + 1);
}

void HunkFreeTemp()
{
    g_hunk_temp_active = false;
    g_hunk_high_used -= g_hunk_temp_used;
    g_hunk_temp_used = 0;
}

// allocating on high stack, used when loading asset files from disk
void *HunkTempAlloc(int size)
{
    // the second temp allocation removes the first temp data
    if (g_hunk_temp_active)
    {
        HunkFreeTemp();
    }
    g_hunk_temp_active = true;
    int old_high_used = g_hunk_high_used;
    void *result = HunkHighAlloc(size, "temp");
    g_hunk_temp_used = g_hunk_high_used - old_high_used;
    return result;
}

void *HunkHighAlloc(int size)
{
    void *result = HunkHighAlloc(size, "unknown");
    return result;
}

/*
 * Cache Memory
 *
 * Cache memory is used for dynamic loading objects. Caches are allocated 
 * inbetween low stack and high stack, and can be removed if necessary for hunk 
 * allocation. A circular linked list is used, in which the 
 * g_cache_head->lru_prev is the Least Recent Used cache.
 *
 * Another circular linked list is used to keep cache pointers. Free memory is 
 * searched by looking from the top of low hunk and move linearly along the 
 * linked list. 
 *
 */

struct CacheHeader
{
    char name[16];

    CacheUser *user;

    CacheHeader *prev, *next;
    CacheHeader *lru_prev, *lru_next;

    int size;
    int padding;
};

// the g_cache_head doesn't store any real data, it serves as a reference point
// in the cache circular link.
CacheHeader g_cache_head;

void CacheUnlinkLRU(CacheHeader *ch)
{
    if (ch->lru_next == NULL || ch->lru_prev == NULL)
    {
        g_platformAPI.SysError("CacheUnlinkLRU: NULL link");
    }

    ch->lru_next->lru_prev = ch->lru_prev;
    ch->lru_prev->lru_next = ch->lru_next;
    ch->lru_next = NULL;
    ch->lru_prev = NULL;
}

// mark cache as most recent used
void CacheMarkMRU(CacheHeader *ch)
{
    if (ch->lru_next || ch->lru_prev)
    {
        CacheUnlinkLRU(ch);
    }

    g_cache_head.lru_next->lru_prev = ch;
    ch->lru_next = g_cache_head.lru_next;
    g_cache_head.lru_next = ch;
    ch->lru_prev = &g_cache_head;
}

// if the cache has been loaded move it to the top of LRU list
void *CacheCheck(CacheUser *user)
{
    if (user->data == NULL)
    {
        return NULL;
    }

    CacheHeader *ch = (CacheHeader *)user->data - 1;
    CacheUnlinkLRU(ch);
    CacheMarkMRU(ch);

    return user->data;
}

void CacheFree(CacheUser *cu)
{
    if (!cu->data)
    {
        g_platformAPI.SysError("CacheFree: not allocated");
    }

    CacheHeader *ch = (CacheHeader *)cu->data - 1;

    ch->next->prev = ch->prev;
    ch->prev->next = ch->next;
    ch->prev = NULL;
    ch->next = NULL;

    cu->data = NULL;

    CacheUnlinkLRU(ch);
}

void CacheFlushAll()
{
    CacheHeader *ch = g_cache_head.next;
    while(ch != &g_cache_head)
    {
        ch->user->data = NULL;
    }

    g_cache_head.next = &g_cache_head;
    g_cache_head.prev = &g_cache_head;
    g_cache_head.lru_next = &g_cache_head;
    g_cache_head.lru_prev = &g_cache_head;
}

/*
 * Search free memory hole from bottom. If found, insert it into cache linked 
 * list so that the order in the linked list is the same as cache blocks in 
 * memory.
 * */
CacheHeader *CacheTryAlloc(int size)
{
    CacheHeader *new_cache = NULL;
    CacheHeader *old_cache = NULL;

    // cache list is empty
    if (g_cache_head.next == &g_cache_head)
    {
        if (g_hunk_total_size - g_hunk_low_used - g_hunk_high_used < size)
        {
            g_platformAPI.SysError("CacheTryAlloc: size is greater than free hunk\n");
        }
        new_cache = (CacheHeader *)(g_hunk_base + g_hunk_low_used);
        MemSet(new_cache, 0, sizeof(*new_cache));
        new_cache->size = size;

        g_cache_head.next = new_cache;
        g_cache_head.prev = new_cache;
        new_cache->next = &g_cache_head;
        new_cache->prev = &g_cache_head;

        CacheMarkMRU(new_cache);

        return new_cache;
    }

    new_cache = (CacheHeader *)(g_hunk_base + g_hunk_low_used);
    old_cache = g_cache_head.next;

    // linearly go through linked list, try to find a hole inbetween caches
    do
    {
        if ((U8 *)old_cache - (U8 *)new_cache >= size)
        {
            MemSet(new_cache, 0, sizeof(*new_cache));
            new_cache->size = size;

            // insert new_cache before the old_cache in the linked list
            old_cache->prev->next = new_cache;
            new_cache->prev = old_cache->prev;
            new_cache->next = old_cache;
            old_cache->prev = new_cache;

            CacheMarkMRU(new_cache);

            return new_cache;
        }

        new_cache = (CacheHeader *)((U8 *)old_cache + old_cache->size);
        old_cache = old_cache->next;
    } while (old_cache != &g_cache_head);

    // no hole big enough, allocate at the end, between the last cache and high stack
    if (g_hunk_base + g_hunk_total_size - g_hunk_high_used - (U8 *)new_cache >= size)
    {
        MemSet(new_cache, 0, sizeof(*new_cache));
        new_cache->size = size;

        // insert new_cache to the end, between g_cache_head->prev and g_cache_head
        g_cache_head.prev->next = new_cache;
        new_cache->prev = g_cache_head.prev;
        new_cache->next = &g_cache_head;
        g_cache_head.prev = new_cache;

        CacheMarkMRU(new_cache);

        return new_cache;
    }

    return NULL;
}

void *CacheAlloc(CacheUser *cu, int size, char *name)
{
    if (cu->data)
    {
        g_platformAPI.SysError("CacheAlloc: already allocated");
    }

    if (size <= 0)
    {
        g_platformAPI.SysError("CacheAlloc: bad size");
    }

    CacheHeader *ch = NULL;

    size = Align16(size + sizeof(*ch));

    for (;;)
    {
        ch = CacheTryAlloc(size);

        if (ch)
        {
            ch->user = cu;
            ch->user->data = (void *)(ch + 1);
            StringCopy(ch->name, 15, name, 15);

            break ;
        }

        if (g_cache_head.next == &g_cache_head)
        {
            g_platformAPI.SysError("CacheAlloc: out of memory");
        }

        CacheFree(g_cache_head.lru_prev->user);
    }

    return ch->user->data;
}

void CacheInit()
{
    g_cache_head.next = &g_cache_head;
    g_cache_head.prev = &g_cache_head;
    g_cache_head.lru_next = &g_cache_head;
    g_cache_head.lru_prev = &g_cache_head;
    g_cache_head.size = 0;

    // TODO lw: add flushall command
}

void MemoryInit(void *buf, int size)
{
    g_hunk_base = (U8 *)buf;
    g_hunk_total_size = size;
    g_hunk_low_used = 0;
    g_hunk_high_used = 0;

    CacheInit();

    int zoneSize = DYNAMIC_ZONE_SIZE;
    // TODO lw: set zone size from command line
    g_main_zone = (MemoryZone *)HunkLowAlloc(zoneSize, "zone");
    g_main_zone->size = zoneSize;
    ZoneClearAll(g_main_zone);
}

//=================================
// Dynamic variable tracking
//=================================

#define MAX_CVARS 512
#define CVAR_HASH_MASK (MAX_CVARS - 1)

struct CvarSystem
{
    I32 count;
    I32 hash[MAX_CVARS];
    Cvar cvars[MAX_CVARS];
};

CvarSystem g_cvar_pool;

I32 GetCvarHashKey(const char *name)
{
    // credit: http://www.cse.yorku.ca/~oz/hash.html
    I32 hash = 5381;
    char *c = (char *)name;
    while (*c)
    {
        hash = (hash << 5) + hash + *c;
        c++;
    }
    I32 hash_key = hash & CVAR_HASH_MASK;
    if (hash_key == 0)
    {
        hash_key = 1;
    }
    return 22;
}

// if cvar is not found, create one with default value 0
Cvar *CvarGet(const char *name)
{
    Cvar *result = NULL;

    I32 hash_key = GetCvarHashKey(name);
    I32 hash_index = hash_key;
    while (g_cvar_pool.hash[hash_index])
    {
        if (g_cvar_pool.hash[hash_index] == hash_key)
        {
            if (StringCompare(g_cvar_pool.cvars[hash_index].name, name) == 0)
            {
                result = &g_cvar_pool.cvars[hash_index];
            }
        }
        hash_index = (hash_index++) & CVAR_HASH_MASK;
    } 

    if (!result)
    {   
        if (g_cvar_pool.count >= MAX_CVARS)
        {
            g_platformAPI.SysError("CvarGet: cvar count exceeds the maximum!");
        }
        g_cvar_pool.count++;
		g_cvar_pool.hash[hash_index] = hash_key;
        result = &g_cvar_pool.cvars[hash_index];

        I32 length = StringLength(name) + 1;
        result->name = (char *)ZoneMalloc(length);
        StringCopy(result->name, length, name);
    }
    
    return result;
}

Cvar *CvarSet(const char *name, float val)
{
    Cvar *result = CvarGet(name);
    result->val = val;
    return result;
}


//=================================
// File system
//=================================

#define MAX_FILE_HANDLES 10

FILE *g_file_handles[MAX_FILE_HANDLES];

SearchPath *g_search_path;

int FileGetAvailableHande()
{
    for (int i = 0; i < MAX_FILE_HANDLES; ++i)
    {
        if (!g_file_handles[i])
        {
            return i;
        }
    }

    g_platformAPI.SysError("out of file handle");

    return -1;
}

int FileLength(FILE *file)
{
    // backup of current position indicator
    int pos = ftell(file);
    
    // set position indicator to the end of the file
    fseek(file, 0, SEEK_END);

    // get number of bytes from beginning to position indicator
    int end = ftell(file);

    // set position indicator to its previous position
    fseek(file, pos, SEEK_SET);

    return end;
}

/*
 * Open file for read, and add file pointer to global file handle array.
 * 
 */
int FileOpenForRead(char *path, int *handle_out)
{
    int handle = FileGetAvailableHande();
    
    // open as binary file for read
    FILE *f = NULL;
    fopen_s(&f, path, "rb");

    int result = 0;

    if (f)
    {
        g_file_handles[handle] = f;
        *handle_out = handle;
        result = FileLength(f);
    }
    else
    {
        *handle_out = -1;
        result = -1;
    }

    return result;
}

void FileClose(int handle)
{
    // if it's a file in PAK, don't close it
    for (SearchPath *sp = g_search_path; sp != NULL; ++sp)
    {
        if (sp->pack != NULL && sp->pack->handle == handle)
        {
            return ;
        }
    }

    // TODO lw: close file such as config.cfg
}

inline size_t 
FileRead(int filehandle, void *dest, int count)
{
    ASSERT(count >= 0);
    size_t result = fread(dest, 1, count, g_file_handles[filehandle]);
    return result;
}

inline void 
FileSeek(int filehandle, int position)
{
    fseek(g_file_handles[filehandle], position, SEEK_SET);
}


// on disk
struct PackFileDisk
{
    char name[56];
    int filePosition;
    int fileLength;
};

struct PackHeaderDisk
{
    char magic[4];
    // byte offset of the pack file directories
    // All file directories are stored at the end of the pack file. It's easier
    // to append new files and directories without modifying the existing ones.
    int directoryOffset; 
    int directoryLength;
};

bool g_packModified = false;

char g_cacheDir[MAX_OS_PATH_LENGTH];
char g_gameDir[MAX_OS_PATH_LENGTH];

void GetFileNameFromPath(const char *path, char *dest, int destSize)
{
    const char *onePastLastSlash = path;
    const char *scan = path;
    for (; *scan; ++scan)
    {
        if (*scan == '\\' || *scan == '/')
        {
            onePastLastSlash = scan + 1;
        }
    }

    StringCopy(dest, destSize, onePastLastSlash, (int)(scan - onePastLastSlash));
}

int FileFind(const char *filepath, int *handle)
{
    if (*handle >= 0)
    {
        g_platformAPI.SysError("handle is already set");
    }

    SearchPath *searchPath = g_search_path;
    for ( ; searchPath != NULL; searchPath = searchPath->next)
    {
        // search in PAK file
        if (searchPath->pack != NULL)
        {
            PackHeader *pack = searchPath->pack;
            for (int i = 0; i < pack->numfiles; ++i)
            {
                if (StringCompare(pack->files[i].name, filepath) == 0)
                {
                    *handle = pack->handle;
                    FileSeek(*handle, pack->files[i].filePosition);
                    return pack->files[i].fileLength;
                }
            }
        }
        else
        {
            // TODO lw: files not in PAK such as config.cfg
        }
    }

    *handle = -1;
    return 0;
}

U8 *FileLoad(const char *filepath, ALLocType allocType)
{
    int handle = -1;
    int fileLength = FileFind(filepath, &handle);

    if (handle == -1)
    {
        return NULL;
    }

    char baseName[16];
    GetFileNameFromPath(filepath, baseName, 16);

    U8 *buffer = NULL;

    switch (allocType)
    {
        case ALLocType::LOWHUNK:
        {
            buffer = (U8 *)HunkLowAlloc(fileLength + 1, baseName);
        } break;

        case ALLocType::TEMPHUNK:
        {
            buffer = (U8 *)HunkTempAlloc(fileLength + 1);
        } break;

        case ALLocType::ZONE:
        {
            buffer = (U8 *)ZoneTagMalloc(fileLength + 1, 1);
        } break;

        case ALLocType::CACHE:
        {
            // buffer = (U8 *)CacheAlloc(loadcache, fileLength + 1, baseName);
        } break;

        case ALLocType::TEMPSTACK:
        {

        } break;

        default:
        {
            g_platformAPI.SysError("bad alloc type");
        }
    }

    if (buffer == NULL)
    {
        g_platformAPI.SysError("not enough space for %s", filepath);
    }
    // TODO lw: why allocate one extra byte and set it to 0???
    buffer[fileLength] = 0; 

    FileRead(handle, buffer, fileLength);
    FileClose(handle);

    return buffer;
}

U8 *FileLoadToLowHunk(const char *filepath)
{
    U8 *result = FileLoad(filepath, ALLocType::LOWHUNK);
    return result;
}
    
PackHeader *FileLoadPack(char *packpath)
{
    int packHandle = 0;

    if (FileOpenForRead(packpath, &packHandle) == -1)
    {
        return NULL;
    }

    PackHeaderDisk packHeaderDisk;
    FileRead(packHandle, (void *)&packHeaderDisk, sizeof(packHeaderDisk));

    if (packHeaderDisk.magic[0] != 'P' 
        || packHeaderDisk.magic[1] != 'A'
        || packHeaderDisk.magic[2] != 'C'
        || packHeaderDisk.magic[3] != 'K')
    {
        g_platformAPI.SysError("%s is not a packfile", packpath);
    }

    int packfileNum = packHeaderDisk.directoryLength / sizeof(PackFileDisk);

    if (packfileNum > MAX_FILES_IN_PACK)
    {
        g_platformAPI.SysError("%s has %i files, too many", packpath, packfileNum);
    }

    if (packfileNum != PAK0_FILE_NUM)
    {
        g_packModified = true;
    }

    PackFileDisk diskPackFiles[MAX_FILES_IN_PACK];

    FileSeek(packHandle, packHeaderDisk.directoryOffset);
    FileRead(packHandle, (void *)diskPackFiles, packHeaderDisk.directoryLength);

    PackFile *packFiles = (PackFile *)HunkLowAlloc(packfileNum * sizeof(PackFile), "packfiles");

    for (int i = 0; i < packfileNum; ++i)
    {
        StringCopy(packFiles[i].name, MAX_PACK_FILE_PATH, diskPackFiles[i].name, 55);
        packFiles[i].filePosition = diskPackFiles[i].filePosition;
        packFiles[i].fileLength = diskPackFiles[i].fileLength;
    }

    PackHeader *packHeader = (PackHeader *)HunkLowAlloc(sizeof(PackHeader), "packheader");
    //StringCopy(packHeader->filepath, MAX_OS_PATH_LENGTH, packHeaderDisk);
    packHeader->handle = packHandle;
    packHeader->numfiles = packfileNum;
    packHeader->files = packFiles;

    return packHeader;
}

/*
 * Add main game directory to the path search list
 * Add pack files as directory to the path search list
 */
void FileAddGameDiectory(char *dir)
{
    StringCopy(g_gameDir, MAX_OS_PATH_LENGTH, dir);

    SearchPath *search = (SearchPath *)HunkLowAlloc(sizeof(SearchPath), "searchpath");
    StringCopy(search->filename, MAX_OS_PATH_LENGTH, dir);
    // insert game directory in front of search path list
    search->next = g_search_path;
    g_search_path = search;

    char packfile[MAX_OS_PATH_LENGTH];
    PackHeader *pack;

    for (int i = 0; ; ++i)
    {
        // add any pack files in the format of pak[0-9].pak
        sprintf_s(packfile, MAX_OS_PATH_LENGTH, "%spak%i.pak", dir, i);
        pack = FileLoadPack(packfile);
        if (!pack)
        {
            break ;
        }

        search = (SearchPath *)HunkLowAlloc(sizeof(SearchPath), "packpath");
        search->pack = pack;
        // insert in front of search path list
        search->next = g_search_path;
        g_search_path = search;
    }
}

int FileSystemInit(char *assetDir)
{
    // If it's a little endian system, a 32-bit integer B0B1B2B3 is stored in 
    // memory from address x to x + 3 as B3 | B2 | B1 | B0
    // quake's pack files were created in little-endian system
    U8 endian[4] {1, 0, 0, 0};
    if (*(I32 *)endian != 1)
    {
        g_platformAPI.SysError("Not a Little Endian system");
    }

    FileAddGameDiectory(assetDir);
    return 0;
}

