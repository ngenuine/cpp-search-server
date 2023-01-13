#pragma once
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <set>
#include <deque>
#include <tuple>
#include <map>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <future>
#include <execution>
#include <thread>
#include <atomic>
#include "document.h"
#include "string_processing.h"
#include "paginator.h"
#include "concurrent_map.h"

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

    // конструктор на основе вью с любым количеством пробелов до, между и после слов
    SearchServer(std::string_view stop_words_text);

    void AddDocument(int document_id, std::string_view document, const DocumentStatus& status, const std::vector<int>& ratings);

    // перегрузка FindTopDocuments для передачи в качестве второго параметра функционального объекта
    template <typename Predicate>
    std::vector<Document> FindTopDocuments(std::string_view raw_query, Predicate filter) const;

    // перегрузка FindTopDocuments для передачи политики и в качестве третьего параметра функционального объекта
    template <typename ExecutionPolicy, typename Predicate>
    std::vector<Document> FindTopDocuments(ExecutionPolicy policy, std::string_view raw_query, Predicate filter) const;

    // перегрузка FindTopDocuments для принятия статусов
    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentStatus given_status = DocumentStatus::ACTUAL) const;

    // перегрузка FindTopDocuments для принятия политики и статусов
    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy policy, std::string_view raw_query, DocumentStatus given_status = DocumentStatus::ACTUAL) const;

    int GetDocumentCount() const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::string_view raw_query, int document_id) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::execution::sequenced_policy, std::string_view raw_query, int document_id) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::execution::parallel_policy, std::string_view raw_query, int document_id) const;

    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

    void RemoveDocument(int document_id);

    void RemoveDocument(std::execution::sequenced_policy policy, int document_id);

    void RemoveDocument(std::execution::parallel_policy policy, int document_id);

private:

    struct PlusMinusWords {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;

        void RemovePlusWordsDublicates() {
            std::sort(plus_words.begin(), plus_words.end());
            plus_words.erase(std::unique(plus_words.begin(), plus_words.end()), plus_words.end());
        }
    };

    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    std::deque<std::string> all_data_; // информация о всех (+, -, и documents) словах в объекте SearchServer; хранилище, на которое смотрят вью
    std::map<int, DocumentData> document_info_; // [id -- инфа об id (рейтинг и статус)]
    std::map<std::string_view, std::map<int, double>> TF_by_term_; // слово -- [id -- в котором у этого слова посчитан TF_]
    std::map<int, std::map<std::string_view, double>> TF_by_id_; // TF_ наоборот (не от слова, а от id отталкиваемся)
    const std::set<std::string_view> stop_words_; // все стоп-слова
    std::set<int> document_order_; // какие id вообще есть


    bool IsStopWord(std::string_view word) const;

    std::vector<std::string_view> SplitIntoWordsNoStopView(std::string_view text) const;

    // однопоточная версия ParseQuery
    PlusMinusWords ParseQuery(std::string_view raw_query) const;

    // распараллеленная версия ParseQuery
    PlusMinusWords ParseQuery(std::execution::parallel_policy, std::string_view raw_query) const;

    template <typename Predicate>
    std::vector<Document> FindAllDocuments(const PlusMinusWords& query_words, Predicate filter) const;

    template <typename ExecutionPolicy, typename Predicate>
    std::vector<Document> FindAllDocuments(ExecutionPolicy policy, const PlusMinusWords& query_words, Predicate filter) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);

    bool IsSpecialSymboslInText(std::string_view text) const;

    bool IsNegativeDocumentId(const int document_id) const;

    bool IsNonexistentDocumentId(const int document_id) const;

    bool IsRecurringDocumentId(const int document_id) const;

    void ThrowSpecialSymbolInText(std::string_view text) const;
};

template<typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words) : stop_words_(MakeSetStopWords(stop_words)) {
    for (std::string_view word : stop_words_) {
        ThrowSpecialSymbolInText(word);
    }
}

// функция (не метод), добавляющая документы в указанный в качестве первого параметра сервер
void AddDocument(SearchServer& search_server, int document_id, std::string_view document, const DocumentStatus& status, const std::vector<int>& ratings);

template <typename Predicate>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, Predicate filter) const {
    const PlusMinusWords prepared_query = ParseQuery(raw_query);

    auto matched_documents = FindAllDocuments(prepared_query, filter);

    sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                if (std::abs(lhs.relevance - rhs.relevance) > PRECISE) {
                    return lhs.relevance > rhs.relevance;
                }

                return lhs.rating > rhs.rating;
            }
        );
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }

    return matched_documents;
}

template <typename ExecutionPolicy, typename Predicate>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy policy, std::string_view raw_query, Predicate filter) const {

    if (std::is_same_v<std::execution::sequenced_policy, ExecutionPolicy>) {
        return FindTopDocuments(raw_query, filter);
    }

    // тут два вектора с возможно повторяющимися + и - словами
    SearchServer::PlusMinusWords prepared_query = ParseQuery(std::execution::par, raw_query);

    // очищаем от повтором, потому что релевандность высчитывается на уникальных плюс-словах запроса
    // почему очищаю тут -- да потому что при вызове этой очистки в параллельной ипостаси ParseQuery я получаю непрохождение по времени
    // с соотношением мой код/учителя код = 0.9, а если тут вызываю, то 0.5 и соответственно прохожу по времени
    prepared_query.RemovePlusWordsDublicates();

    auto matched_documents = FindAllDocuments(std::execution::par, prepared_query, filter);

    sort(std::execution::par, matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                if (std::abs(lhs.relevance - rhs.relevance) > PRECISE) {
                    return lhs.relevance > rhs.relevance;
                }

                return lhs.rating > rhs.rating;
            }
        );
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }

    return matched_documents;
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy policy, std::string_view raw_query, DocumentStatus given_status /* = DocumentStatus::ACTUAL */) const {
    if (std::is_same_v<std::execution::sequenced_policy, ExecutionPolicy>) {
        return FindTopDocuments(raw_query,
                                [given_status](int document_id, DocumentStatus status, int rating) {
                                    return status == given_status;
                                });
    }

    return FindTopDocuments(std::execution::par, raw_query,
                                [given_status](int document_id, DocumentStatus status, int rating) {
                                    return status == given_status;
                                });

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

    for (std::string_view word : query_words.plus_words) {
        if (TF_by_term_.count(word) != 0) { // если плюс-слово запроса есть в TF_, значит по TF_.at(плюс-слово запроса) мы получим все id документов, где это слово имеет вес tf, эти документы интересы; а по TF_.at(word).size() поймем, в скольких документах это слово есть.
            
            idf = log(static_cast<double>(document_order_.size()) / TF_by_term_.at(word).size());
            
            for (const auto& [document_id, tf] : TF_by_term_.at(word)) { // будем идти по предпосчитанному TF_.at(плюс-слово запроса) и наращивать релевантность документам по их id по офрмуле IDF-TF.
                const DocumentData& document_data = document_info_.at(document_id);
                if (filter(document_id, document_data.status, document_data.rating)) { // если документ соответсвует предикату, рассчитаем ему релевантность по алгоритму IDF-TF, иначе нет смысла считать, чтобы потом не удалять пусть и релевантные документы, не соответствующие предикату
                    IDF_TF[document_id] += idf * tf;
                }
            }
        }
    }

    // теперь надо пройтись по минус словам и посмотреть при помощи TF_, какие id документов есть по этому слову, и вычеркнуть их из выдачи.

    for (std::string_view word : query_words.minus_words) {
        if (TF_by_term_.count(word) != 0) {
            for (const auto& [documents_id, _] : TF_by_term_.at(word)) {
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

template <typename ExecutionPolicy, typename Predicate>
std::vector<Document> SearchServer::FindAllDocuments(ExecutionPolicy policy, const PlusMinusWords& query_words, Predicate filter) const {

    if (std::is_same_v<std::execution::sequenced_policy, ExecutionPolicy>) {
        return FindAllDocuments(query_words, filter);
    }

    ConcurrentMap<int, double> IDF_TF(157);

    auto calculator = [&IDF_TF, this, &filter](std::string_view word) {
        if (this->TF_by_term_.count(word) != 0) {
            
            double idf = log(static_cast<double>(this->document_order_.size()) / this->TF_by_term_.at(word).size());
            
            for (const auto& [document_id, tf] : this->TF_by_term_.at(word)) {
                const DocumentData& document_data = this->document_info_.at(document_id);
                if (filter(document_id, document_data.status, document_data.rating)) {
                    IDF_TF[document_id].ref_to_value += idf * tf;
                }
            }
        }
    };

    auto eraser = [&IDF_TF, this](std::string_view word) {
        if (this->TF_by_term_.count(word) != 0) {
            for (const auto& [documents_id, _] : TF_by_term_.at(word)) {
                IDF_TF.erase(documents_id);
            }
        }
    };

    for_each(std::execution::par, query_words.plus_words.begin(), query_words.plus_words.end(), calculator);

    for_each(std::execution::par, query_words.minus_words.begin(), query_words.minus_words.end(), eraser);

    std::vector<Document> result;

    for (const auto& [document_id, relevance] : IDF_TF.BuildOrdinaryMap()) {
        result.push_back({document_id, relevance, document_info_.at(document_id).rating});
    }

    return result;
}

void RemoveDuplicates(SearchServer& search_server);