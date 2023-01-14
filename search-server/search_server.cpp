#include <algorithm>
#include <set>
#include <map>
#include <list>
#include <string>
#include <utility>
#include <execution>

#include <iostream>

#include "search_server.h"

std::set<int>::const_iterator SearchServer::begin() const {
    return document_order_.begin();
}

std::set<int>::const_iterator SearchServer::end() const {
    return document_order_.end();
}

SearchServer::SearchServer(const std::string& stop_words_text) : SearchServer(std::string_view(stop_words_text)) {}

SearchServer::SearchServer(std::string_view stop_words_text) : SearchServer(SplitIntoWordsView(stop_words_text)) {}

void SearchServer::AddDocument(int document_id, std::string_view document, const DocumentStatus& status, const std::vector<int>& ratings) {
    
    ThrowSpecialSymbolInText(document);

    if (IsNegativeDocumentId(document_id)) {
        throw std::invalid_argument("Negative document id"s);
    }

    if (IsRecurringDocumentId(document_id)) {
        throw std::invalid_argument("Recurring document id"s);
    }

    // наполняем счетчик документов -- он пригодится для подсчета IDF.
    // одновременно и порядок добавления получаем
    document_order_.insert(document_id); 

    document_info_[document_id] = {ComputeAverageRating(ratings), status};

    all_data_.push_back(static_cast<std::string>(document));

    const std::vector<std::string_view> words = SplitIntoWordsNoStopView(all_data_.back());

    for (std::string_view word : words) {
        TF_by_term_[word][document_id] += 1.0 / words.size(); // Рассчитываем TF каждого слова в каждом документе.
        TF_by_id_[document_id][word] += 1.0 / words.size();
    }
}

std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentStatus given_status /* = DocumentStatus::ACTUAL */) const {
    return FindTopDocuments(raw_query,
                            [given_status](int document_id, DocumentStatus status, int rating) {
                                return status == given_status;
                            });
}

Matching SearchServer::MatchDocument(std::string_view raw_query, int document_id) const {

    
    ThrowSpecialSymbolInText(raw_query);

    if (IsNegativeDocumentId(document_id)) {
        throw std::invalid_argument("Negative document id"s);
    }

    if (IsNonExistentDocumentId(document_id)) {
        throw std::invalid_argument("Nonexistent document id"s);
    }

    SearchServer::PlusMinusWords prepared_query = ParseQuery(raw_query /* is_parallel_need = false */);

    for (std::string_view minus_word : prepared_query.minus_words) {
        if (TF_by_term_.count(minus_word) > 0) {
            if (TF_by_term_.at(minus_word).count(document_id) > 0) {
                return {std::vector<std::string_view>{}, document_info_.at(document_id).status};
            }
        }
    }

    std::set<std::string_view> plus_words_in_document;

    for (std::string_view plus_word : prepared_query.plus_words) {
        if (TF_by_term_.count(plus_word) == 1) {
            if (TF_by_term_.at(plus_word).count(document_id) == 1) {
                plus_words_in_document.insert(plus_word);
            }
        }
    }

    std::vector<std::string_view> result_intersection;
    for (std::string_view word : plus_words_in_document) {
        result_intersection.push_back(word);
    }

    return {result_intersection, document_info_.at(document_id).status};
}

Matching SearchServer::MatchDocument(std::execution::sequenced_policy, std::string_view raw_query, int document_id) const {
    return MatchDocument(raw_query, document_id);
}

Matching SearchServer::MatchDocument(std::execution::parallel_policy, std::string_view raw_query, int document_id) const {

    if (IsNegativeDocumentId(document_id)) {
        throw std::invalid_argument("Negative document id"s);
    }

    if (IsNonExistentDocumentId(document_id)) {
        throw std::invalid_argument("Nonexistent document id"s);
    }

    // тут два вектора с возможно повторяющимися + и - словами
    // true, потому что это параллельная версия MatchDocument; а параллельной MatchDocument
    // параллельная ParseQuery, которая запускается вторым параметром, установленным в true

    SearchServer::PlusMinusWords prepared_query = ParseQuery(raw_query, true);

    auto find_word = [this, document_id](std::string_view word) {
                        return this->GetWordFrequencies(document_id).count(word);
                     };

    bool is_minus_words_in_document = any_of(std::execution::par,
                                             prepared_query.minus_words.begin(), prepared_query.minus_words.end(),
                                             find_word);

    if (is_minus_words_in_document) {
        return {std::vector<std::string_view>{}, document_info_.at(document_id).status};
    }

    // очистим от повторов, а то повторы не свое место займут, которое резервится в result_intersection
    // тогда параллельный алгоритм начнет добавлять элементы и все упадет -- segmentation fault будет
    prepared_query.RemovePlusWordsDublicates();

    std::vector<std::string_view> result_intersection(TF_by_id_.at(document_id).size());
    std::copy_if(std::execution::par,
                 prepared_query.plus_words.begin(), prepared_query.plus_words.end(),
                 result_intersection.begin(),
                 find_word
                );

    std::sort(std::execution::par,
              result_intersection.begin(), result_intersection.end());
    
    // найдем первое не пустое слово
    auto start = std::upper_bound(result_intersection.begin(), result_intersection.end(), ""sv);

    return {{start, result_intersection.end()}, document_info_.at(document_id).status};
}

int SearchServer::GetDocumentCount() const {
    return document_order_.size();
}

const std::map<std::string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    if (TF_by_id_.count(document_id)) {
        return TF_by_id_.at(document_id);
    }
    static const std::map<std::string_view, double> empty_map{};
    return empty_map;
}

void SearchServer::RemoveDocument(int document_id) {
    if (document_order_.count(document_id) == 0) {
        return;
    }

    for (const auto& [word, freq] : TF_by_id_.at(document_id)) {
        TF_by_term_.at(word).erase(document_id);
        
        if (TF_by_term_.at(word).empty()) {
            TF_by_term_.erase(word);
        }
    }

    TF_by_id_.erase(document_id);
    document_info_.erase(document_id);
    document_order_.erase(document_id);

}

void SearchServer::RemoveDocument(std::execution::sequenced_policy, int document_id) {
    RemoveDocument(document_id);
}

void SearchServer::RemoveDocument(std::execution::parallel_policy, int document_id) {
    if (document_order_.count(document_id) == 0) {
        return;
    }

   std::vector<std::string_view> words(TF_by_id_.at(document_id).size());

    std::transform(std::execution::par, TF_by_id_.at(document_id).begin(), TF_by_id_.at(document_id).end(),
                   words.begin(),
                   [](auto& item) { return item.first; });

     std::for_each(std::execution::par, words.begin(), words.end(),
                  [this, document_id](std::string_view word) { (this->TF_by_term_).at(word).erase(document_id); });

    /* это медленно ровно как непараллельная версия, потому что по map параллельные алгоритмы почему-то плохо работают
       поэтому мы выше и делаем вектор (но не строк, а указателей, чтобы не таскать эти строки!)
    
    std::for_each(std::execution::par, TF_by_id_.at(document_id).begin(), TF_by_id_.at(document_id).end(),
                  [this, document_id](const auto item) { (this->TF_by_term_).at(item.first).erase(document_id); }); */

    TF_by_id_.erase(document_id);
    document_info_.erase(document_id);
    document_order_.erase(document_id);
}

bool SearchServer::IsStopWord(std::string_view word) const {
    return stop_words_.count(word) > 0;
}

std::vector<std::string_view> SearchServer::SplitIntoWordsNoStopView(std::string_view text) const {
    std::vector<std::string_view> words;
    for (std::string_view word : SplitIntoWordsView(text)) {
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }

    return words;
}

SearchServer::PlusMinusWords SearchServer::ParseQuery(std::string_view raw_query, bool is_parallel_need) const {

    SearchServer::PlusMinusWords query_words;

    ThrowSpecialSymbolInText(raw_query);

    // это нужно, чтобы и плюс-, и минус-слова в своих векторах были уже отсортированы, потому что далее их ждет unique-erase
    std::vector<std::string_view> splited_query = SplitIntoWordsNoStopView(raw_query);

    for (std::string_view word : splited_query) {
        if (word[0] == '-') {
            auto minus_word = word.substr(1);
            
            if (minus_word.empty() || minus_word[0] == '-') {
                throw std::invalid_argument("Alone or double minus in query"s);
            }

            query_words.minus_words.push_back(minus_word);

        } else {
            query_words.plus_words.push_back(word);
        }
    }

    if (is_parallel_need) {
        return query_words;
    }

    std::vector<std::string_view>::iterator last;

    std::sort(std::execution::par,
              query_words.plus_words.begin(), query_words.plus_words.end());
    // чистим непараллельную версию ParseQuery, ее плюс-слова, от дубликатов
    last = std::unique(query_words.plus_words.begin(), query_words.plus_words.end());
    query_words.plus_words.erase(last, query_words.plus_words.end());

    std::sort(std::execution::par,
              query_words.minus_words.begin(), query_words.minus_words.end());
    // чистим непараллельную версию ParseQuery, ее минус-слова, от дубликатов
    last = std::unique(query_words.minus_words.begin(), query_words.minus_words.end());
    query_words.minus_words.erase(last, query_words.minus_words.end());

    return query_words;
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {

    if (ratings.size() > 0) {
        return std::accumulate(ratings.begin(), ratings.end(), 0.0) / static_cast<int>(ratings.size());
    }
    
    return 0; // если нет оценок, по условию рэйтинг такого документа равен нулю
}

bool SearchServer::IsSpecialSymboslInText(std::string_view text) const {
    return !std::none_of(text.begin(), text.end(), [](char c) { return c >= '\0' && c < ' '; });
}

bool SearchServer::IsNegativeDocumentId(const int document_id) const {
    return document_id < 0;
}

bool SearchServer::IsRecurringDocumentId(const int document_id) const {
    return document_info_.count(document_id);
}

void SearchServer::ThrowSpecialSymbolInText(std::string_view text) const {
    if (IsSpecialSymboslInText(text)) { // это чтобы колбасу лямбды сюда не пихать и + IsSpecialSymboslInText понятнее лямбды, и + делает свою работу
        throw std::invalid_argument("Special symbol in text"s); // а текущая функция свою -- только бросает исключение
    }
}

bool SearchServer::IsNonExistentDocumentId(const int document_id) const {
    return !document_order_.count(document_id);
}

void AddDocument(SearchServer& search_server, int document_id, std::string_view document, const DocumentStatus& status, const std::vector<int>& ratings) {
    search_server.AddDocument(document_id, document, status, ratings);
}

void RemoveDuplicates(SearchServer& search_server) {
    std::map<std::set<std::string_view>, std::set<int>> duplicates;
    for (const int document_id : search_server) {
        std::set<std::string_view> document_words{};
        for (const auto& [word, freq] : search_server.GetWordFrequencies(document_id)) {
            document_words.insert(word);
        }

        duplicates[document_words].insert(document_id);

    }

    for (const auto& [word_set, doc_id_set]: duplicates) {
        auto removed_document = doc_id_set.rbegin();
        while (removed_document != --doc_id_set.rend()) {
            std::cout << "Found duplicate document id "sv << *removed_document << std::endl;
            search_server.RemoveDocument(*removed_document);

            ++removed_document;
        }
    }
}