/* @file LRUCache.hpp*/
#ifndef LRU_CACHE_HPP
#define LRU_CACHE_HPP

#include <algorithm>
#include <cstdint>
#include <list>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <unordered_map>

namespace lru {

    /*
     * a no op lockable concept that can be used in place of std::mutex
     */
    class NullLock {
    public:
        void lock() {}
        void unlock() {}
        bool try_lock() { return true; }
    };

    /**
     * error raised when a key not in cache is passed to get()
     */
    class KeyNotFound : public std::invalid_argument {
    public:
        KeyNotFound() : std::invalid_argument("key_not_found") {}
    };

    /**
     * error raised when a key not in cache is passed to get()
     */
    class TooLargeSize : public std::invalid_argument {
    public:
        TooLargeSize() : std::invalid_argument("val_size_too_large_for_this_cache_size") {}
    };

    /**
     * error raised when a callback fails
     */
    class CallBackFailed : public std::invalid_argument {
    public:
        CallBackFailed() : std::invalid_argument("callback_provided_to_lru_failed") {}
    };

    /**
     * lru cache entry teemplate
     */
    template <typename K, typename V>
    struct KeyValuePair {
    public:
        K key;
        V value;
        size_t size;
        KeyValuePair(const K& k, const V& v, const size_t& s ) : key(k), value(v), size(s) {}
    };

    /**
     * The LRUCache class templated by
     *        Key - key type
     *        Value - value type
     *        MapType - an associative container like std::unordered_map
     *        LockType - a lock type derived from the Lock class (default: NullLock = no synchronization)
     *
     * The default NullLock based template is not thread-safe, however passing a lock like Lock=std::mutex
     * will make it thread-safe
     */
    template <class Key, class Value, class Lock = NullLock,
    class Map = std::unordered_map<
    Key, typename std::list<KeyValuePair<Key, Value>>::iterator>>
    class LRUCache {

    public:
        typedef KeyValuePair<Key, Value> node_type;
        typedef std::list<KeyValuePair<Key, Value>> list_type;
        typedef Map map_type;
        typedef Lock lock_type;
        using Guard = std::lock_guard<lock_type>;
        using Callback = std::function<void(void *,KeyValuePair<Key,Value>)>;
    private:
        // Dissallow copying.
        LRUCache(const LRUCache&) = delete;
        LRUCache& operator=(const LRUCache&) = delete;

        mutable Lock lock_;
        Map cache_;
        list_type keys_;
        size_t maxSize_;
        size_t elasticity_;
        size_t cacheSize_;
        Callback removeCallback_;
        Callback insertCallback_;
        /**
         * Below are Global contexts provided to LRU cache during time of initialization. For insert
         * operations there is a provision of providing local context during the time of each insert()
         * call however for remove this doesn't make sense as it removal of elements can happen at prune()
         * as well as at remove()
         */
        void* insertClientContext_;
        void* removeClientContext_;
    public:
        /**
         * the maxSize is the soft limit of keys and (maxSize + elasticity) is the hard limit
         * the cache is allowed to grow till (maxSize + elasticity) and is pruned back to maxSize keys
         * set maxSize = 0 for an unbounded cache
         */
        explicit LRUCache(size_t maxSize = 64, size_t elasticity = 10, Callback insertCallback = nullptr,
        Callback removeCallback = nullptr,void * insertClientContext = nullptr,void * removeClientContext = nullptr )
        : maxSize_(maxSize), elasticity_(elasticity), cacheSize_(0), 
          removeCallback_(removeCallback), insertCallback_(insertCallback),
          insertClientContext_(insertClientContext),removeClientContext_(removeClientContext){}

        virtual ~LRUCache() = default;

        void updateSize(size_t maxSize, size_t elasticity){
            maxSize_ = maxSize;
            elasticity_ = elasticity;
            prune();
        }

        size_t size() const {
            Guard g(lock_);
            return cacheSize_;
        }

        size_t freeSize() const {
            Guard g(lock_);
            return maxSize_ + elasticity_ - cacheSize_;
        }

        bool empty() const {
            Guard g(lock_);
            return cache_.empty();
        }
        void clear() {
            Guard g(lock_);
            cache_.clear();
            keys_.clear();
        }
        
        void insert(const Key& k, const Value& v,const size_t & s,void * insertClientContext) {
            Guard g(lock_);
            const auto iter = cache_.find(k);

            if (iter != cache_.end()) {
                size_t prev_size = iter->second->size;
                size_t size_diff = s-prev_size;
                if(size_diff > maxSize_ + elasticity_ - cacheSize_)
                    throw TooLargeSize();
                iter->second->size = s;
                iter->second->value = v;
                keys_.splice(keys_.begin(), keys_, iter->second);
                cacheSize_+=size_diff;
            }
            else{
                if(s > maxSize_ + elasticity_)
                    throw TooLargeSize();
                keys_.emplace_front(k, v, s);
                cacheSize_+=s;
                cache_[k] = keys_.begin();
            }
            prune();
            if(insertCallback_){
                try{
                    // if no specific context is provided of the insert use global context
                    if(!insertClientContext)
                        insertCallback_(insertClientContext_,keys_.front());
                    else
                        insertCallback_(insertClientContext,keys_.front());
                }
                catch(...){
                    throw CallBackFailed();
                }
            }
        }

        void insert(const Key& k, const Value& v,const size_t & s) {
            insert(k,v,s,nullptr);
        }

        /**
         * This function throws
         * The const reference returned here is only
         * guaranteed to be valid till the next insert/delete
         */
        const Value& get(const Key& k) {
            Guard g(lock_);
            const auto iter = cache_.find(k);
            if (iter == cache_.end()) {
                throw KeyNotFound();
            }
            keys_.splice(keys_.begin(), keys_, iter->second);
            return iter->second->value;
        }
        /**
         * This function throws
         * returns a copy of the stored object (if found)
         */
        Value getCopy(const Key& k) {
            return get(k);
        }

        /**
         * Try to get the value for k and copy it to v and return true
         */
        bool getCopy(const Key& k, Value& v) {
            Guard g(lock_);
            const auto iter = cache_.find(k);
            if (iter == cache_.end()) {
                return false;
            }
            keys_.splice(keys_.begin(), keys_, iter->second);
            v = iter->second->value;
            return true;
        }
        bool remove(const Key& k) {
            Guard g(lock_);
            auto iter = cache_.find(k);
            if (iter == cache_.end()) {
                return false;
            }
            if(removeCallback_){
                try{
                    removeCallback_(removeClientContext_,*iter->second);
                }
                catch(...){
                    throw CallBackFailed();
                }
            }
            size_t s = iter->second->size;
            keys_.erase(iter->second);
            cache_.erase(iter);
            cacheSize_-=s;
            return true;
        }
        bool contains(const Key& k) const {
            Guard g(lock_);
            return cache_.find(k) != cache_.end();
        }
        size_t getMaxSize() const { return maxSize_; }
        size_t getElasticity() const { return elasticity_; }
        size_t getMaxAllowedSize() const { return maxSize_ + elasticity_; }
        template <typename F>
        void cwalk(F& f) const {
            Guard g(lock_);
            std::for_each(keys_.begin(), keys_.end(), f);
        }

    protected:
        size_t prune() {
            size_t maxAllowed = maxSize_ + elasticity_;
            if (maxSize_ == 0 || cacheSize_ < maxAllowed) {
                return 0;
            }
            size_t removedSize = 0;
            while (cacheSize_ > maxSize_) {
                //Try to execute callback
                if(removeCallback_){
                    try{
                        removeCallback_(removeClientContext_,keys_.back());
                    }
                    catch(...){
                        throw CallBackFailed();
                    }
                }
                //Remove from the LRU
                size_t s = keys_.back().size;
                cache_.erase(keys_.back().key);
                keys_.pop_back();
                removedSize+=s;
                cacheSize_-=s;
            }
            return removedSize;
        }
    };
}
#endif // LRU_CACHE_HPP
