#pragma once

#include <future>
#include <mutex>
#include <map>
#include <vector>

using namespace std::string_literals;

template <typename Key, typename Value>
class ConcurrentMap {
public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys"s);

    explicit ConcurrentMap(size_t bucket_count) : buckets_(bucket_count),
                                                  cm_(bucket_count)
    {
    }

    struct Access {
        std::lock_guard<std::mutex> guard_;
        Value& ref_to_value;
    };

    Access operator[](const Key& key) {
        // сначала! блокирую корзину, потом лезу в неё
        return {std::lock_guard(cm_[key % buckets_].guard), cm_[key % buckets_].Map[key]};
    }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> result;
        for (auto& cmap : cm_) {
            std::lock_guard<std::mutex> guard(cmap.guard); // заблокировал корзину, в которой "лежит" словать
            for (auto& [k, v] : cmap.Map) { // забираю из корзины все, находясь под защитой guard
                result[k] = v;
            }
        }

        return result;
    }

    void erase(Key key) {
        cm_[key % buckets_].Map.erase(key);
    }

private:

    struct CMap {
        std::mutex guard;
        std::map<Key, Value> Map;
    };

    size_t buckets_;
    std::vector<CMap> cm_;
};