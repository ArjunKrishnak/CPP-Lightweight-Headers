/* @file LRUCacheTest.cpp */

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <memory>
#include "LRUCache.hpp"

using namespace lru;

class lru_tests{
public:
    typedef LRUCache<std::string, int> LRU_Cache;
    typedef LRU_Cache::node_type LRU_Node;
    typedef LRU_Cache::list_type LRU_List;
    
    static void LRUInsertCallback(void *insertContext, const LRU_Node & node)
    {
        std::cout<<"LRUInsertCallback"<<std::endl;
    }
    
    static void LRURemoveCallback(void *deleteContext, const LRU_Node & node)
    {
        std::cout<<"LRURemoveCallback"<<std::endl;
    }
    
    lru_tests(){};
    
    // Test the singe threaded version with no lock
    void testNoLock() {
        std::cout << "Testing the singe threaded version " << std::endl;
        auto cachePrint =
        [&] (const LRU_Cache& c) {
            std::cout << "Cache (size: "<<c.size()<<") (max="<<c.getMaxSize()<<") (e="<<c.getElasticity()<<") (allowed:" << c.getMaxAllowedSize()<<")"<< std::endl;
            size_t index = 0;
            auto nodePrint = [&] (const LRU_Node& n) {
                std::cout << " ... [" << ++index <<"] " << n.key << " => " << n.value <<" size = "<<n.size<< std::endl;
            };
            c.cwalk(nodePrint);
        };
        LRU_Cache LRU(50, 10, LRUInsertCallback, LRURemoveCallback, nullptr, nullptr);
        LRU.insert("hello", 1, 5);
        LRU.insert("world", 2, 5);
        LRU.insert("this", 3, 4);
        LRU.insert("is", 4, 2);
        LRU.insert("your", 5, 4);
        LRU.insert("LRU", 5, 3);
        LRU.insert("Cache", 6, 5);
        LRU.insert(":)", 6, 5);
        cachePrint(LRU);
        LRU.get("hello");
        std::cout << "... hello should move to the top..." << std::endl;
        cachePrint(LRU);
        LRU.remove(":)");
        std::cout << "... :) should be removed..." << std::endl;
        cachePrint(LRU);
    }
    
    // Test a thread-safe version of the cache with a std::mutex
    void testWithLock() {
        std::cout << "Testing the Multi threaded version " << std::endl;
        using LCache = LRUCache<std::string, std::string, std::mutex>;
        auto cachePrint =
        [&] (const LCache& c) {
            std::cout << "Cache (size: "<<c.size()<<") (max="<<c.getMaxSize()<<") (e="<<c.getElasticity()<<") (allowed:" << c.getMaxAllowedSize()<<")"<< std::endl;
            size_t index = 0;
            auto nodePrint = [&] (const LCache::node_type& n) {
                std::cout << " ... [" << ++index <<"] " << n.key << " => " << n.value <<" size = "<<n.size<< std::endl;
            };
            c.cwalk(nodePrint);
        };
        // with a lock
        LCache lc(25,2);
        auto worker = [&] () {
            std::ostringstream os;
            os << std::this_thread::get_id();
            std::string id = os.str();
            
            for (int i = 0; i < 10; i++) {
                std::ostringstream os2;
                os2 << "id:"<<id<<":"<<i;
                //generate a random size which is between 0 and 5
                size_t random_size = rand()%5;
                lc.insert(os2.str(), id, random_size);
                
            }
        };
        std::vector<std::unique_ptr<std::thread>> workers;
        workers.reserve(5);
        for (int i = 0; i < 5; i++) {
            workers.push_back(std::unique_ptr<std::thread>(
                                                           new std::thread(worker)));
        }
        
        for (const auto& w : workers) {
            w->join();
        }
        std::cout << "... workers finished!" << std::endl;
        cachePrint(lc);
    }
    
};

int main(int argc, char** argv) {
   lru_tests tst;
   tst.testNoLock();
   tst.testWithLock();
   return 0;
}


