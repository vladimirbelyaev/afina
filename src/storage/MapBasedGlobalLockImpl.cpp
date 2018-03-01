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

bool MapBasedGlobalLockImpl::FreeCache(const size_t& input_size){
    std::lock_guard<std::recursive_mutex> lk(m);
    if(input_size > _max_size)
        return false;
    while(_curr_size + input_size > _max_size){
        auto key_to_delete = _tail->_prev->_key_iterator;
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
        if(!(_backend.count(key))){
            if(!FreeCache(SizeOfNode(key,value,_type)))
                return false;
            // We have memory
            /* LRU: create new node, pin to head, put key-pointer into map */
            entry* new_node = new entry(/*key,*/ value, _head, _head->_next);
            _head-> _next-> _prev = new_node;
            _head-> _next = new_node;
            _backend[key] = new_node;
            new_node ->_key_iterator = _backend.find(key);
            _curr_size += SizeOfNode(key, value, _type);
            return true;
        }else return false;
    }



// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Set(const std::string &key, const std::string &value) {
        std::lock_guard<std::recursive_mutex> lk(m);
        if(_backend.count(key)){
            MoveToHead(_backend[key]);
            if(!FreeCache(SizeOfNode(key,value,_type) - SizeOfNode(key,_backend[key]->_data,_type)))
                return false;
            _backend[key]->_data = value;
            _curr_size += SizeOfNode(key,value,_type) - SizeOfNode(key,_backend[key]->_data,_type);
            //TEST
            _backend[key]->_key_iterator = _backend.find(key);
            //TEST
            return true;
        }
        return false;
    }

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Delete(const std::string &key) {
        std::lock_guard<std::recursive_mutex> lk(m);
        if(_backend.count(key)){
            auto node_to_delete = _backend[key];
            _curr_size -= SizeOfNode(key, _backend[key]->_data, _type);
            _backend.erase(key);
            node_to_delete-> _next-> _prev = node_to_delete-> _prev;
            node_to_delete-> _prev-> _next = node_to_delete-> _next;
            delete node_to_delete;
            return true;
        }
        return true;
    }

bool MapBasedGlobalLockImpl::Delete(const std::unordered_map<std::string, entry*>::iterator it){
    std::lock_guard<std::recursive_mutex> lk(m);
    return Delete(it->first);
}

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Get(const std::string &key, std::string &value) const {
        std::lock_guard<std::recursive_mutex> lk(m);
        if(_backend.count(key)){
            value = _backend.find(key)->second ->_data;
            MoveToHead(_backend.find(key)->second);
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
