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
                                                  guards_(bucket_count),
                                                  parts_(bucket_count)
    {
    }

    struct Access {
        std::lock_guard<std::mutex> guard_;
        Value& ref_to_value;
    };

    Access operator[](const Key& key) {
        // сначала! блокирую корзину, потом лезу в неё
        return {std::lock_guard(guards_[key % buckets_]), parts_[key % buckets_][key]};
    }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> result;
        for (size_t i = 0; i < parts_.size(); ++i) {
            std::lock_guard<std::mutex> guard(guards_[i]); // заблокировал корзину, в которой "лежит" словать
            for (auto& [k, v] : parts_[i]) { // забираю из корзины все, находясь под защитой guard
                result[k] = v;
            }
        }

        return result;
    }

    void erase(Key key) {
        parts_[key % buckets_].erase(key);
    }

private:
    size_t buckets_;
    std::vector<std::mutex> guards_;
    std::vector<std::map<Key, Value>> parts_;
};