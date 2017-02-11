#ifndef CACHE_H
#define CACHE_H

#include "global.h"

#ifdef __cplusplus

#include <unordered_map>

struct CacheItem
{
    void* data;          //!< Pointer to the cached item
    Uint32 data_size;    //!< size of item
    Uint32 access_time;  //!< Last time used
    Uint32 access_count; //!< Number of usages since last checkpoint

    /*!
     * \brief Constructor
     *
     * Create a new cache item with \a size bytes of data starting at address
     * \a data.
     */
    CacheItem(void* data, Uint32 size):
        data(data), data_size(size), access_time(cur_time), access_count(1) {}

    /*!
     * \brief Update the last use time of this cache item
     *
     * Sets the time this cache item was accessed last to the current time
     */
    void use()
    {
        access_time = cur_time;
        ++access_count;
    }
    /*!
     * Update the data size
     *
     * Set the new size of the data in this item to \a new_size bytes.
     * \param new_size The new data size, in bytes
     */
    void set_data_size(Uint32 new_size)
    {
        data_size = new_size;
    }
    /*!
     * Adjust the data size
     *
     * Add \a diff bytes to the size of the data in this item.
     * \param diff The difference in data size.
     */
    void adjust_data_size(Sint32 diff)
    {
        data_size += diff;
    }
};

class Cache
{
public:
    //! Type definition for the function freeing cached data
    typedef void (*FreeHandler)(void*);
    //! Type definition for the function compacting cached data
    typedef Uint32 (*CompactHandler)(void*);

    /*!
     * \brief Constructor
     *
     * Create a new, empty, cache with name \a name.
     * \param name The name of the cache
     */
    Cache(const char* name);
    //! Destructor
    virtual ~Cache();

    //! Return the number of items in this cache
    Uint32 size() const { return _cache.size(); }

    /*!
     * \brief Add a new item to the cache
     *
     * Add a new item with \a size bytes of data in \a data to the cache
     * under the name \a name. If \a name is already in the cache, nothing
     * happens.
     * \a param name The name of the new item
     * \a param data The data of the item to add
     * \a param size The size of the data in bytes
     */
    bool add(const std::string& name, void* data, Uint32 size)
    {
        return _cache.emplace(name, CacheItem(data, size)).second;
    }
    /*!
     * \brief Remove an item from the cache
     *
     * Remove the item with name \a name from the cache
     * \param name The name of the item to remove
     */
    void remove(const std::string& name);

    /*!
     * \brief Clear the cache
     *
     * Clear the cache, removing all items.
     */
    void clear();

    /*!
     * \brief Update the last use time of a cache item
     *
     * Sets the time cache item \a name was accessed last to the current time
     *
     * \param name The name of the item in the cache
     */
    void use(const std::string& name)
    {
        auto iter = _cache.find(name);
        if (iter != _cache.end())
            iter->second.use();
    }
    /*!
     * Update the size of an item
     *
     * Set the new size of the data for item \a name to \a new_size bytes.
     *
     * \param name     The name of the item in the cache
     * \param new_size The new data size, in bytes
     */
    void set_data_size(const std::string& name, Uint32 new_size)
    {
        auto iter = _cache.find(name);
        if (iter != _cache.end())
            iter->second.set_data_size(new_size);
    }
    /*!
     * Adjust the data size
     *
     * Add \a diff bytes to the size of the data for item \a name.
     *
     * \param name The name of the item in the cache
     * \param diff The difference in data size.
     */
    void adjust_data_size(const std::string& name, Sint32 diff)
    {
        auto iter = _cache.find(name);
        if (iter != _cache.end())
            iter->second.adjust_data_size(diff);
    }

    /*!
     * \brief Look up an item in the cache.
     *
     * Look up the item with given name \a name in this cache, and return the
     * associated data.
     *
     * \param name    The name of the item to look for in \a cache.
     * \retval void*  A pointer to the data associated with the cache item
     */
    void *find(const std::string& name);

    /*!
     * \brief   sets the time limit for items in this cache.
     *
     * Sets the time limit for items in this cache to \a time_limit milliseconds.
     *
     * \param time_limit the maximum amount of time to live for items in this cache.
     */
    void set_time_limit(Uint32 time_limit) { _time_limit = time_limit; }
    /*!
     * \brief   sets the free handler for this cache
     *
     * Sets the routine used to free item data in this cache.
     *
     * \param free_item  Routine to use for freeing item data.
     */
    void set_free(FreeHandler free_item)
    {
        _free_item = free_item;
    }
    /*!
     * \ingroup cache
     * \brief   sets the compact handler for this cache
     *
     * Sets the routine used to compact items in this cache.
     *
     * \param compact_item  Routine to use when items in the cache get compacted.
     */
    void set_compact(CompactHandler compact_item)
    {
        _compact_item = compact_item;
    }

    /*!
     * Compact the items in the cache.
     *
     * Reduce memory use by compacting items in the cache. If \a unused_only
     * is \c true (the default), only items that have not been used within
     * this cache's timie limit are compacted. Otherwise all items are
     * compacted. This requires a compact handler to be set for this cache,
     * see \see set_compact.
     *
     * \param unused_only If \c true, only compact unused items.
     */
    Uint32 compact(bool unused_only=true);

    #ifdef	ELC
    /*!
     * \brief Print the sizes of this cache.
     *
     * Print the sizes of this cache to the console.
     */
    virtual void print_sizes() const;
#endif // ELC

private:
    friend class CacheSystem;

    //! Name of this cache
    const char* _name;
    //! The cache itself
    std::unordered_map<std::string, CacheItem> _cache;
    //! Last time LRU processing done
    Uint32 _LRU_time;
    //! limit on LRU time before forcing a scan
    Uint32 _time_limit;
    //!< routine to call to free an item
    FreeHandler _free_item;
    //!< routine to call to reduce memory usage without freeing
    CompactHandler _compact_item;

    //! Constructor for the cache system
    Cache();

    //! Remove unused items from the cache
    Uint32 clean_unused();
    //! Estimate of the total memory allocated for this cache, including the cached items
    Uint32 memory_size() const;
    //! Update the last time this cache was used
    void update_LRU() { _LRU_time = cur_time; }

};

class CacheSystem: public Cache
{
public:
    /*!
     * Constructor
     *
     * Create a new cache system
     */
    CacheSystem(): Cache() {}

    /*!
     * Periodic maintenance
     *
     * Maintenance function, to be called periodically. This checks all caches
     * in the system, and removes or compacts unused items.
     */
    Uint32 maintenance();

#ifdef	ELC
    /*!
     * \brief Print the sizes of this cache.
     *
     * Print the sizes of this cache to the console.
     */
    void print_sizes() const;
#endif // ELC
};

#else // __cplusplus

// C wrapper functions

typedef struct Cache Cache;

extern Cache *cache_e3d;
extern Cache *cache_system;

/*!
 * \ingroup cache
 * \brief   initializes a new cache
 *
 * Initializes a new cache with name \a name.
 *
 * \param name     The name of the new cache
 * \retval Cache*  A pointer to a newly created cache.
 * \callgraph
 */
Cache *ncache_init(const char* name);
/*!
 * \ingroup cache
 * \brief Deletes a cache
 *
 * Deletes a cache and releases the resources associated with it. If a
 * free handler was set for the cache, the item data will also be freed by
 * calling the handler on all items in the cache. Otherwise, it is up to
 * the caller to free the cached data itself.
 *
 * \param cache e cache to delete
 * \callgraph
 */
void ncache_delete(Cache *cache);

/*!
 * \ingroup cache
 * \brief   sets the time limit for items in \a cache.
 *
 * Sets the time limit to  \a time_limit for items in the given \a cache.
 *
 * \param cache         the cache for which the time limit should be set.
 * \param time_limit    the maximum amount of time to live for items in \a cache.
 */
void ncache_set_time_limit(Cache *cache, Uint32 time_limit);
/*!
 * \ingroup cache
 * \brief   sets the free handler for cache \a cache.
 *
 * Sets the routine used to free item data in \a cache.
 *
 * \param cache         The cache for which to set the free item handler.
 * \param free_item     Routine to use free freeing item data in \a cache.
 */
void ncache_set_free(Cache *cache, void (*free_item)());
/*!
 * \ingroup cache
 * \brief   sets the compact handler for cache \a cache.
 *
 * Sets the routine used to compact items in \a cache.
 *
 * \param cache         the cache for which to set the compact item handler.
 * \param compact_item  routine to use when items in \a cache get compacted.
 */
void ncache_set_compact(Cache *cache, Uint32 (*compact_item)());

/*!
 * \ingroup cache
 * \brief Add an item to a cache
 *
 * Adds the given item \a item of size \a size to cache \a cache with the given
 * name \a name.
 *
 * \param cache       The cache to which \a item gets added
 * \param name        The name to use for \a item in \a cache
 * \param item        Pointer to the item to add
 * \param size        Size of the data in \a item in bytes
 * \callgraph
 */
int ncache_add_item(Cache *cache, const char* name, void *item, Uint32 size);

/*!
 * \ingroup cache
 * \brief Update the last use time of a cache item
 *
 * Sets the time cache item \a name in cache \a cache was accessed last to the
 * current time.
 *
 * \param cache The cache in which the item is stored
 * \param name  The name of the item in \a cache
 */
void ncache_use(Cache *cache, const char* name);
/*!
 * \ingroup cache
 * \brief Update the data size of a cache item
 *
 * Sets the size of the data associated with cache item \a name in cache
 * \a cache to \a new_size bytes.
 *
 * \param cache    The cache in which the item is stored
 * \param name     The name of the item in \a cache
 * \param new_size The new size of the data in bytes.
 */
void ncache_set_size(Cache *cache, const char* name, Uint32 size);
/*!
 * \ingroup cache
 * \brief Adjust the data size of a cache item
 *
 * Add \a diff bytes to the size of the data associated with cache item \a name
 * in cache \a cache.
 *
 * \param cache The cache in which the item is stored
 * \param name  The name of the item in \a cache
 * \param diff  The difference in the size of the data in bytes.
 */
void ncache_adj_size(Cache *cache, const char* name, Sint32 size);

/*!
 * \ingroup cache
 * \brief Look up an item in a cache.
 *
 * Look up the item with given name \a name in \a cache, and return the
 * associated data.
 *
 * \param cache   The cache to search
 * \param name    The name of the item to look for in \a cache.
 * \retval void*  A pointer to the data associated with the cache item
 * \callgraph
 */
void *ncache_find_item(Cache *cache, const char* name);

/*!
 * \ingroup cache
 *
 * Compact all items in the cache using its compact handler.
 *
 * \param cache The cache in which to compact the items data.
 * \retval Uint32 the number of bytes of memory freed.
 */
Uint32 ncache_compact_all(Cache *cache);

#ifdef ELC
/*!
 * \ingroup cache
 * \brief Print the sizes of cache \a cache.
 *
 * Print the sizes of cache \a cache to the console.
 *
 * \param cache Cache to query for its size.
 *
 * \callgraph
 */
void ncache_dump_sizes(const Cache *cache);
#endif // ELC

/**
 * Initialize the cache system
 *
 * Initialize the cache system. This is a cache tyhat holds all other caches.
 */
void ncache_system_init();
/**
 * Do cache maintenance
 *
 * Do cache maintenance. This checks all registered caches for unused items,
 * and either compacts them or removes them from the cache altogether, depending
 * on whether a compact handler or a free handler for the cache is set.
 */
void ncache_system_maint();

#endif // __cplusplus

#endif // CACHE_H
