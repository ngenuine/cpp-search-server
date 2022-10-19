#include "search_server.h"

SearchServer::SearchServer(const std::string& stop_words_text) : SearchServer(SplitIntoWords(stop_words_text)) {}

void SearchServer::AddDocument(int document_id, const std::string& document, const DocumentStatus& status, const std::vector<int>& ratings) {
        
    ThrowSpecialSymbolInText(document);

    if (IsNegativeDocumentId(document_id)) {
        throw std::invalid_argument("Negative document id"s);
    }

    if (IsRecurringDocumentId(document_id)) {
        throw std::invalid_argument("Recurring document id"s);
    }

    // наполняем счетчик документов ( -- он пригодится для подсчета IDF.
    // одновременно и порядок добавления получаем
    document_order_.push_back(document_id); 

    document_status_[document_id] = status;

    const std::vector<std::string> words = SplitIntoWordsNoStop(document);

    documents_rating_[document_id] = ComputeAverageRating(ratings);

    
    for (const std::string& word : words) {
        TF_[word][document_id] += 1.0 / words.size(); // Рассчитываем TF каждого слова в каждом документе.
    }
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, DocumentStatus given_status /* = DocumentStatus::ACTUAL */) const {
    return FindTopDocuments(raw_query,
                            [given_status](int document_id, DocumentStatus status, int rating) {
                                return status == given_status;
                            });
}

std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(const std::string& raw_query, int document_id) const {
        
    SearchServer::PlusMinusWords prepared_query = ParseQuery(raw_query);

    for (const std::string& minus_word : prepared_query.minus_words) {
        if (TF_.count(minus_word) == 1) {
            if (TF_.at(minus_word).count(document_id) == 1) {
                return {std::vector<std::string>{}, document_status_.at(document_id)};
            }
        }
    }

    std::set<std::string> plus_words_in_document;

    for (const std::string& plus_word : prepared_query.plus_words) {
        if (TF_.count(plus_word) == 1) {
            if (TF_.at(plus_word).count(document_id) == 1) {
                plus_words_in_document.insert(plus_word);
            }
        }
    }

    std::vector<std::string> result_intersection;
    for (const std::string& word : plus_words_in_document) {
        result_intersection.push_back(word);
    }

    return {result_intersection, document_status_.at(document_id)};

}

int SearchServer::GetDocumentCount() const {
    return document_order_.size();
}

int SearchServer::GetDocumentId(int order) const {
    if (order < 0 || order >= static_cast<int>(document_order_.size())) {
        throw std::out_of_range("Order of document out of range");
    }

    return document_order_[order];
}

bool SearchServer::IsStopWord(const std::string& word) const {
    return stop_words_.count(word) > 0;
}

std::vector<std::string> SearchServer::SplitIntoWordsNoStop(const std::string& text) const {
    std::vector<std::string> words;
    for (const std::string& word : SplitIntoWords(text)) {
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }

    return words;
}

SearchServer::PlusMinusWords SearchServer::ParseQuery(const std::string& raw_query) const {

    SearchServer::PlusMinusWords query_words;

    ThrowSpecialSymbolInText(raw_query);
    
    for (const std::string& word : SplitIntoWordsNoStop(raw_query)) {
        if (word[0] == '-') {
            auto prepared_word = word.substr(1);
            
            if (prepared_word.empty() || prepared_word[0] == '-') {
                throw std::invalid_argument("Alone or double minus in query"s);
            }

            query_words.minus_words.insert(prepared_word);
        } else if (query_words.minus_words.count(word) == 0) {
            query_words.plus_words.insert(word);
        }
    }

    return query_words;
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {

    if (ratings.size() > 0) {
        return std::accumulate(ratings.begin(), ratings.end(), 0.0) / static_cast<int>(ratings.size());
    }
    
    return 0; // если нет оценок, по условию рэйтинг такого документа равен нулю
}

bool SearchServer::IsSpecialSymboslInText(const std::string& text) const {
    int length = text.length();
    for (int i = 0; i < length; ++i) {
        int c = text[i];
        if (c >= 0 && c <= 31) {
            return true;
        }
    }

    return false;
}

bool SearchServer::IsNegativeDocumentId(const int document_id) const {
    return document_id < 0;
}

bool SearchServer::IsRecurringDocumentId(const int document_id) const {
    return documents_rating_.count(document_id);
}

void SearchServer::ThrowSpecialSymbolInText(const std::string& text) const {
    if (IsSpecialSymboslInText(text)) {
        throw std::invalid_argument("Special symbol in text"s);
    }
}