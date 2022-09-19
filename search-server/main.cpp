//#include "search_server.h"

#include <iostream>
#include <numeric>
#include <tuple>
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

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

template<typename K, typename V>
ostream& operator<<(ostream& output, const pair<K, V>& p) {

    output << p.first << ": " << p.second;

    return output;
}

template<typename ContainerType>
void PrintContainer(ostream& output, const ContainerType& container) {
    bool is_first = true;
    for (auto elem : container) {
        
        if (!is_first) {
            output << ", ";
        }

        is_first = false;

        output << elem;
    }
}

template<typename K, typename V>
ostream& operator<<(ostream& output, const map<K, V>& m) {

    output << '{';
    PrintContainer(output, m);
    output << '}';

    return output;
}

template<typename T>
ostream& operator<<(ostream& output, const vector<T>& v) {

    output << '[';
    PrintContainer(output, v);
    output << ']';

    return output;
}

template<typename T>
ostream& operator<<(ostream& output, const set<T>& s) {

    output << '{';
    PrintContainer(output, s);
    output << '}';

    return output;
}

class SearchServer {

public:
    void SetStopWords(const string& text) {

        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, const DocumentStatus& status, const vector<int> ratings) {

        // наполняем счетчик документов -- он пригодится для подсчета IDF.
        ++document_count_;

        document_status_[document_id] = status;

        const vector<string> words = SplitIntoWordsNoStop(document);

        documents_rating_[document_id] = ComputeAverageRating(ratings);

        
        for (const string& word : words) {
            TF_[word][document_id] += 1.0 / words.size(); // Рассчитываем TF каждого слова в каждом документе.
            index_[word].insert(document_id); // заодно создаем индекс, где по слову можно будет узнать, в каких документах слово есть.
        }
    }

    // перегрузка FindTopDocuments для передачи в качестве вторго параметра функционального объекта
    template <typename T>
    vector<Document> FindTopDocuments(const string& raw_query, T filter) const {

        const PlusMinusWords query_words = ParseQuery(raw_query);

        auto matched_documents = FindAllDocuments(query_words, filter);

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
        vector<Document> complete_search = FindTopDocuments(raw_query, [given_status](int document_id, DocumentStatus status, int rating) { return status == given_status; });
    
        return complete_search;
    }

    int GetDocumentCount() const {
        return document_count_;
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        
        PlusMinusWords prepared_query = ParseQuery(raw_query);

        for (const string& minus_word : prepared_query.minus_words) {
            if (index_.count(minus_word) == 1) {
                if (index_.at(minus_word).count(document_id) == 1) {
                    return {vector<string>{}, document_status_.at(document_id)}; 
                }
            }
        }

        set<string> plus_words_in_document;

        for (const string& plus_word : prepared_query.plus_words) {
            if (index_.count(plus_word) == 1) {
                if (index_.at(plus_word).count(document_id) == 1) {
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

private:

    struct PlusMinusWords {
        set<string> plus_words;
        set<string> minus_words;
    };

    int document_count_ = 0;

    map<int, int> documents_rating_;

    map<int, DocumentStatus> document_status_;

    map<string, map<int, double>> TF_;

    map<string, set<int>> index_;

    set<string> stop_words_;

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

    PlusMinusWords ParseQuery(const string& text) const {

        PlusMinusWords query_words;

        for (const string& word : SplitIntoWordsNoStop(text)) {
            if (word[0] == '-') {
                query_words.minus_words.insert(word.substr(1));
            } else {
                if (query_words.minus_words.count(word) == 0) {
                    query_words.plus_words.insert(word);
                }
            }
        }
        return query_words;
    }

    template <typename T>
    vector<Document> FindAllDocuments(const PlusMinusWords& query_words, T filter) const {

        /* Рассчитываем IDF каждого плюс-слова в запросе:
        1) количество документов document_count_ делим на количество документов, где это слово встречается;
        2) берем от полученного значения log.
        Функция AddDocument построила index_, где каждому слову отнесено множество документов, где оно встречается.
        */

        map<string, double> idf; // в результате получим слово из запроса и его посчитанный IDF (не факт, что все слова из запроса обрели IDF, ведь слова может не быть в индексе, а значит знаменателя нет).
        for (const string& word : query_words.plus_words) {
            if (index_.count(word) != 0) { // если слово есть в индексе, значит можно быстро понять, в скольких документах оно есть -- index_.at(word).size().
                idf[word] = log(static_cast<double>(document_count_) / index_.at(word).size());
            }
        }

        map<int, double> matched_documents;

        for (const auto& [word, idf] : idf) { // раз мы идем здесь по словарю idf, значит мы идем по плюс-словам запроса.
            if (TF_.count(word) != 0) { // если плюс-слово запроса есть в TF_, значит по TF_.at(плюс-слово запроса) мы получим все id документов, где это слово имеет вес tf, эти документы интересы.
                for (const auto& [document_id, tf] : TF_.at(word)) { // будем идти по предпосчитанному TF_.at(плюс-слово запроса) и наращивать релевантность документам по их id по офрмуле IDF-TF.
                    matched_documents[document_id] += idf * tf; 
                }
            }
        }

        /* теперь надо пройтись по минус словам и посмотреть при помощи index_,
        какие id документов есть по этому слову, и вычеркнуть их из выдачи.
        */

        for (const string& word : query_words.minus_words) {
            if (index_.count(word) != 0) {
                for (const auto& documents_id : index_.at(word)) {
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
                matched_documents.erase(document_id);
            }
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
};

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
                     const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cerr << boolalpha;
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
                const string& hint) {
    if (!value) {
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename ReturnedType>
void RunTestImpl(ReturnedType func, const string& func_name) {
    func();
    cerr << func_name << " OK"s << endl;
}

#define RUN_TEST(func) RunTestImpl((func), #func)

// -------- Начало модульных тестов поисковой системы ----------

void TestAddingDocuments() {
    const int id1 = 35;
    const string content1 = "spider man and doctor stiven strange with hulk"s;
    const vector<int> ratings1 = {4, 5, 6, 5};

    {
        SearchServer search_server;
        search_server.AddDocument(id1, content1, DocumentStatus::ACTUAL, ratings1);
        vector<Document> answer = search_server.FindTopDocuments("stiven"s);
        ASSERT_EQUAL_HINT(answer.size(), 1, "Size of search results should be 1");
        ASSERT_EQUAL(answer[0].id, id1);
    }
}

void TestExcludingStopWords() {
    const int id1 = 35;
    const string content1 = "spider man and doctor stiven strange with hulk"s;
    const vector<int> ratings1 = {4, 5, 6, 5};

    {
        SearchServer search_server;
        search_server.SetStopWords("with and");
        search_server.AddDocument(id1, content1, DocumentStatus::ACTUAL, ratings1);
        ASSERT_HINT(search_server.FindTopDocuments("and with"s).empty(), "Search result should be empty");
    }
}

void TestExcludingMinusWords() {
    const int id1 = 35;
    const string content1 = "spider man and doctor stiven strange with hulk"s;
    const vector<int> ratings1 = {4, 5, 6, 5};

    const int id2 = 45;
    const string content2 = "spider man and doctor stiven strange with neo"s;
    const vector<int> ratings2 = {4, 5, 1};

    {
        SearchServer search_server;
        search_server.AddDocument(id1, content1, DocumentStatus::ACTUAL, ratings1);
        search_server.AddDocument(id2, content2, DocumentStatus::ACTUAL, ratings2);
        vector<Document> answer = search_server.FindTopDocuments("spider man -hulk"s);
        ASSERT_EQUAL_HINT(answer.size(), 1, "Search engine doesn't exclude query witn minus-words");
        ASSERT_EQUAL(answer[0].id, id2);
    }
}

void TestMatchingDocument() {
    const int id1 = 35;
    const string content1 = "spider man and doctor stiven strange with hulk"s;
    const vector<int> ratings1 = {4, 5, 6, 5};

    {
        SearchServer search_server;
        search_server.AddDocument(id1, content1, DocumentStatus::ACTUAL, ratings1);
        tuple<vector<string>, DocumentStatus> answer1 = search_server.MatchDocument("spider man -hulk"s, id1);
        ASSERT_EQUAL(get<0>(answer1).size(), 0);
        
        tuple<vector<string>, DocumentStatus> answer2 = search_server.MatchDocument("spider hulk"s, id1);
        const vector<string> intersection = {"hulk"s, "spider"s};
        ASSERT_EQUAL(get<0>(answer2), intersection);
    }
}

void TestSortingByRelevance() {
    
    {
        const int id1 = 3;
        const string content1 = "spider man and doctor stiven strange with hulk"s;
        const vector<int> ratings1 = {4, 5, 6, 5};
        const int id2 = 2;
        const string content2 = "scooby dooby man our pretty fan you should finger flip pa-pa-pam"s;
        const vector<int> ratings2 = {1, 2, 4};
        const int id3 = 1;
        const string content3 = "pretty woman with hulk"s;
        const vector<int> ratings3 = {4, 4, 4};

        SearchServer search_server;
        search_server.AddDocument(id1, content1, DocumentStatus::ACTUAL, ratings1);
        search_server.AddDocument(id2, content2, DocumentStatus::ACTUAL, ratings2);
        search_server.AddDocument(id3, content3, DocumentStatus::ACTUAL, ratings3);

        vector<Document> answer = search_server.FindTopDocuments("spider man and hulk"s);
        ASSERT_EQUAL(answer[0].id, id1);
        ASSERT_EQUAL(answer[1].id, id3);
        ASSERT_EQUAL(answer[2].id, id2);
    }
}

void TestSortingByRating() {

    {
        const int id1 = 3;
        const string content1 = "spider man and doctor stiven strange with hulk"s;
        const vector<int> ratings1 = {};
        const int id2 = 15;
        const string content2 = "spider man and doctor stiven strange with hulk"s;
        const vector<int> ratings2 = {100};
        const int id3 = 400;
        const string content3 = "spider man and doctor stiven strange with hulk"s;
        const vector<int> ratings3 = {500};

        SearchServer search_server;
        search_server.AddDocument(id1, content1, DocumentStatus::ACTUAL, ratings1);
        search_server.AddDocument(id2, content2, DocumentStatus::ACTUAL, ratings2);
        search_server.AddDocument(id3, content3, DocumentStatus::ACTUAL, ratings3);

        vector<Document> answer = search_server.FindTopDocuments("spider scooby pretty"s);
        ASSERT_EQUAL(answer[0].id, id3);
        ASSERT_EQUAL(answer[1].id, id2);
        ASSERT_EQUAL(answer[2].id, id1);
    }
}

void TestCalculateRating() {
    const int id1 = 3;
    const string content1 = "spider man and doctor stiven strange with hulk"s;
    const vector<int> ratings1 = {4, 5, 6, 5};
    const int id2 = 2;
    const string content2 = "black cat and harry potter play hard"s;
    const vector<int> ratings2 = {};

    const int ZERO_RATING = 0;

    int expected_rating1 = (4 + 5 + 6 + 5) / 4;
    int expected_rating2 = ZERO_RATING;

    {
        SearchServer search_server;
        search_server.AddDocument(id1, content1, DocumentStatus::ACTUAL, ratings1);
        search_server.AddDocument(id2, content2, DocumentStatus::ACTUAL, ratings2);

        vector<Document> answer = search_server.FindTopDocuments("spider man and hulk"s);
        ASSERT_EQUAL(answer[0].rating, expected_rating1);
        ASSERT_EQUAL(answer[1].rating, expected_rating2);
    }
}

void TestPredicateAsFilter() {
    const int id1 = 3;
    const string content1 = "spider man and doctor stiven strange with hulk"s;
    const vector<int> ratings1 = {4, 5, 6, 5};
    const int id2 = 2;
    const string content2 = "scooby dooby man our pretty fan you should finger flip pa-pa-pam"s;
    const vector<int> ratings2 = {1, 2, 4};
    const int id3 = 1;
    const string content3 = "pretty woman with hulk"s;
    const vector<int> ratings3 = {4, 4, 4};

    {
        SearchServer search_server;
        search_server.AddDocument(id1, content1, DocumentStatus::ACTUAL, ratings1);
        search_server.AddDocument(id2, content2, DocumentStatus::ACTUAL, ratings2);
        search_server.AddDocument(id3, content3, DocumentStatus::ACTUAL, ratings3);

        auto predicate = [](const int& id, const DocumentStatus& status, const int& rating) {
            return id > 2;
        };

        vector<Document> answer = search_server.FindTopDocuments("spider man and hulk"s, predicate);
        ASSERT_EQUAL(answer.size(), 1);
    }
}

void TestGivenStatusAsFilter() {
    const int id1 = 3;
    const string content1 = "spider man and doctor stiven strange with hulk"s;
    const vector<int> ratings1 = {4, 5, 6, 5};
    const int id2 = 2;
    const string content2 = "scooby dooby man our pretty fan you should finger flip pa-pa-pam"s;
    const vector<int> ratings2 = {1, 2, 4};
    const int id3 = 1;
    const string content3 = "pretty woman with hulk"s;
    const vector<int> ratings3 = {4, 4, 4};

    {
        SearchServer search_server;
        search_server.AddDocument(id1, content1, DocumentStatus::IRRELEVANT, ratings1);
        search_server.AddDocument(id2, content2, DocumentStatus::BANNED, ratings2);
        search_server.AddDocument(id3, content3, DocumentStatus::ACTUAL, ratings3);

        vector<Document> answer1 = search_server.FindTopDocuments("spider man and hulk"s, DocumentStatus::IRRELEVANT);
        ASSERT_EQUAL(answer1[0].id, id1);
    }
}

void TestIsCorrectRelevance() {
    const int id1 = 3;
    const string content1 = "spider man and doctor stiven strange with hulk"s;
    const vector<int> ratings1 = {4, 5, 6, 5};
    const int id2 = 2;
    const string content2 = "scooby dooby man our pretty fan you should finger flip pa-pa-pam"s;
    const vector<int> ratings2 = {1, 2, 4};
    const int id3 = 1;
    const string content3 = "pretty woman with hulk"s;
    const vector<int> ratings3 = {4, 4, 4};

    {
        SearchServer search_server;
        search_server.AddDocument(id1, content1, DocumentStatus::ACTUAL, ratings1);
        search_server.AddDocument(id2, content2, DocumentStatus::ACTUAL, ratings2);
        search_server.AddDocument(id3, content3, DocumentStatus::ACTUAL, ratings3);

        const int precise = 10e-6;

        vector<Document> answer = search_server.FindTopDocuments("spider man and hulk"s);

        const double expected_relevance0 = 0.3760;
        const double expected_relevance1 = 0.1014;
        const double expected_relevance2 = 0.0369;

        auto approximator = [](const double relevance, const double expected_relevance) {
            return (round(relevance * 10000) / 10000) - expected_relevance;
        };

        ASSERT(approximator(answer[0].relevance, expected_relevance0) <= precise);
        ASSERT(approximator(answer[1].relevance, expected_relevance1) <= precise);
        ASSERT(approximator(answer[2].relevance, expected_relevance2) <= precise);
    }
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestAddingDocuments);
    RUN_TEST(TestExcludingStopWords);
    RUN_TEST(TestExcludingMinusWords);
    RUN_TEST(TestMatchingDocument);
    RUN_TEST(TestSortingByRelevance);
    RUN_TEST(TestSortingByRating);
    RUN_TEST(TestCalculateRating);
    RUN_TEST(TestPredicateAsFilter);
    RUN_TEST(TestGivenStatusAsFilter);
    RUN_TEST(TestIsCorrectRelevance);
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    cerr << "Search server testing finished"s << endl;
}