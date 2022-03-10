#include "SimpleLRU.h"
#include <iostream>

namespace Afina {
namespace Backend {

void SimpleLRU::print() {
    if (!_lru_head && !_lru_tail && !_lru_index.size()) {
        std::cout << "Empty\n";
        return;
    }
    else {
        std::cout << "NOT Empty " << _lru_index.size() << '\n';
    }

    std::cout << "_lru_head = {" << _lru_head->key << " - " << _lru_head->value << "}\n";
    std::cout << "_lru_tail = {" << _lru_tail->key << " - " << _lru_tail->value << "}\n";

    for (auto const &pair: _lru_index) {
        std::cout << "{" << pair.first.get() << ": " << pair.second.get().key << " - " << pair.second.get().value << "}\n";
    }

    lru_node *node = _lru_head.get();
    while (node) {
        std::cout << node->key << ") " << node->value << "\n";
        node = node->next.get();
    }
}

void SimpleLRU::cache_cleaner(const size_t key_size, const size_t node_size) {
    // check cache overflow
    std::size_t cache_size = sizeof(_lru_index) + 
        _lru_index.size() * (sizeof(decltype(_lru_index)::key_type) + 
        sizeof(decltype(_lru_index)::mapped_type));

    // if cache might overflow - clear 1/4 of all cache
    if ( cache_size + key_size + node_size >= _max_size ) {
        int num_to_del = _lru_index.size() / 4;
        // std::cout << "Starting to clear:: cache_size=" << cache_size << " _lru_index.size()=" << _lru_index.size() << " num_to_del=" << num_to_del << '\n';

        int cnt = 0;
        lru_node *tmp_node = _lru_tail.get();
        while ( tmp_node && (cnt < num_to_del) ) {
            for (auto it = _lru_index.begin(); it != _lru_index.end(); ++it) {
                if (it->first.get() == tmp_node->key) {
                    _lru_index.erase( it );
                    ++cnt;
                    break;
                }
            }
            tmp_node = tmp_node->prev.get();
        }
    }
}

void SimpleLRU::release_node(lru_node *tmp_node) {
    if (tmp_node->prev) {
        tmp_node->prev->next.release();
        tmp_node->prev->next = std::unique_ptr<lru_node>(tmp_node->next.get());
    }
    if (tmp_node->next) {
        tmp_node->next->prev.release();
        tmp_node->next->prev = std::unique_ptr<lru_node>(tmp_node->prev.get());
    }
    // if deleting node is last one
    if (tmp_node->prev && !tmp_node->next) {
        _lru_tail.release();
        _lru_tail = std::unique_ptr<lru_node>(tmp_node->prev.get());
    }
}

void SimpleLRU::put_node_in_head(lru_node *tmp_node) {
    if (tmp_node == _lru_head.get())
        return;

    release_node(tmp_node);

    tmp_node->prev.release();
    tmp_node->next.release();
    tmp_node->next.reset(_lru_head.get());
    if (_lru_head) {
        _lru_head->prev.reset(tmp_node);
        _lru_head.release();
    }
    _lru_head = std::unique_ptr<lru_node>(tmp_node);
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Put(const std::string &key, const std::string &value) { 
    // check if key in cache - set if in

    auto it = _lru_index.find(key);
    if (it != _lru_index.end()) {
        _lru_index.at(key).get().value = value;
        put_node_in_head(&it->second.get());
        return true;
    }

    // if not - check in list
    lru_node *tmp_node = _lru_head.get();
    while (tmp_node) {
        if (tmp_node->key == key) {
            tmp_node->value = value;
            // put node in head
            put_node_in_head(tmp_node);
            assert(!_lru_head->prev);
            return true;
        }
        tmp_node = tmp_node->next.get();
    }

    // create new node
    lru_node *node = new lru_node(key, value);

    // size of reference wrapper = 8
    cache_cleaner(8, sizeof(node));

    put_node_in_head(node);

    assert(!_lru_head->prev);

    if (!_lru_tail) {
        _lru_tail = std::unique_ptr<lru_node>(node);
    }

    // put node in cache
    _lru_index.insert({std::cref(_lru_head.get()->key), std::ref(*_lru_head.get())});

    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value) { 
    // check if key in cache - set if in
    auto it = _lru_index.find(key);
    if (it != _lru_index.end()) {
        return false;
    }

    // if not - check in list
    lru_node *tmp_node = _lru_head.get();
    while (tmp_node) {
        if (tmp_node->key == key) {
            // put node in head
            put_node_in_head(tmp_node);
            assert(!_lru_head->prev);
            return false;
        }
        tmp_node = tmp_node->next.get();
    }


    // create new node
    lru_node *node = new lru_node(key, value);

    cache_cleaner(sizeof(key), sizeof(node));

    put_node_in_head(node);

    assert(!_lru_head->prev);

    if (!_lru_tail) {
        _lru_tail = std::unique_ptr<lru_node>(node);
    }

    // put node in cache
    _lru_index.insert({std::cref(_lru_head.get()->key), std::ref(*_lru_head.get())});

    return true;    
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Set(const std::string &key, const std::string &value) { 
    // check if key in cache - set if in

    auto it = _lru_index.find(key);
    if (it != _lru_index.end()) {
        _lru_index.at(key).get().value = value;
        put_node_in_head(&it->second.get());
        return true;
    }

    // if not - check in list
    lru_node *tmp_node = _lru_head.get();
    while (tmp_node) {
        if (tmp_node->key == key) {
            tmp_node->value = value;
            // put node in head
            put_node_in_head(tmp_node);
            assert(!_lru_head->prev);
            return true;
        }
        tmp_node = tmp_node->next.get();
    }

    return false; 
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Delete(const std::string &key) { 
    // check if key in cache - set if in

    auto it = _lru_index.find(key);
    if (it != _lru_index.end()) {
        _lru_index.erase( it );
        release_node(&it->second.get());
        if (_lru_index.size() == 0) {
            _lru_head.release();
            _lru_tail.release();
        }

        return true;
    }

    // if not - check in list
    lru_node *tmp_node = _lru_head.get();
    while (tmp_node) {
        if (tmp_node->key == key) {
            release_node(tmp_node);
            
            return true;
        }
        tmp_node = tmp_node->next.get();
    }
    return false; 
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Get(const std::string &key, std::string &value) { 
    // check if key in cache - set if in

    auto it = _lru_index.find(key);
    if (it != _lru_index.end()) {
        value = _lru_index.at(key).get().value;
        return true;
    }

    // if not - check in list
    lru_node *tmp_node = _lru_head.get();
    while (tmp_node) {
        if (tmp_node->key == key) {
            value = tmp_node->value;
            // put node in head
            put_node_in_head(tmp_node);
            assert(!_lru_head->prev);
            return true;
        }
        tmp_node = tmp_node->next.get();
    }

    return false; 
}

} // namespace Backend
} // namespace Afina
