#ifndef AFINA_STORAGE_SIMPLE_LRU_H
#define AFINA_STORAGE_SIMPLE_LRU_H

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <algorithm>
#include <cassert>

#include <afina/Storage.h>

namespace Afina {
namespace Backend {

/**
 * # Map based implementation
 * That is NOT thread safe implementaiton!!
 */
class SimpleLRU : public Afina::Storage {
public:
    SimpleLRU(size_t max_size = 4*1024) : _max_size(max_size) {}

    ~SimpleLRU() {
        _lru_index.clear();
        _lru_head.release(); // TODO: Here is stack overflow
        _lru_tail.release();
    }

    void print();

    // Implements Afina::Storage interface
    bool Put(const std::string &key, const std::string &value) override;

    // Implements Afina::Storage interface
    bool PutIfAbsent(const std::string &key, const std::string &value) override;

    // Implements Afina::Storage interface
    bool Set(const std::string &key, const std::string &value) override;

    // Implements Afina::Storage interface
    bool Delete(const std::string &key) override;

    // Implements Afina::Storage interface
    bool Get(const std::string &key, std::string &value) override;

private:
    // LRU cache node
    using lru_node = struct lru_node {
        lru_node(const std::string& k, const std::string& v) : key(k), value(v) {};

        const std::string key;
        std::string value;
        std::unique_ptr<lru_node> prev;
        std::unique_ptr<lru_node> next;
    };

    void cache_cleaner(const size_t key_size, const size_t node_size);
    void put_node_in_head(lru_node *tmp_node);
    void release_node(lru_node *tmp_node);

    // Maximum number of bytes could be stored in this cache.
    // i.e all (keys+values) must be not greater than the _max_size
    std::size_t _max_size;

    // Main storage of lru_nodes, elements in this list ordered descending by "freshness": in the head
    // element that wasn't used for longest time.
    //
    // List owns all nodes
    std::unique_ptr<lru_node> _lru_head;
    std::unique_ptr<lru_node> _lru_tail;

    // Index of nodes from list above, allows fast random access to elements by lru_node#key
    std::map<std::reference_wrapper<const std::string>, std::reference_wrapper<lru_node>, std::less<std::string>> _lru_index;
};

} // namespace Backend
} // namespace Afina

#endif // AFINA_STORAGE_SIMPLE_LRU_H
