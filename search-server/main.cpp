#include <execution>
#include <iostream>
#include <random>
#include <string>
#include <vector>
#include <thread>

#include "search_server.h"
#include "log_duration.h"
#include "document.h"
#include "paginator.h"
#include "request_queue.h"
#include "test_example_functions.h"
#include "process_queries.h"

using namespace std;

string GenerateWord(mt19937& generator, int max_length) {
    const int length = uniform_int_distribution(1, max_length)(generator);
    string word;
    word.reserve(length);
    for (int i = 0; i < length; ++i) {
        word.push_back(uniform_int_distribution('a', 'z')(generator));
    }
    return word;
}
vector<string> GenerateDictionary(mt19937& generator, int word_count, int max_length) {
    vector<string> words;
    words.reserve(word_count);
    for (int i = 0; i < word_count; ++i) {
        words.push_back(GenerateWord(generator, max_length));
    }
    sort(words.begin(), words.end());
    words.erase(unique(words.begin(), words.end()), words.end());
    return words;
}
string GenerateQuery(mt19937& generator, const vector<string>& dictionary, int word_count, double minus_prob = 0) {
    string query;
    for (int i = 0; i < word_count; ++i) {
        if (!query.empty()) {
            query.push_back(' ');
        }
        if (uniform_real_distribution<>(0, 1)(generator) < minus_prob) {
            query.push_back('-');
        }
        query += dictionary[uniform_int_distribution<int>(0, dictionary.size() - 1)(generator)];
    }
    return query;
}
string GenerateQueryForMatchBenchmark(mt19937& generator, const vector<string>& dictionary, int word_count, double minus_prob = 0) {
    string query;
    for (int i = 0; i < word_count; ++i) {
        if (!query.empty()) {
            query.push_back(' ');
        }
        if (uniform_real_distribution<>(0, 1)(generator) < minus_prob) {
            query.push_back('-');
        }
        query += dictionary[uniform_int_distribution<int>(0, dictionary.size() - 1)(generator)];
    }
    return query;
}
vector<string> GenerateQueries(mt19937& generator, const vector<string>& dictionary, int query_count, int max_word_count) {
    vector<string> queries;
    queries.reserve(query_count);
    for (int i = 0; i < query_count; ++i) {
        queries.push_back(GenerateQuery(generator, dictionary, max_word_count));
    }
    return queries;
}
template <typename ExecutionPolicy>
void TestFindTopDocuments(string_view mark, const SearchServer& search_server, const vector<string>& queries, ExecutionPolicy&& policy) {
    LOG_DURATION(mark);
    double total_relevance = 0;
    for (const string_view query : queries) {
        for (const auto& document : search_server.FindTopDocuments(policy, query)) {
            total_relevance += document.relevance;
        }
    }
    cout << total_relevance << endl;
}
#define TEST_FIND_TOP_DOCUMENTS(policy) TestFindTopDocuments(#policy, search_server, queries, execution::policy)


template <typename QueriesProcessor>
void Test(string_view mark, QueriesProcessor processor, const SearchServer& search_server, const vector<string>& queries) {
    LOG_DURATION(mark);
    const auto documents_lists = processor(search_server, queries);
}
#define TEST(processor) Test(#processor, processor, search_server, queries)

template <typename ExecutionPolicy>
void Test(string_view mark, SearchServer search_server, ExecutionPolicy&& policy) {
    LOG_DURATION(mark);
    const int document_count = search_server.GetDocumentCount();
    for (int id = 0; id < document_count; ++id) {
        search_server.RemoveDocument(policy, id);
    }
    cout << search_server.GetDocumentCount() << endl;
}
#define TEST_REMOVE(mode) Test(#mode, search_server, execution::mode)


template <typename ExecutionPolicy>
void Test(string_view mark, SearchServer search_server, const string& query, ExecutionPolicy&& policy) {
    LOG_DURATION(mark);
    const int document_count = search_server.GetDocumentCount();
    int word_count = 0;
    for (int id = 0; id < document_count; ++id) {
        const auto [words, status] = search_server.MatchDocument(policy, query, id);
        word_count += words.size();
    }
    cout << word_count << endl;
}
#define TEST_MATCH(policy) Test(#policy, search_server, query, execution::policy)


struct RandomData {
    vector<string> dictionary;
    vector<string> documents;
    vector<string> queries;
};

RandomData MakeRandomData() {

    mt19937 generator;
    const auto dictionary_ = GenerateDictionary(generator, 2'000, 25);
    const auto documents_ = GenerateQueries(generator, dictionary_, 20'000, 10);
    const auto queries_ = GenerateQueries(generator, dictionary_, 2'000, 7);

    return {dictionary_, documents_, queries_};
}

int main() {
TestSearchServer();
    std::cerr << "\nSearch server testing finished\n"sv << std::endl;
    
    {
        SearchServer search_server("and with"sv);

        AddDocument(search_server, 1, "funny pet and nasty rat"sv, DocumentStatus::ACTUAL, {7, 2, 7});
        AddDocument(search_server, 2, "funny pet with curly hair"sv, DocumentStatus::ACTUAL, {1, 2});

        // дубликат документа 2, будет удалён
        AddDocument(search_server, 3, "funny pet with curly hair"sv, DocumentStatus::ACTUAL, {1, 2});

        // отличие только в стоп-словах, считаем дубликатом
        AddDocument(search_server, 4, "funny pet and curly hair"sv, DocumentStatus::ACTUAL, {1, 2});

        // множество слов такое же, считаем дубликатом документа 1
        AddDocument(search_server, 5, "funny funny pet and nasty nasty rat"sv, DocumentStatus::ACTUAL, {1, 2});

        // добавились новые слова, дубликатом не является
        AddDocument(search_server, 6, "funny pet and not very nasty rat"sv, DocumentStatus::ACTUAL, {1, 2});

        // множество слов такое же, как в id 6, несмотря на другой порядок, считаем дубликатом
        AddDocument(search_server, 7, "very nasty rat and not very funny pet"sv, DocumentStatus::ACTUAL, {1, 2});

        // есть не все слова, не является дубликатом
        AddDocument(search_server, 8, "pet with rat and rat and rat"sv, DocumentStatus::ACTUAL, {1, 2});

        // слова из разных документов, не является дубликатом
        AddDocument(search_server, 9, "nasty rat with curly hair"sv, DocumentStatus::ACTUAL, {1, 2});
        
        std::cout << "Before duplicates removed: "sv << search_server.GetDocumentCount() << std::endl;
        RemoveDuplicates(search_server);
        std::cout << "After duplicates removed: "sv << search_server.GetDocumentCount() << std::endl;
    }
    
    {
        SearchServer search_server("and in at"sv);
        RequestQueue request_queue(search_server);
        search_server.AddDocument(5, "curly cat curly tail"sv, DocumentStatus::ACTUAL, {7, 2, 7});
        search_server.AddDocument(4, "curly dog and fancy collar"sv, DocumentStatus::ACTUAL, {1, 2, 3});
        search_server.AddDocument(1, "big cat fancy collar "sv, DocumentStatus::ACTUAL, {1, 2, 8});
        search_server.AddDocument(3, "big dog sparrow Eugene"sv, DocumentStatus::ACTUAL, {1, 3, 2});
        search_server.AddDocument(2, "big dog sparrow Vasiliy"sv, DocumentStatus::ACTUAL, {1, 1, 1});
        search_server.AddDocument(6974, "big dog sparrow Vasiliy"sv, DocumentStatus::ACTUAL, {1, 1, 1});
        {
            LOG_DURATION_STREAM("Operation time"sv, std::cout);
            std::this_thread::sleep_for(std::chrono::seconds(3));
        }

        {
            LOG_DURATION_STREAM("Operation time"sv, std::cerr);
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }

        std::cout << "Testing GetWordFrequencies"sv << std::endl;
        if (search_server.GetWordFrequencies(2).size()) {
            for (const auto& [k, v] : search_server.GetWordFrequencies(2)) { // почему если const auto [k, v] писать, то ошибка?
                std::cout << k << ": "sv << v << std::endl;
            }
        }

        if (search_server.GetWordFrequencies(505).size()) {
            for (const auto& [k, v] : search_server.GetWordFrequencies(505)) {
                std::cout << k << ": "sv << v << std::endl;
            }
        } else {
            std::cout << "Noting found. Document is not exist"sv << std::endl;
        }

        // 1439 запросов с нулевым результатом
        for (int i = 0; i < 1439; ++i) {
            request_queue.AddFindRequest("empty request"sv);
        }
        // первый запрос удален, 1437 запросов с нулевым результатом
        request_queue.AddFindRequest("sparrow"sv);
        // все еще 1439 запросов с нулевым результатом
        request_queue.AddFindRequest("curly dog"sv);
        // новые сутки, первый запрос удален, 1438 запросов с нулевым результатом
        request_queue.AddFindRequest("big collar"sv, DocumentStatus::BANNED);
        std::cout << "Total empty requests: "sv << request_queue.GetNoResultRequests() << std::endl;  // Total empty requests: 1437 
    }

    {
        SearchServer search_server("and with"sv);
        search_server.AddDocument(1, "funny pet and nasty rat"sv, DocumentStatus::ACTUAL, {7, 2, 7});
        search_server.AddDocument(2, "funny pet with curly hair"sv, DocumentStatus::ACTUAL, {1, 2, 3});
        search_server.AddDocument(3, "big cat nasty hair"sv, DocumentStatus::ACTUAL, {1, 2, 8});
        search_server.AddDocument(4, "big dog cat Vladislav"sv, DocumentStatus::ACTUAL, {1, 3, 2});
        search_server.AddDocument(5, "big dog hamster Borya"sv, DocumentStatus::ACTUAL, {1, 1, 1});
        
        const auto search_results = search_server.FindTopDocuments("curly dog"sv);
        int page_size = 2;
        const auto pages = Paginate(search_results, page_size);
        // Выводим найденные документы по страницам
        for (auto page = pages.begin(); page != pages.end(); ++page) {
            std::cout << *page << std::endl;
            std::cout << "Page break"sv << std::endl;
        }
    }

    {
        SearchServer search_server("and with"sv);
        int id = 0;
        for (
            string_view text : {
                "funny pet and nasty rat"sv,
                "funny pet with curly hair"sv,
                "funny pet and not very nasty rat"sv,
                "pet with rat and rat and rat"sv,
                "nasty rat with curly hair"sv,
            }
            ) {
                search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, { 1, 2 });
        }

        const vector<string> queries = {
            "nasty rat -not"s,
            "not very funny nasty pet"s,
            "curly hair"s,
        };

        id = 0;

        for (const auto& documents : ProcessQueries(search_server, queries)) {
            cout << documents.size() << " documents for query ["sv << queries[id++] << "]"sv << endl;
        }
    }

    {
        cout << "BIG TEST_1"sv << endl;
        
        RandomData random_data = MakeRandomData();

        //mt19937 generator;
        //const auto dictionary = GenerateDictionary(generator, 2'000, 25);
        //const auto documents = GenerateQueries(generator, dictionary, 20'000, 10);
        SearchServer search_server(random_data.dictionary[0]);
        for (size_t i = 0; i < random_data.documents.size(); ++i) {
            search_server.AddDocument(i, random_data.documents[i], DocumentStatus::ACTUAL, {1, 2, 3});
        }
        
        // const auto queries = GenerateQueries(generator, dictionary, 2'000, 7);
        const auto queries = random_data.queries;
        TEST(ProcessQueries);
    }

    {
        SearchServer search_server("and with"sv);
        int id = 0;
        for (
            string_view text : {
                "funny pet and nasty rat"sv,
                "funny pet with curly hair"sv,
                "funny pet and not very nasty rat"sv,
                "pet with rat and rat and rat"sv,
                "nasty rat with curly hair"sv,
            }
            ) {
                search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});
        }

        const vector<string> queries = {
            "nasty rat -not"s,
            "not very funny nasty pet"s,
            "curly hair"s
        };

        for (const Document& document : ProcessQueriesJoined(search_server, queries)) {
            cout << "Document "sv << document.id << " matched with relevance "sv << document.relevance << endl;
        }
    }

    {
        cout << "BIG TEST_2"sv << endl;

        RandomData random_data = MakeRandomData();

        //mt19937 generator;
        //const auto dictionary = GenerateDictionary(generator, 2'000, 25);
        //const auto documents = GenerateQueries(generator, dictionary, 20'000, 10);
        SearchServer search_server(random_data.dictionary[0]);
        for (size_t i = 0; i < random_data.documents.size(); ++i) {
            search_server.AddDocument(i, random_data.documents[i], DocumentStatus::ACTUAL, {1, 2, 3});
        }
        
        // const auto queries = GenerateQueries(generator, dictionary, 2'000, 7);
        const auto queries = random_data.queries;
        TEST(ProcessQueriesJoined);
    }

    {
        SearchServer search_server("and with"sv);

        int id = 0;
        for (
            string_view text : {
                "funny pet and nasty rat"sv,
                "funny pet with curly hair"sv,
                "funny pet and not very nasty rat"sv,
                "pet with rat and rat and rat"sv,
                "nasty rat with curly hair"sv,
            }
        ) {
            search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});
        }

        string_view query = "curly and funny"sv;

        auto report = [&search_server, &query] {
            cout << search_server.GetDocumentCount() << " documents total, "sv
                << search_server.FindTopDocuments(query).size() << " documents for query ["sv << query << "]"sv << endl;
        };

        report();
        // однопоточная версия
        search_server.RemoveDocument(5);
        report();
        // однопоточная версия
        search_server.RemoveDocument(execution::seq, 1);
        report();
        // многопоточная версия
        search_server.RemoveDocument(execution::par, 2);
        report();
    }

    {
        cout << "BIG TEST_3"sv << endl;
        
        mt19937 generator;

        const auto dictionary = GenerateDictionary(generator, 10'000, 25);
        const auto documents = GenerateQueries(generator, dictionary, 10'000, 100);

        {
            SearchServer search_server(dictionary[0]);
            for (size_t i = 0; i < documents.size(); ++i) {
                search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, {1, 2, 3});
            }

            TEST_REMOVE(seq);
        }

        {
            SearchServer search_server(dictionary[0]);
            for (size_t i = 0; i < documents.size(); ++i) {
                search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, {1, 2, 3});
            }

            TEST_REMOVE(par);
        }
    }

    {
        SearchServer search_server("and with"sv);

        int id = 0;
        for (
            string_view text : {
                "funny pet and nasty rat"sv,
                "funny pet with curly hair"sv,
                "funny pet and not very nasty rat"sv,
                "pet with rat and rat and rat"sv,
                "nasty rat with curly hair"sv,
                "ab burip mclqhd mtxod ndjl nozlc ohmqspozm ovmr uyglccvb x xp ytoqnc"sv,
                "ab burip mclqhd mtxod ndjl nozlc ohmqspozm ovmr uyglccvb x xp ytoqnc pjt"sv,

            }
        ) {
            search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});
        }

        string_view query = "curly and funny -not"sv;
        string_view crash_query = "usedxuiu dlzlnrr wspnlvxnf kiuato yj somsxzkhbk cxwzlnght uzlnamh -jqkkkf mtfyznnu ize axsrmkl scbgb ddegd ght fwslqxp mbad busnuxz nxnjuxdw wfkqfu zyz bcmrqlfp xns r rzknvss wubw oy kgfkilovfv ucjkffcc djnusljl xqchngd ndjl rufywewnb pzhmnfdfv -pjt rcsu zzwrzfrrkq uwzju svvv qgj wtuz losbgravn djcddp yggmvkg pxzflhs um gclevw sjbrfklv oruoslwq rufywewnb zzwrzfrrkq pqygcnaoxd unn mtxod woyikly owexd xytepxy hvfxfz qacucdvm ndjl zap wtuz qzzjxzmj adliabmm lzeqp kfnvor mzoohi cxdbes zjzz wckpqyzh yubgcyc -yvbahq afmgipzmo qkafvaobw koxejay tyfka un widcjjqb ruylwncc bbxbiufxcv seqdlmu ydfrxp rltxdezgc -cxwzlnght dcnmpastf oheayigxyc zlqh -ebcy rvkuzsktq dhdht -nrsgsxup oc pqygcnaoxd wuvctv -fiqqgpoky -eoz vmuumpimr gayvjhigx idc qvxfeusap heecmjffwm -pwav axsrmkl zwygf qndblsqjvd eyul oaosgn -ypudd -ru bvgssdfrk azrqltltp awxyxaup g afmgipzmo nrsgsxup ttuppgmrp fqg yeosl nzxsuifpx fkcxhr yskaavlx gdbepduu tdt gr fapv pwav dlzlnrr jxknzhykr -djcddp hkikge pxztfgprad vwkww dfixsc y powuyho oy vv wmxtdkqh clbikvloj lgszku azmyhfi jxdjwx diwqc nnssrfpob shtb qjdz sdg astjftmyho vyasak bq qs ttuppgmrp nyhbbnckbd acoacd pipz fqg rg kijzont novq wxk raquyafpsq -af zfpcg ulm zzt lms nafpskz asxs gpn rf yotmzmtioy qkafvaobw -rufywewnb nrsgsxup kfnvor qduk -cxwzlnght wjgtvko tdt gtydjlkjn ursjxqmgpo af wpumo kiuato lwvsjair -djnusljl grhcqmgz -fnvzyftc kvdfp btroxec dbwp -wivigrppa -qftypwtsg mzoohi wvii oelfninyak kiuato xzws diwqc irn gcgo ebafsuixiu vgmp vgawjsdwx zap lzds kgfkilovfv zsaa -wlzmnowvp -wspnlvxnf diwqc -ggjsc ostcfkwp cjyluhm qs busnuxz ootbd hjxbmsnz bznt ejxcvnwjo pxndcnxkd qveopophs vlqbo pworppnu -cfdu lbrp mbxgyfx nrsgsxup un nlxqoyl imemelkk xytepxy lbtxvdhve zmtsqs fb uqfot wrzxf xkqreq qttbu rqnikfplt ebcy kmkasi xtuuhkoxzc -pghsunul ostcfkwp bbc clbikvloj jeutugr vmxqvgr tvbd gclevw iphgz kgfkilovfv lyvftye da fnwb pejq woyikly xnezwfwkws koxejay xqchngd xffvx jddcltc irhhtqa xzws adliabmm -tnimibyr ab tyfka ehpphjdifu yeosl ohmqspozm rly rqmegjo -mkfuxelcc y qduk -opydko -usowzpfb igp qgj zhugyx iafyndxf boisuus gayvjhigx amauhsc xv fnvzyftc -qjxlfbkazw -atyxlpz wdzbjz mdxdooaobp abvsvwb btroxec qkafvaobw kmzba hkulx yvcycbdr pwav xp jsuqqkx bwbynr dpllsyursl acahwllpcw zlbsyvag -ifkroxqfl awxyxaup az reicxbdh erxsaxg -cjhenn qtfkubbp -nyb li ciyrkqfn szcskxhwh pfirdolw yxxnqciv uxb burip -wjgtvko dbiptqmqaw kpaarivq wuvctv pbjlf fqmecouyhu hiewegfpt pzhmnfdfv lsoix vlqbo hyvsmm jp ushys fzdyftd -qttbu mkxbku stmin nbpsvqnw ycyrjnmfk rvkuzsktq pfirdolw -ukycol stzlscx kjzscm -rjg -xsfs iisyxnq eg lzuk fg dyzrsdfs nzxsuifpx beoch hzaidvt suai -wlkcmgvs jbsdsnfwgu lj udmdrikn -vzgivhgay dmy dfixsc nrsgsxup gayvjhigx qgj mh -yzytd qfv vfsq ujgcutuiy kree xefsthjrkz sxtuhsra c zuzpq ihufmgauem lrv amauhsc -vmpg novq zhugyx npzf -kieozpkkvr -aqkvmze fbnlapnrks xn vv juxqojdvf cwxzswovix asty trngm bphah ydfrxp -pejq nqcqu spqtiry nuctce pghsunul pbjlf zuzpq wtuz mh cnicyx ulm eoz iciif xfygvpsnt lmlthcnknw awcszaqjg cs -tkzvwccrjc -djftb szcskxhwh nh csgekdp -mpwovne caekvtpua pipz ypudd v dlzlnrr uzlnamh uitrdt lnjjyg mluxmkwtq udij xi rly fv lgszku oaoyrw sbrkramf kieozpkkvr asty xfygvpsnt lgwh qacucdvm lrdyp yggmvkg wcpqoadrw hkulx at djftb -bcmrqlfp sau kdvln sdg qagtxyzin ushys ccgvgg -nqpczaequo utdohtjq woyikly ebcy bjnpu bvgssdfrk ivxt ieqescx neuwfrrb mh twn xmp bbxbiufxcv -hvfxfz dmy eyqaujnr qcnkwec rejxztc qdlumd -dwezhaawk hvfpecwx xqchngd lzlfotlpl gutgypf siocnkvhw ifbj hzo nafpskz bhkw w ndjl zk xgsf kmfdoisp zzwrzfrrkq tlvauci sikpr mbad slvzrv vkx yrr u engz xfygvpsnt pq lmn hkikge gayvjhigx xzws vsditymar"sv;

        {
            const auto [words, status] = search_server.MatchDocument(std::execution::par, crash_query, 6);
            cout << words.size() << " words for document 6"sv << endl;
            // 6 words for document 6
        }

        {
            const auto [words, status] = search_server.MatchDocument(std::execution::par, crash_query, 7);
            cout << words.size() << " words for document 7"sv << endl;
            // 0 words for document 7 // т.к ptj это минус-слово
        }

        {
            const auto [words, status] = search_server.MatchDocument(query, 1);
            cout << words.size() << " words for document 1"sv << endl;
            // 1 words for document 1
        }

        {
            const auto [words, status] = search_server.MatchDocument(execution::par, query, 2);
            cout << words.size() << " words for document 2"sv << endl;
            // 2 words for document 2
        }

        {
            const auto [words, status] = search_server.MatchDocument(execution::par, query, 3);
            cout << words.size() << " words for document 3"sv << endl;
            // 0 words for document 3
        }

    }

    {
        cout << "BIG TEST_4"sv << endl;

        mt19937 generator;

        const auto dictionary = GenerateDictionary(generator, 1000, 10);
        const auto documents = GenerateQueries(generator, dictionary, 10'000, 70);

        const string query = GenerateQueryForMatchBenchmark(generator, dictionary, 500, 0.1);

        SearchServer search_server(dictionary[0]);
        for (size_t i = 0; i < documents.size(); ++i) {
            search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, {1, 2, 3});
        }

        TEST_MATCH(seq);
        TEST_MATCH(par);
    }

    {
        cout << "BIG TEST_5"sv << endl;

        mt19937 generator;
        const auto dictionary = GenerateDictionary(generator, 1000, 10);
        const auto documents = GenerateQueries(generator, dictionary, 10'000, 70);
        SearchServer search_server(dictionary[0]);
        for (size_t i = 0; i < documents.size(); ++i) {
            search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, {1, 2, 3});
        }
        const auto queries = GenerateQueries(generator, dictionary, 100, 70);
        TEST_FIND_TOP_DOCUMENTS(seq);
        TEST_FIND_TOP_DOCUMENTS(par);

        cout << endl;
    }

    cout << "Search Server Development Complete"s << endl;

    return 0;

}