#include <numeric>

#include "asc.h"
#include "elwindows.h"
#include "gamewin.h"
#include "ncache.h"
#include "translate.h"

CacheSystem *cache_system;
Cache *cache_e3d;

Cache::Cache(const char* name, FreeHandler free_item):
    _name(name),
    _cache(),
    _LRU_time(cur_time),
    _time_limit(0), // 0 == no time based LRU check
    _free_item(free_item),
    _compact_item(nullptr)
{
    cache_system->add(name, this, 0);
}

Cache::Cache():
    _name(""),
    _cache(),
    _LRU_time(cur_time),
    _time_limit(1*60*1000), // // once per minute check for processing
    _free_item(nullptr),
    _compact_item(nullptr) {}

Cache::~Cache()
{
    clear();
    // Don't remove cache system from itself, check if name is not empty
    if (*_name)
        cache_system->remove(_name);
}

void Cache::remove(const std::string& name)
{
    auto iter = _cache.find(name);
    if (iter != _cache.end())
    {
        if (_free_item)
            _free_item(iter->second.data);
        _cache.erase(iter);
    }
}

void Cache::clear()
{
    if (_free_item)
    {
        for (auto& name_value: _cache)
            _free_item(name_value.second.data);
    }
    _cache.clear();
}

void *Cache::find(const std::string& name)
{
    auto iter = _cache.find(name);
    if (iter == _cache.end())
        return nullptr;
    iter->second.use();
    return iter->second.data;
}

#ifdef ELC
void Cache::print_sizes() const
{
    char str[256];

    for (const auto& name_value: _cache)
    {
        Uint32 size = name_value.second.data_size;
        char scale = ' ';
        if (size > 100000000)
        {
            size /= 1024*1024;
            scale = 'M';
        }
        else if (size > 100000)
        {
            size /= 1024;
            scale = 'K';
        }

        safe_snprintf(str, sizeof(str), "%s %6d%c: %s",
                      cache_size_str, size, scale, name_value.first.c_str());
        put_colored_text_in_buffer(c_yellow1, CHAT_SERVER, (unsigned char*)str, -1);
#ifdef MAP_EDITOR2
        log_error(str);
#else
        write_to_log(CHAT_SERVER, (unsigned char*)str, strlen(str));
#endif
    }
}
#endif

Uint32 Cache::clean_unused()
{
    if (_time_limit == 0)
        return 0;

    Uint32 freed = 0;
    auto iter = _cache.begin();
    while (iter != _cache.end())
    {
        CacheItem& item = iter->second;
        if (item.access_count > 0 || item.access_time + _time_limit >= cur_time)
        {
            ++iter;
        }
        else
        {
            freed += item.data_size;
            if (_free_item)
                _free_item(item.data);
            iter = _cache.erase(iter);
        }
    }

    update_LRU();
    return freed;
}

Uint32 Cache::compact(bool unused_only)
{
    if (!_compact_item || (unused_only && _time_limit == 0))
        return 0;

    Uint32 freed = 0;
    for (auto& name_value: _cache)
    {
        CacheItem& item = name_value.second;
        if (!item.data)
            continue;

        if (item.access_count > 0
            || (unused_only && item.access_time + _time_limit >= cur_time))
        {
            item.access_count = 0;
        }
        else
        {
            Uint32 new_size = _compact_item(item.data);
            freed += item.data_size - new_size;
            item.set_data_size(new_size);
        }
    }

    update_LRU();
    return freed;
}

Uint32 Cache::memory_size() const
{
    // Size used by this structure itself
    Uint32 res = sizeof(*this);
    // Size used for storing cached items
    res += size() * sizeof(CacheItem);
    // Try to guess the amount of memory used by the hash map
    res += _cache.bucket_count() * (sizeof(size_t) + sizeof(void*))
        + size() * (sizeof(void*) + sizeof(void*) + sizeof(CacheItem));
    // Add size of cached data
    res += std::accumulate(_cache.begin(), _cache.end(), 0u,
                           [](Uint32 s, auto name_value) {
                                return s + name_value.second.data_size;
                           });

    return res;
}

Uint32 CacheSystem::maintenance()
{
    if (!cur_time || _LRU_time + _time_limit > cur_time)
        return 0;

    Uint32 freed = 0;
    for (auto name_value: _cache)
        freed += (reinterpret_cast<Cache*>(name_value.second.data))->compact();

    update_LRU();
    return freed;
}

#ifdef ELC
void CacheSystem::print_sizes() const
{
    char str[256];

    for (const auto& name_value: _cache)
    {
        const Cache *temp = reinterpret_cast<const Cache*>(name_value.second.data);

        Uint32 size = temp->memory_size();
        char scale = ' ';
        if (size > 100000000)
        {
            size /= 1024*1024;
            scale = 'M';
        }
        else if (size > 100000)
        {
            size /= 1024;
            scale = 'K';
        }

        safe_snprintf(str, sizeof(str), "%s %6d%c: %s (%d %s)",
                        cache_size_str, size, scale, name_value.first.c_str(),
                        temp->size(), cache_items_str);
        put_colored_text_in_buffer(c_yellow1, CHAT_SERVER, (unsigned char*)str, -1);
#ifdef MAP_EDITOR2
        log_error(str);
#else
        write_to_log(CHAT_SERVER, (unsigned char*)str, strlen(str));
#endif
    }
}
#endif // ELC



// C wrapper functions

extern "C"
{

Cache *ncache_init(const char* name, Cache::FreeHandler free_item)
{
    return new Cache(name, free_item);
}
void ncache_delete(Cache *cache)
{
    delete cache;
}

void ncache_set_time_limit(Cache *cache, Uint32 time_limit)
{
    cache->set_time_limit(time_limit);
}
void ncache_set_compact(Cache *cache, Uint32 (*compact_item)())
{
    cache->set_compact(reinterpret_cast<Cache::CompactHandler>(compact_item));
}

int ncache_add_item(Cache *cache, const char* name, void *item, Uint32 size)
{
    return cache->add(name, item, size);
}

void ncache_use(Cache *cache, const char* name)
{
    cache->use(name);
}
void ncache_set_size(Cache *cache, const char* name, Uint32 new_size)
{
    cache->set_data_size(name, new_size);
}
void ncache_adj_size(Cache *cache, const char* name, Sint32 size)
{
    cache->adjust_data_size(name, size);
}

void *ncache_find_item(Cache *cache, const char* name)
{
    return cache->find(name);
}

Uint32 ncache_compact_all(Cache *cache)
{
    return cache->compact(false);
}

#ifdef ELC
void ncache_dump_sizes(const Cache *cache)
{
    cache->print_sizes();
}
#endif // ELC

void ncache_system_init()
{
    cache_system = new CacheSystem();
}

void ncache_system_maint()
{
    cache_system->maintenance();
}

} // extern "C"

