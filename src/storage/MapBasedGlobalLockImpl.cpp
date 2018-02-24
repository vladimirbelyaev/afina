#include "MapBasedGlobalLockImpl.h"

#include <mutex>
#include <iostream>

namespace Afina {
namespace Backend {

// See MapBasedGlobalLockImpl.h
void MapBasedGlobalLockImpl::MoveToHead(entry* value) const{
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


bool MapBasedGlobalLockImpl::Put(const std::string &key, const std::string &value) {
        if(_backend.count(key)){ // If there is a pointer to node
            (_backend[key])->_data = value; // Rewrite the value
            MoveToHead(_backend[key]); // LRU implementation
        }else{
            if (!(_curr_size == _max_size)){/* LRU: create new node, pin to head, put key-pointer into map */
                entry* new_node = new entry(key, value, _head, _head->_next);
                _head-> _next-> _prev = new_node;
                _head-> _next = new_node;
                _backend[new_node->_key] = new_node;
                _curr_size++;

            }else{/* LRU: change data in the last node, delete key-value for last node,
                                        * create key-value for new node, move to head
                                        */
                auto node_to_change = _tail->_prev;
                _backend.erase(node_to_change-> _key); // Delete key-value for last node
                node_to_change-> _data = value; // Change data in last node
                node_to_change-> _key = key;
                _backend[node_to_change-> _key] = node_to_change; // Create key-value for pair to be inserted
                MoveToHead(node_to_change); //Move to head
            }
        }
    return true;
    }

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::PutIfAbsent(const std::string &key, const std::string &value) {
        if(!(_backend.count(key))){
            if (!(_curr_size == _max_size)){/* LRU: create new node, pin to head, put key-pointer into map */
                entry* new_node = new entry(key, value, _head, _head->_next);
                _head-> _next-> _prev = new_node;
                _head-> _next = new_node;
                _backend[new_node->_key] = new_node;
                _curr_size++;

            }else{/* LRU: change data in the last node, delete key-value for last node,
                                        * create key-value for new node, move to head
                                        */
                auto node_to_change = _tail->_prev;
                _backend.erase(node_to_change-> _key); // Delete key-value for last node
                node_to_change-> _data = value; // Change data in last node
                node_to_change-> _key = key;
                _backend[node_to_change-> _key] = node_to_change; // Create key-value for pair to be inserted
                MoveToHead(node_to_change); //Move to head
            }
            return true;
        }else return true;
    }

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Set(const std::string &key, const std::string &value) {
        if(_backend.count(key)){
            _backend[key] -> _data = value;// Write the new value
            MoveToHead(_backend[key]);//Move to head
            return true;
        }
        return true;
    }

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Delete(const std::string &key) {
        if(_backend.count(key)){
            auto node_to_delete = _backend[key];
            _backend.erase(key);
            node_to_delete-> _next-> _prev = node_to_delete-> _prev;
            node_to_delete-> _prev-> _next = node_to_delete-> _next;
            delete node_to_delete;
            _curr_size--;
            return true;
        }
        return true;
    }

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Get(const std::string &key, std::string &value) const {
        if(_backend.count(key)){
            value = _backend.find(key)->second ->_data;
            MoveToHead(_backend.find(key)->second); //Should be, but const class?
            return true;
        }
        return false;
    }

} // namespace Backend
} // namespace Afina
