#pragma once
#include <vector>
#include <string>
#include <string_view>
#include <set>
#include <algorithm>

std::vector<std::string> SplitIntoWords(const std::string& text);

std::vector<std::string_view> SplitIntoWordsView(std::string_view str);

template <typename StringContainer>
std::set<std::string_view> MakeSetStopWords(const StringContainer& words) {
    std::set<std::string_view> stop_words;
    for (std::string_view word : words) {
        if (!word.empty()) {
            stop_words.insert(word);
        }
    }
    
    return stop_words;
}