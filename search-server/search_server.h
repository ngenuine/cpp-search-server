#pragma once
#include <stdexcept>
#include <string>
#include <vector>
#include <set>
#include <tuple>
#include <map>
#include <cmath>
#include <algorithm>
#include <numeric>
#include "document.h"
#include "string_processing.h"

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const int PRECISE = 1e-06;

using namespace std::literals;


class SearchServer {

public:

    std::set<int>::const_iterator begin() const;
    std::set<int>::const_iterator end() const;

    // конструктор на основе коллекции vector или set
    template<typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);

    // конструктор на основе строки с любым количеством пробелов до, между и после слов
    SearchServer(const std::string& stop_words_text);

    void AddDocument(int document_id, const std::string& document, const DocumentStatus& status, const std::vector<int>& ratings);

    // перегрузка FindTopDocuments для передачи в качестве вторго параметра функционального объекта
    template <typename Predicate>
    std::vector<Document> FindTopDocuments(const std::string& raw_query, Predicate filter) const;

    // перегрузка FindTopDocuments для принятия статусов
    std::vector<Document> FindTopDocuments(const std::string& raw_query, DocumentStatus given_status = DocumentStatus::ACTUAL) const;

    int GetDocumentCount() const;

    std::tuple<std::vector<std::string>, DocumentStatus> MatchDocument(const std::string& raw_query, int document_id) const;

    const std::map<std::string, double>& GetWordFrequencies(int document_id) const;

    void RemoveDocument(int document_id);

private:

    struct PlusMinusWords {
        std::set<std::string> plus_words;
        std::set<std::string> minus_words;
    };

    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    std::map<int, DocumentData> document_info_;
    std::map<std::string, std::map<int, double>> TF_;
    std::map<int, std::map<std::string, double>> word_frequencies_by_document_id_;
    const std::set<std::string> stop_words_;
    std::set<int> document_order_;

    bool IsStopWord(const std::string& word) const;

    std::vector<std::string> SplitIntoWordsNoStop(const std::string& text) const;

    PlusMinusWords ParseQuery(const std::string& raw_query) const;

    template <typename Predicate>
    std::vector<Document> FindAllDocuments(const PlusMinusWords& query_words, Predicate filter) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);

    bool IsSpecialSymboslInText(const std::string& text) const;

    bool IsNegativeDocumentId(const int document_id) const;

    bool IsRecurringDocumentId(const int document_id) const;

    void ThrowSpecialSymbolInText(const std::string& text) const;
};

void AddDocument(SearchServer& search_server, int document_id, const std::string& document, const DocumentStatus& status, const std::vector<int>& ratings);

template<typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words) : stop_words_(MakeSetStopWords(stop_words)) {
    for (const std::string& word : stop_words) {
        ThrowSpecialSymbolInText(word);
    }
}

template <typename Predicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, Predicate filter) const {

    const PlusMinusWords prepared_query = ParseQuery(raw_query);

    auto matched_documents = FindAllDocuments(prepared_query, filter);

    sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
            if (std::abs(lhs.relevance - rhs.relevance) > PRECISE) {
                return lhs.relevance > rhs.relevance;
            }
            return lhs.rating > rhs.rating;
            });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }

    return matched_documents;
}

template <typename Predicate>
std::vector<Document> SearchServer::FindAllDocuments(const PlusMinusWords& query_words, Predicate filter) const {

    /* Рассчитываем IDF каждого плюс-слова в запросе:
    1) количество документов document_order_.size() делим на количество документов, где это слово встречается;
    2) берем от полученного значения log.
    Функция AddDocument построила TF_, где каждому слову отнесено множество документов, где оно встречается.
    */

    double idf;
    std::map<int, double> IDF_TF; // в результате получим соответствие документ -- его релевантность, посчитанная по алгоритму IDF-TF.

    for (const std::string& word : query_words.plus_words) {
        if (TF_.count(word) != 0) { // если плюс-слово запроса есть в TF_, значит по TF_.at(плюс-слово запроса) мы получим все id документов, где это слово имеет вес tf, эти документы интересы; а по TF_.at(word).size() поймем, в скольких документах это слово есть.
            
            idf = log(static_cast<double>(document_order_.size()) / TF_.at(word).size());
            
            for (const auto& [document_id, tf] : TF_.at(word)) { // будем идти по предпосчитанному TF_.at(плюс-слово запроса) и наращивать релевантность документам по их id по офрмуле IDF-TF.
                const DocumentData& document_data = document_info_.at(document_id);
                if (filter(document_id, document_data.status, document_data.rating)) { // если документ соответсвует предикату, рассчитаем ему релевантность по алгоритму IDF-TF, иначе нет смысла считать, чтобы потом не удалять пусть и релевантные документы, не соответствующие предикату
                    IDF_TF[document_id] += idf * tf; 
                }
            }
        }
    }

    /* теперь надо пройтись по минус словам и посмотреть при помощи TF_,
    какие id документов есть по этому слову, и вычеркнуть их из выдачи.
    */

    for (const std::string& word : query_words.minus_words) {
        if (TF_.count(word) != 0) {
            for (const auto& [documents_id, _] : TF_.at(word)) {
                IDF_TF.erase(documents_id);
            }
        }
    }

    std::vector<Document> result;

    for (const auto& [document_id, relevance] : IDF_TF) {
        result.push_back({document_id, relevance, document_info_.at(document_id).rating});
    }

    return result;
}