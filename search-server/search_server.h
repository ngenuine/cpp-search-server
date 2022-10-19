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

    // конструктор на основе коллекции vector или set
    template<typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words) : stop_words_(MakeSetStopWords(stop_words)) {
        for (const std::string& word : stop_words) {
            ThrowSpecialSymbolInText(word);
        }
    }

    // конструктор на основе строки с любым количеством пробелов до, между и после слов
    SearchServer(const std::string& stop_words_text);

    void AddDocument(int document_id, const std::string& document, const DocumentStatus& status, const std::vector<int>& ratings);

    // перегрузка FindTopDocuments для передачи в качестве вторго параметра функционального объекта
    template <typename Predicate>
    std::vector<Document> FindTopDocuments(const std::string& raw_query, Predicate filter) const {

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

    // перегрузка FindTopDocuments для принятия статусов
    std::vector<Document> FindTopDocuments(const std::string& raw_query, DocumentStatus given_status = DocumentStatus::ACTUAL) const;

    int GetDocumentCount() const;

    std::tuple<std::vector<std::string>, DocumentStatus> MatchDocument(const std::string& raw_query, int document_id) const;

    int GetDocumentId(int order) const;

private:

    struct PlusMinusWords {
        std::set<std::string> plus_words;
        std::set<std::string> minus_words;
    };

    std::map<int, int> documents_rating_;
    std::map<int, DocumentStatus> document_status_;
    std::map<std::string, std::map<int, double>> TF_;
    const std::set<std::string> stop_words_;
    std::vector<int> document_order_;

    bool IsStopWord(const std::string& word) const;

    std::vector<std::string> SplitIntoWordsNoStop(const std::string& text) const;

    PlusMinusWords ParseQuery(const std::string& raw_query) const;

    template <typename Predicate>
    std::vector<Document> FindAllDocuments(const PlusMinusWords& query_words, Predicate filter) const {

        /* Рассчитываем IDF каждого плюс-слова в запросе:
        1) количество документов document_order_.size() делим на количество документов, где это слово встречается;
        2) берем от полученного значения log.
        Функция AddDocument построила TF_, где каждому слову отнесено множество документов, где оно встречается.
        */

        std::map<std::string, double> IDF; // в результате получим слово из запроса и его посчитанный IDF (не факт, что все слова из запроса обрели IDF, ведь слова может не быть в индексе, а значит знаменателя нет).
        for (const std::string& word : query_words.plus_words) {
            if (TF_.count(word) != 0) { // если слово есть в индексе, значит можно быстро понять, в скольких документах оно есть -- TF_.at(word).size().
                IDF[word] = log(static_cast<double>(document_order_.size()) / TF_.at(word).size());
            }
        }

        std::map<int, double> matched_documents;

        for (const auto& [word, idf] : IDF) { // раз мы идем здесь по словарю IDF, значит мы идем по плюс-словам запроса.
            if (TF_.count(word) != 0) { // если плюс-слово запроса есть в TF_, значит по TF_.at(плюс-слово запроса) мы получим все id документов, где это слово имеет вес tf, эти документы интересы.
                for (const auto& [document_id, tf] : TF_.at(word)) { // будем идти по предпосчитанному TF_.at(плюс-слово запроса) и наращивать релевантность документам по их id по офрмуле IDF-TF.
                    matched_documents[document_id] += idf * tf; 
                }
            }
        }

        /* теперь надо пройтись по минус словам и посмотреть при помощи TF_,
        какие id документов есть по этому слову, и вычеркнуть их из выдачи.
        */

        for (const std::string& word : query_words.minus_words) {
            if (TF_.count(word) != 0) {
                for (const auto& [documents_id, _] : TF_.at(word)) {
                    if (matched_documents.count(documents_id) != 0) {
                        matched_documents.erase(documents_id);
                    }

                }
            }
        }

        /* и еще пройтись filter, чтобы соответствовали запросу: id, status, rating */

        std::vector<int> for_erase;
        for (const auto& [document_id, relevance] : matched_documents) {
            if (!filter(document_id, document_status_.at(document_id), documents_rating_.at(document_id))) {
                for_erase.push_back(document_id);
            }
        }

        for (int document_id: for_erase) {
            matched_documents.erase(document_id);
        }

        std::vector<Document> result;

        for (const auto& [document_id, relevance] : matched_documents) {
            result.push_back({document_id, relevance, documents_rating_.at(document_id)});
        }

        return result;
    }

    static int ComputeAverageRating(const std::vector<int>& ratings);

    bool IsSpecialSymboslInText(const std::string& text) const;

    bool IsNegativeDocumentId(const int document_id) const;

    bool IsRecurringDocumentId(const int document_id) const;

    void ThrowSpecialSymbolInText(const std::string& text) const;
};