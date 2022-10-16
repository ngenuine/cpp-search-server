#include <algorithm>
#include <iostream>
#include <set>
#include <map>
#include <string>
#include <vector>
#include <cmath>
#include <numeric>
#include <tuple>
#include <stdexcept>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const int PRECISE = 1e-06;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result = 0;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {

    vector<string> words;
    string word;
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

struct Document {

    Document() = default;

    Document(const int given_id, const double given_relevance, const int given_rating)
        : id(given_id)
        , relevance(given_relevance)
        , rating(given_rating) {
    }

    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

enum class DocumentStatus {
        ACTUAL,
        IRRELEVANT,
        BANNED,
        REMOVED,
    };

template <typename StringContainer>
set<string> MakeSetStopWords(const StringContainer& strings) {
    set<string> non_empty_strings;
    for (const string& str : strings) {
        if (!str.empty()) {
            non_empty_strings.insert(str);
        }
    }
    return non_empty_strings;
}

class SearchServer {

public:

    // конструктор на основе коллекции vector или set
    template<typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words)
        : stop_words_(MakeSetStopWords(stop_words)) {
            for (const string& word : stop_words) {
                ThrowSpecialSymbolInText(word);
            }
    }

    // конструктор на основе строки с любым количеством пробелов до, между и после слов
    SearchServer(const string& stop_words_text)
        : SearchServer(SplitIntoWords(stop_words_text)) {
    }

    void AddDocument(int document_id, const string& document, const DocumentStatus& status, const vector<int>& ratings) {
        
        ThrowSpecialSymbolInText(document);

        if (IsNegativeDocumentId(document_id)) {
            throw invalid_argument("Negative document id"s);
        }

        if (IsRecurringDocumentId(document_id)) {
            throw invalid_argument("Recurring document id"s);
        }

        // наполняем счетчик документов ( -- он пригодится для подсчета IDF.
        // одновременно и порядок добавления получаем
        document_order_.push_back(document_id); 

        document_status_[document_id] = status;

        const vector<string> words = SplitIntoWordsNoStop(document);

        documents_rating_[document_id] = ComputeAverageRating(ratings);

        
        for (const string& word : words) {
            TF_[word][document_id] += 1.0 / words.size(); // Рассчитываем TF каждого слова в каждом документе.
        }
    }

    // перегрузка FindTopDocuments для передачи в качестве вторго параметра функционального объекта
    template <typename Predicate>
    vector<Document> FindTopDocuments(const string& raw_query, Predicate filter) const {

        const PlusMinusWords prepared_query = ParseQuery(raw_query);

        auto matched_documents = FindAllDocuments(prepared_query, filter);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) > PRECISE) {
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
    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus given_status = DocumentStatus::ACTUAL) const {
        return FindTopDocuments(raw_query,
                                [given_status](int document_id, DocumentStatus status, int rating) {return status == given_status;});
    }

    int GetDocumentCount() const {
        return document_order_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        
        PlusMinusWords prepared_query = ParseQuery(raw_query);

        for (const string& minus_word : prepared_query.minus_words) {
            if (TF_.count(minus_word) == 1) {
                if (TF_.at(minus_word).count(document_id) == 1) {
                    return {vector<string>{}, document_status_.at(document_id)};
                }
            }
        }

        set<string> plus_words_in_document;

        for (const string& plus_word : prepared_query.plus_words) {
            if (TF_.count(plus_word) == 1) {
                if (TF_.at(plus_word).count(document_id) == 1) {
                    plus_words_in_document.insert(plus_word);
                }
            }
        }

        vector<string> result_intersection;
        for (const string& word : plus_words_in_document) {
            result_intersection.push_back(word);
        }

        return {result_intersection, document_status_.at(document_id)};

    }

    int GetDocumentId(int order) const {
        if (order < 0 || order >= static_cast<int>(document_order_.size())) {
            throw out_of_range("Order of document out of range");
        }

        return document_order_[order];
    }

private:

    struct PlusMinusWords {
        set<string> plus_words;
        set<string> minus_words;
    };

    map<int, int> documents_rating_;
    map<int, DocumentStatus> document_status_;
    map<string, map<int, double>> TF_;
    const set<string> stop_words_;
    vector<int> document_order_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    PlusMinusWords ParseQuery(const string& raw_query) const {

        PlusMinusWords query_words;

        ThrowSpecialSymbolInText(raw_query);
        
        for (const string& word : SplitIntoWordsNoStop(raw_query)) {
            if (word[0] == '-') {
                auto prepared_word = word.substr(1);
                
                if (prepared_word.empty() || prepared_word[0] == '-') {
                    throw invalid_argument("Alone or double minus in query"s);
                }

                query_words.minus_words.insert(prepared_word);
            } else if (query_words.minus_words.count(word) == 0) {
                query_words.plus_words.insert(word);
            }
        }

        return query_words;
    }

    template <typename Predicate>
    vector<Document> FindAllDocuments(const PlusMinusWords& query_words, Predicate filter) const {

        /* Рассчитываем IDF каждого плюс-слова в запросе:
        1) количество документов document_order_.size() делим на количество документов, где это слово встречается;
        2) берем от полученного значения log.
        Функция AddDocument построила TF_, где каждому слову отнесено множество документов, где оно встречается.
        */

        map<string, double> IDF; // в результате получим слово из запроса и его посчитанный IDF (не факт, что все слова из запроса обрели IDF, ведь слова может не быть в индексе, а значит знаменателя нет).
        for (const string& word : query_words.plus_words) {
            if (TF_.count(word) != 0) { // если слово есть в индексе, значит можно быстро понять, в скольких документах оно есть -- TF_.at(word).size().
                IDF[word] = log(static_cast<double>(document_order_.size()) / TF_.at(word).size());
            }
        }

        map<int, double> matched_documents;

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

        for (const string& word : query_words.minus_words) {
            if (TF_.count(word) != 0) {
                for (const auto& [documents_id, _] : TF_.at(word)) {
                    if (matched_documents.count(documents_id) != 0) {
                        matched_documents.erase(documents_id);
                    }

                }
            }
        }

        /* и еще пройтись filter, чтобы соответствовали запросу: id, status, rating */

        vector<int> for_erase;
        for (const auto& [document_id, relevance] : matched_documents) {
            if (!filter(document_id, document_status_.at(document_id), documents_rating_.at(document_id))) {
                for_erase.push_back(document_id);
            }
        }

        for (int document_id: for_erase) {
            matched_documents.erase(document_id);
        }

        vector<Document> result;

        for (const auto& [document_id, relevance] : matched_documents) {
            result.push_back({document_id, relevance, documents_rating_.at(document_id)});
        }

        return result;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {

        if (ratings.size() > 0) {
            return accumulate(ratings.begin(), ratings.end(), 0.0) / static_cast<int>(ratings.size());
        }
        return 0; // если нет оценок, по условию рэйтинг такого документа равен нулю
    }

    bool IsSpecialSymboslInText(const string& text) const {
        int length = text.length();
        for (int i = 0; i < length; ++i) {
            int c = text[i];
            if (c >= 0 && c <= 31) {
                return true;
            }
        }

        return false;
    }

    bool IsNegativeDocumentId(const int document_id) const {
        return document_id < 0;
    }

    bool IsRecurringDocumentId(const int document_id) const {
        return documents_rating_.count(document_id);
    }

    void ThrowSpecialSymbolInText(const string& text) const {
        if (IsSpecialSymboslInText(text)) {
            throw invalid_argument("Special symbol in text"s);
        }
    }
};

template <typename Iterator>
class IteratorRange {
public:

    IteratorRange(Iterator begin, Iterator end) : begin_(begin), end_(end) {}

    Iterator begin() const { return begin_; }
    Iterator end() const { return end_; }
    int size() const { return distance(end_, begin_); }

private:
    const Iterator begin_;
    const Iterator end_;
};

template <typename Iterator>
class Paginator {
public:
    Paginator(Iterator range_begin, Iterator range_end, int page_size) {

        auto page_start = range_begin;

        while (page_start != range_end) {
            
            auto page_end = page_start;
            if (distance(page_start, range_end) >= page_size) {
                advance(page_end, page_size);
            } else {
                page_end = range_end;
            }

            pages_.push_back(IteratorRange(page_start, page_end));
            
            page_start = page_end;
        }
    }

    auto begin() const { return pages_.begin(); }

    auto end() const { return pages_.end(); }

private:
    vector<IteratorRange<Iterator>> pages_;
};

void PrintDocument(const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating << " }"s << endl;
}

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}

ostream& operator<<(ostream& output, const Document& doc) {

    output
    << "{ document_id = "s << doc.id
    << ", relevance = "s << doc.relevance
    << ", rating = " << doc.rating << " }";

    return output;
}

template<typename Iterator>
ostream& operator<<(ostream& output, const IteratorRange<Iterator>& page) {

    for (auto it = page.begin(); it != page.end(); ++it)
    output << *it;

    return output;
}


int main() {
    SearchServer search_server("and with"s);
    search_server.AddDocument(1, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "funny pet with curly hair"s, DocumentStatus::ACTUAL, {1, 2, 3});
    search_server.AddDocument(3, "big cat nasty hair"s, DocumentStatus::ACTUAL, {1, 2, 8});
    search_server.AddDocument(4, "big dog cat Vladislav"s, DocumentStatus::ACTUAL, {1, 3, 2});
    search_server.AddDocument(5, "big dog hamster Borya"s, DocumentStatus::ACTUAL, {1, 1, 1});
    const auto search_results = search_server.FindTopDocuments("curly dog"s);
    int page_size = 2;
    const auto pages = Paginate(search_results, page_size);
    // Выводим найденные документы по страницам
    for (auto page = pages.begin(); page != pages.end(); ++page) {
        cout << *page << endl;
        cout << "Page break"s << endl;
    }
}