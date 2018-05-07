#include "MapBasedGlobalLockImpl.h"

#include <mutex>
#include <iostream>

namespace Afina {
namespace Backend {



// See MapBasedGlobalLockImpl.h
void MapBasedGlobalLockImpl::MoveToHead(entry* value) const{
        std::lock_guard<std::recursive_mutex> lk(m);
        if(value == _head->_next)
            return;
        value->_next->_prev = value->_prev;
        value->_prev->_next = value->_next; // Connect entries where entry is cut from
        value->_next = _head->_next;
        value->_prev = _head; // Pin entry to head
        _head->_next = value; //Pin head to entry
        return;
    }

bool MapBasedGlobalLockImpl::CheckListChain() const{
    std::lock_guard<std::recursive_mutex> lk(m);
    auto node = _head;
    while(node-> _next){
        node = node-> _next;
    }
    auto test_one = bool(node == _tail);
    while(node-> _prev){
        node = node-> _prev;
    }
    auto test_two = bool(node == _head);

    return test_one && test_two;

}

bool MapBasedGlobalLockImpl::FreeCache(const int& input_size){
    std::lock_guard<std::recursive_mutex> lk(m);
    if(input_size < 0){
        _curr_size -= input_size;
        return true;
    }
    if(input_size > _max_size)
        return false;
    while(_curr_size + input_size > _max_size){
        auto key_to_delete = _tail->_prev->_key_pointer;
        Delete(key_to_delete);
    }
    return true;
}


bool MapBasedGlobalLockImpl::Put(const std::string &key, const std::string &value) {
    std::lock_guard<std::recursive_mutex> lk(m);
    if(Set(key,value))
        return true;
    return PutIfAbsent(key,value);
    }

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::PutIfAbsent(const std::string &key, const std::string &value) {
        std::lock_guard<std::recursive_mutex> lk(m);
        if(_backend.find(key)==_backend.end()){
            if(!FreeCache(SizeOfNode(key,value,_type)))
                return false;
            // We have memory
            /* LRU: create new node, pin to head, put key-pointer into map */
            entry* new_node = new entry(value, _head, _head->_next);
            _head-> _next-> _prev = new_node;
            _head-> _next = new_node;
            _backend[key] = new_node;
            new_node ->_key_pointer = &(_backend.find(key)->first);
            _curr_size += SizeOfNode(key, value, _type);
            return true;
        }else return false;
    }



// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Set(const std::string &key, const std::string &value) {
        std::lock_guard<std::recursive_mutex> lk(m);
        auto node = _backend.find(key);
        if(node != _backend.end()){
            MoveToHead(node->second);
            if(!FreeCache(SizeOfNode(key,value,_type) - SizeOfNode(key,node->second->_data,_type)))
                return false;
            node->second->_data = value;
            _curr_size += SizeOfNode(key,value,_type) - SizeOfNode(key,node->second->_data,_type);
            //TEST
            node->second->_key_pointer = &(node->first);

            //TEST
            return true;
        }
        return false;
    }

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Delete(const std::string &key) {
        std::lock_guard<std::recursive_mutex> lk(m);
        auto pair_to_delete = _backend.find(key);
        if(pair_to_delete != _backend.end()){
            _curr_size -= SizeOfNode(key, pair_to_delete->second->_data, _type);
            _backend.erase(key);
            pair_to_delete->second-> _next-> _prev = pair_to_delete->second-> _prev;
            pair_to_delete->second-> _prev-> _next = pair_to_delete->second-> _next;
            delete pair_to_delete->second;
            return true;
        }
        return false;
    }

bool MapBasedGlobalLockImpl::Delete(const std::string* key){
    std::lock_guard<std::recursive_mutex> lk(m);
    return Delete(*key);
    }

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Get(const std::string &key, std::string &value) const {
        std::lock_guard<std::recursive_mutex> lk(m);
        auto pair = _backend.find(key);
        if(pair != _backend.end()){
            value = pair->second ->_data;
            MoveToHead(pair->second);
            return true;
        }
        return false;
    }

size_t MapBasedGlobalLockImpl::SizeOfNode(const std::string &key, const std::string &value, const bool type) const{/*true type - sum, false type - 1*/
    std::lock_guard<std::recursive_mutex> lk(m);
    if(type)
        return key.size() + value.size();
    else return 1;
}

} // namespace Backend
} // namespace Afina
