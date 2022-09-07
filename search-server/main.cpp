#include <algorithm>
#include <iostream>
#include <set>
#include <map>
#include <string>
#include <vector>
#include <cmath>
#include <numeric>
#include <tuple>

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

void PrintDocument(const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating
         << " }"s << endl;
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
};

SearchServer CreateSearchServer() {

    SearchServer search_server;
    search_server.SetStopWords(ReadLine());

    const int document_count = ReadLineWithNumber();
    for (int document_id = 0; document_id < document_count; ++document_id) {
        string document = ReadLine();

        int status;
        cin >> status;
        DocumentStatus converted_status = static_cast<DocumentStatus>(status);

        vector<string> ratings_line = SplitIntoWords(ReadLine());
        vector<int> ratings;

        if (ratings_line.size() > 0 && stoi(ratings_line[0]) > 0) {
            for (int i = 1; i <= stoi(ratings_line[0]); ++i) {
                ratings.push_back(stoi(ratings_line[i]));
            }
        }

        search_server.AddDocument(document_id, document, converted_status, ratings);
    }

    return search_server;
}

int main() {
     SearchServer search_server;
    const std::vector<int> ratings0 = {1, 2, 3 , 4 , 5};
    const std::vector<int> ratings1 = {-1, -2, 30 , -3, 44 , 5};
    const std::vector<int> ratings2 = {12, -20, 80 , 0, 8, 0, 0, 9, 67};
    const std::vector<int> ratings3 = {1, 2, 3, 4, 4, 3, 2, 1};
    const std::vector<int> ratings4 = {3};
    const std::vector<int> ratings5 = {5, 5, 5, 8};
    const std::vector<int> ratings6 = {-105, -30, 15};
    const std::vector<int> ratings7 = {-105, -30, 10};
    const std::vector<int> ratings8 = {100};



    search_server.AddDocument(0, "belyi kot i modnyi osheinik", DocumentStatus::ACTUAL, ratings0);
    search_server.AddDocument(1, "pushistyi kot pushistyi hvost", DocumentStatus::ACTUAL, ratings1);
    search_server.AddDocument(2, "uhozhennyi pes byrazitelnye glaza", DocumentStatus::ACTUAL, ratings2);
    search_server.AddDocument(3, "uhozhennyi skvorec evgenyi", DocumentStatus::IRRELEVANT, ratings3);
    search_server.AddDocument(4, "uhozhennyi uhod", DocumentStatus::REMOVED, ratings4);
    search_server.AddDocument(5, "uho gorlo kot pes po akcii", DocumentStatus::REMOVED, ratings5);
    search_server.AddDocument(6, "pushistyi", DocumentStatus::REMOVED, ratings6);
    search_server.AddDocument(7, "uhozhennyi", DocumentStatus::IRRELEVANT, ratings7);
    search_server.AddDocument(8, "kot", DocumentStatus::IRRELEVANT, ratings8);



    const SearchServer const_search_server = search_server;
    const std::string query = "pushistyi i uhozhennyi kot";
    for (const Document& document : const_search_server.FindTopDocuments(query, [](int document_id, DocumentStatus status, int rating) {
        return status == DocumentStatus::REMOVED || status == DocumentStatus::IRRELEVANT;}
        )) {
        std::cout << "{ "
                << "document_id = " << document.id << ", "
                << "relevance = " << document.relevance << ", "
                << "rating = " << document.rating
                << " }" << std::endl;
    }
}