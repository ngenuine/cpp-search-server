#include <algorithm>
#include <iostream>
#include <set>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include <cmath>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

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
    int id;
    double relevance;
};

class SearchServer {
public:
    void SetStopWords(const string& text) {

        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document) {

        // наполняем счетчик документов -- он пригодится для подсчета IDF.
        ++document_count_;

        const vector<string> words = SplitIntoWordsNoStop(document);

        // Рассчитываем TF каждого слова в каждом документе.
        for (const string& word : words) {
            TF_[word][document_id] += 1.0 / words.size();
            index_[word].insert(document_id);
        }
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {

        const PlusMinusWords query_words = ParseQuery(raw_query);

        auto matched_documents = FindAllDocuments(query_words);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                 return lhs.relevance > rhs.relevance;
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

private:

    struct PlusMinusWords {
        set<string> plus_words;
        set<string> minus_words;
    };

    int document_count_ = 0;

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
                query_words.minus_words.insert(word.substr(1, word.size()));
            } else {
                if (query_words.minus_words.count(word) == 0) {
                    query_words.plus_words.insert(word);
                }
            }
        }
        return query_words;
    }

    vector<Document> FindAllDocuments(const PlusMinusWords& query_words) const {

        /* Рассчитываем IDF каждого плюс-слова в запросе:
        1) количество документов document_count_ делим на количество документов, где это слово встречается;
        2) берем от полученного значения log.

        Функция AddDocument построила index_, где каждому слову отнесено мнодество документов, где оно встречается.
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

        vector<Document> result;

        for (const auto& [document_id, relevance] : matched_documents) {
            result.push_back({document_id, relevance});
        }

        return result;
    }
};

SearchServer CreateSearchServer() {

    SearchServer search_server;
    search_server.SetStopWords(ReadLine());

    const int document_count = ReadLineWithNumber();
    for (int document_id = 0; document_id < document_count; ++document_id) {
        search_server.AddDocument(document_id, ReadLine());
    }

    return search_server;
}

int main() {

    const SearchServer search_server = CreateSearchServer();

    const string query = ReadLine();

    for (const auto& [document_id, relevance] : search_server.FindTopDocuments(query)) {
        cout << "{ document_id = "s << document_id << ", "
             << "relevance = "s << relevance << " }"s << endl;
    }
}

/*
с в на вы около без и
14
очень красивый прям очень красивый пёс найден в лесу около дом книги
голубая канарейка плавает в озере в утками кто потерял идите в парк победы
белый кот ходит вокруг дом быт и по цепи
кот красивый без ошейника с дырявым ухом с стриженными ногтями
дог с красивыми стриженными волосами убежал в районе ломоносовская
найден домашний таракан с усами
собака потеряшка ломоносовская
грязный кот ошивается около телебашни
ухоженный ребенок породы хаски временно живет на жд станции буратино
голубая канарейка и таракан нашли друг в друге хозяев на станции парнас
красивый африканский морж скоро приедет в зоопарк . принимать солнечные ванны
ухоженный заяц с дырявым ухом ищет хозяев
родители вы в своем уме найден ребенок внимание найден ребенок родители отзовитесь
хороший мультфильм ремейк буратино выходит на наших радиостанциях
пропал ребенок помогите найти где он ошивается он наш друг с дырявым пальцем -кот

s v na vy okolo bez i
14
ochen krasivyj pryam ochen krasivyj pes najden v lesu okolo dom knigi
golubaya kanarejka plavaet v ozere v utkami kto poteryal idite v park pobedy
belyj kot hodit vokrug dom byt i po cepi
kot krasivyj bez oshejnika s dyryavym uhom s strizhennymi nogtyami
dog s krasivymi strizhennymi volosami ubezhal v rajone lomonosovskaya
najden domashnij tarakan s usami
sobaka poteryashka lomonosovskaya
gryaznyj kot oshivaetsya okolo telebashni
uhozhennyj rebenok porody haski vremenno zhivet na zhd stancii buratino
golubaya kanarejka i tarakan nashli drug v druge hozyaev na stancii parnas
krasivyj afrikanskij morzh skoro priedet v zoopark . prinimat solnechnye vanny
uhozhennyj zayac s dyryavym uhom ishchet hozyaev
roditeli vy v svoem ume najden rebenok vnimanie najden rebenok roditeli otzovites
horoshij multfilm remejk buratino vyhodit na nashih radiostanciyah
propal rebenok pomogite najti gde on oshivaetsya on nash drug s dyryavym palcem -kot
*/