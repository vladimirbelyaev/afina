#ifndef AFINA_STORAGE_MAP_BASED_GLOBAL_LOCK_IMPL_H
#define AFINA_STORAGE_MAP_BASED_GLOBAL_LOCK_IMPL_H

#include <unordered_map>
#include <mutex>
#include <string>

#include <afina/Storage.h>

namespace Afina {
namespace Backend {

/**
 * # Map based implementation with global lock
 *
 *
 */

    struct entry{
        std::string _data;
        entry* _prev;
        entry* _next;
        //std::unordered_map<std::string, entry*>::iterator _key;
        std::string _key;
        std::unordered_map<std::string, entry*>::iterator key1;
        entry(std::string key = "",
              std::string data = "",
              entry* prev = nullptr,
              entry* next = nullptr
              ) :
                                                                                _data(data),
                                                                                _prev(prev),
                                                                                _next(next),
                                                                                _key(key){}
    };


class MapBasedGlobalLockImpl : public Afina::Storage {
public:
    MapBasedGlobalLockImpl(size_t max_size = 1024, size_t curr_size = 0) : _max_size(max_size),
                                                                            _curr_size(curr_size){
        _head = new entry();
        _tail = new entry();
        _head->_prev = nullptr;
        _head->_next = _tail;
        _tail->_prev = _head;
        _tail->_next = nullptr;
    }
    ~MapBasedGlobalLockImpl() {}

    bool CheckListChain() const;
    // Implements Afina::Storage interface
    bool Put(const std::string &key, const std::string &value) override;

    // Implements Afina::Storage interface
    bool PutIfAbsent(const std::string &key, const std::string &value) override;

    // Implements Afina::Storage interface
    bool Set(const std::string &key, const std::string &value) override;

    // Implements Afina::Storage interface
    bool Delete(const std::string &key) override;

    // Implements Afina::Storage interface
    bool Get(const std::string &key, std::string &value) const override;

    void MoveToHead(entry* value) const;


private:
    mutable entry* _head;
    mutable entry* _tail;
    size_t _max_size;
    size_t _curr_size;
    std::unordered_map<const std::string, entry*,std::hash<std::string>,
            std::equal_to<std::string>> _backend;
};

} // namespace Backend
} // namespace Afina

#endif // AFINA_STORAGE_MAP_BASED_GLOBAL_LOCK_IMPL_H
