#include "MapBasedGlobalLockImpl.h"

#include <mutex>
#include <iostream>

namespace Afina {
namespace Backend {



// See MapBasedGlobalLockImpl.h
void MapBasedGlobalLockImpl::MoveToHead(entry* value) const{
        std::lock_guard<std::recursive_mutex> lk(m);
        //if(value->_prev != _head) {
            value->_next->_prev = value->_prev;
            value->_prev->_next = value->_next; // Connect entries where entry is cut from
            value->_next = _head->_next;
            value->_prev = _head; // Pin entry to head
            _head->_next = value; //Pin head to entry
        //}
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
    while(_curr_size + input_size > _max_size){
        auto key_to_delete = _tail->_prev->_key;
        Delete(key_to_delete);
    }
    return true;
}


bool MapBasedGlobalLockImpl::Put(const std::string &key, const std::string &value) {
    std::lock_guard<std::recursive_mutex> lk(m);

    if(_backend.count(key)){ // If there is a key to node..
        MoveToHead(_backend[key]);
        FreeCache(SizeOfNode(key,value,_type) - SizeOfNode(key,_backend[key]->_data,_type));
        _backend[key]->_data = value;
        _curr_size += SizeOfNode(key,value,_type) - SizeOfNode(key,_backend[key]->_data,_type);


    }else{ // The key is new
        FreeCache(SizeOfNode(key,value,_type));
        // We have memory
        /* LRU: create new node, pin to head, put key-pointer into map */
        entry* new_node = new entry(key, value, _head, _head->_next, _backend.bucket(key));
        _head-> _next-> _prev = new_node;
        _head-> _next = new_node;
        _backend[new_node->_key] = new_node;
        _curr_size += SizeOfNode(key, value, _type);

        }
    return true;
    }

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::PutIfAbsent(const std::string &key, const std::string &value) {
        std::lock_guard<std::recursive_mutex> lk(m);
        if(!(_backend.count(key))){
            return Put(key,value);
        }else return false;
    }



// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Set(const std::string &key, const std::string &value) {
        std::lock_guard<std::recursive_mutex> lk(m);
        if(_backend.count(key)){
            _backend[key] -> _data = value;// Write the new value
            MoveToHead(_backend[key]);//Move to head
            return true;
        }
        return true;
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

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Get(const std::string &key, std::string &value) const {
        std::lock_guard<std::recursive_mutex> lk(m);
        if(_backend.count(key)){
            value = _backend.find(key)->second ->_data;
            MoveToHead(_backend.find(key)->second); //Should be, but const class?
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
