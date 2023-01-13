#include "string_processing.h"

std::vector<std::string> SplitIntoWords(const std::string& text) {

    std::vector<std::string> words;
    std::string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

std::vector<std::string_view> SplitIntoWordsView(std::string_view str) {
    std::vector<std::string_view> result;

    str.remove_prefix(std::min(str.find_first_not_of(' '), str.size()));
    std::string_view word = str.substr(0, str.find_first_of(' '));


    while (word.size()) {
        result.push_back(word);
        str.remove_prefix(word.size());
        str.remove_prefix(std::min(str.find_first_not_of(' '), str.size()));
        word = str.substr(0, str.find_first_of(' '));
    }

    return result;
}