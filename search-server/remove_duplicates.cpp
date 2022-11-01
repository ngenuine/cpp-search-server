#include <set>
#include <map>
#include <string>
#include "remove_duplicates.h"
#include "search_server.h"

using namespace std::literals;

void RemoveDuplicates(SearchServer& search_server) {
    std::map<std::set<std::string>, std::set<int>> duplicates;
    for (const int document_id : search_server) {
        std::set<std::string> document_words{};
        for (const auto& [word, freq] : search_server.GetWordFrequencies(document_id)) {
            document_words.insert(word);
        }

        duplicates[document_words].insert(document_id);

    }

    std::set<int> to_remove;
    for (const auto& [word_set, doc_id_set]: duplicates) {
        if (doc_id_set.size() > 1) {
            auto removed_document = doc_id_set.rbegin();
            while (removed_document != --doc_id_set.rend()) {
                to_remove.insert(*removed_document);
                ++removed_document;
            }
        }
    }
    
    for (const int document_id : to_remove) {
        std::cout << "Found duplicate document id "s << document_id << std::endl;
        search_server.RemoveDocument(document_id);
    }
}