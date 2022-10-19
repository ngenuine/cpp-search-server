#include "test_example_functions.h"

void AssertImpl(bool value, const std::string& expr_str, const std::string& file, const std::string& func, unsigned line,
                const std::string& hint) {
    if (!value) {
        std::cerr << file << "("s << line << "): "s << func << ": "s;
        std::cerr << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            std::cerr << " Hint: "s << hint;
        }
        std::cerr << std::endl;
        abort();
    }
}

void TestAddingDocuments() {
    const int id1 = 35;
    const std::string content1 = "spider man and doctor stiven strange with hulk"s;
    const std::vector<int> ratings1 = {4, 5, 6, 5};

    {
        std::vector<std::string> stop_words{};
        SearchServer search_server(""s);
        search_server.AddDocument(id1, content1, DocumentStatus::ACTUAL, ratings1);
        std::vector<Document> answer = search_server.FindTopDocuments("stiven"s);
        ASSERT_EQUAL_HINT(answer.size(), 1, "Size of search results should be 1");
        ASSERT_EQUAL(answer[0].id, id1);
    }
}

void TestExcludingStopWords() {
    const int id1 = 35;
    const std::string content1 = "spider man and doctor stiven strange with hulk"s;
    const std::vector<int> ratings1 = {4, 5, 6, 5};

    {
        SearchServer search_server("with and"s);
        search_server.AddDocument(id1, content1, DocumentStatus::ACTUAL, ratings1);
        ASSERT_HINT(search_server.FindTopDocuments("and with"s).empty(), "Search result should be empty");
    }
}

void TestExcludingMinusWords() {
    const int id1 = 35;
    const std::string content1 = "spider man and doctor stiven strange with hulk"s;
    const std::vector<int> ratings1 = {4, 5, 6, 5};

    const int id2 = 45;
    const std::string content2 = "spider man and doctor stiven strange with neo"s;
    const std::vector<int> ratings2 = {4, 5, 1};

    {
        SearchServer search_server(""s);
        search_server.AddDocument(id1, content1, DocumentStatus::ACTUAL, ratings1);
        search_server.AddDocument(id2, content2, DocumentStatus::ACTUAL, ratings2);
        std::vector<Document> answer = search_server.FindTopDocuments("spider man -hulk"s);
        ASSERT_EQUAL_HINT(answer.size(), 1, "Search engine doesn't exclude query witn minus-words");
        ASSERT_EQUAL(answer[0].id, id2);
    }
}

void TestMatchingDocument() {
    const int id1 = 35;
    const std::string content1 = "spider man and doctor stiven strange with hulk"s;
    const std::vector<int> ratings1 = {4, 5, 6, 5};

    {
        SearchServer search_server(""s);
        search_server.AddDocument(id1, content1, DocumentStatus::ACTUAL, ratings1);
        std::tuple<std::vector<std::string>, DocumentStatus> answer1 = search_server.MatchDocument("spider man -hulk"s, id1);
        ASSERT_EQUAL(std::get<0>(answer1).size(), 0);
        
        std::tuple<std::vector<std::string>, DocumentStatus> answer2 = search_server.MatchDocument("spider hulk"s, id1);
        const std::vector<std::string> intersection = {"hulk"s, "spider"s};
        ASSERT_EQUAL(std::get<0>(answer2), intersection);
    }
}

void TestSortingByRelevance() {
    
    {
        const int id1 = 3;
        const std::string content1 = "spider man and doctor stiven strange with hulk"s;
        const std::vector<int> ratings1 = {4, 5, 6, 5};
        const int id2 = 2;
        const std::string content2 = "scooby dooby man our pretty fan you should finger flip pa-pa-pam"s;
        const std::vector<int> ratings2 = {1, 2, 4};
        const int id3 = 1;
        const std::string content3 = "pretty woman with hulk"s;
        const std::vector<int> ratings3 = {4, 4, 4};

        SearchServer search_server(""s);
        search_server.AddDocument(id1, content1, DocumentStatus::ACTUAL, ratings1);
        search_server.AddDocument(id2, content2, DocumentStatus::ACTUAL, ratings2);
        search_server.AddDocument(id3, content3, DocumentStatus::ACTUAL, ratings3);

        std::vector<Document> answer = search_server.FindTopDocuments("spider man and hulk"s);
        ASSERT_EQUAL(answer[0].id, id1);
        ASSERT_EQUAL(answer[1].id, id3);
        ASSERT_EQUAL(answer[2].id, id2);
    }
}

void TestSortingByRating() {

    {
        const int id1 = 3;
        const std::string content1 = "spider man and doctor stiven strange with hulk"s;
        const std::vector<int> ratings1 = {};
        const int id2 = 15;
        const std::string content2 = "spider man and doctor stiven strange with hulk"s;
        const std::vector<int> ratings2 = {100};
        const int id3 = 400;
        const std::string content3 = "spider man and doctor stiven strange with hulk"s;
        const std::vector<int> ratings3 = {500};

        SearchServer search_server(""s);
        search_server.AddDocument(id1, content1, DocumentStatus::ACTUAL, ratings1);
        search_server.AddDocument(id2, content2, DocumentStatus::ACTUAL, ratings2);
        search_server.AddDocument(id3, content3, DocumentStatus::ACTUAL, ratings3);

        std::vector<Document> answer = search_server.FindTopDocuments("spider scooby pretty"s);
        ASSERT_EQUAL(answer[0].id, id3);
        ASSERT_EQUAL(answer[1].id, id2);
        ASSERT_EQUAL(answer[2].id, id1);
    }
}

void TestCalculateRating() {
    const int id1 = 3;
    const std::string content1 = "spider man and doctor stiven strange with hulk"s;
    const std::vector<int> ratings1 = {4, 5, 6, 5};
    const int id2 = 2;
    const std::string content2 = "black cat and harry potter play hard"s;
    const std::vector<int> ratings2 = {};

    const int ZERO_RATING = 0;

    int expected_rating1 = (4 + 5 + 6 + 5) / 4;
    int expected_rating2 = ZERO_RATING;

    {
        SearchServer search_server(""s);
        search_server.AddDocument(id1, content1, DocumentStatus::ACTUAL, ratings1);
        search_server.AddDocument(id2, content2, DocumentStatus::ACTUAL, ratings2);

        std::vector<Document> answer = search_server.FindTopDocuments("spider man and hulk"s);
        ASSERT_EQUAL(answer[0].rating, expected_rating1);
        ASSERT_EQUAL(answer[1].rating, expected_rating2);
    }
}

void TestPredicateAsFilter() {
    const int id1 = 3;
    const std::string content1 = "spider man and doctor stiven strange with hulk"s;
    const std::vector<int> ratings1 = {4, 5, 6, 5};
    const int id2 = 2;
    const std::string content2 = "scooby dooby man our pretty fan you should finger flip pa-pa-pam"s;
    const std::vector<int> ratings2 = {1, 2, 4};
    const int id3 = 1;
    const std::string content3 = "pretty woman with hulk"s;
    const std::vector<int> ratings3 = {4, 4, 4};

    {
        SearchServer search_server(""s);
        search_server.AddDocument(id1, content1, DocumentStatus::ACTUAL, ratings1);
        search_server.AddDocument(id2, content2, DocumentStatus::ACTUAL, ratings2);
        search_server.AddDocument(id3, content3, DocumentStatus::ACTUAL, ratings3);

        auto predicate = [](const int& id, const DocumentStatus& status, const int& rating) {
            return id > 2;
        };

        std::vector<Document> answer = search_server.FindTopDocuments("spider man and hulk"s, predicate);
        ASSERT_EQUAL(answer.size(), 1);
    }
}

void TestGivenStatusAsFilter() {
    const int id1 = 3;
    const std::string content1 = "spider man and doctor stiven strange with hulk"s;
    const std::vector<int> ratings1 = {4, 5, 6, 5};
    const int id2 = 2;
    const std::string content2 = "scooby dooby man our pretty fan you should finger flip pa-pa-pam"s;
    const std::vector<int> ratings2 = {1, 2, 4};
    const int id3 = 1;
    const std::string content3 = "pretty woman with hulk"s;
    const std::vector<int> ratings3 = {4, 4, 4};

    {
        SearchServer search_server(""s);
        search_server.AddDocument(id1, content1, DocumentStatus::IRRELEVANT, ratings1);
        search_server.AddDocument(id2, content2, DocumentStatus::BANNED, ratings2);
        search_server.AddDocument(id3, content3, DocumentStatus::ACTUAL, ratings3);

        std::vector<Document> answer1 = search_server.FindTopDocuments("spider man and hulk"s, DocumentStatus::IRRELEVANT);
        ASSERT_EQUAL(answer1[0].id, id1);
    }
}

void TestIsCorrectRelevance() {
    const int id1 = 3;
    const std::string content1 = "spider man and doctor stiven strange with hulk"s;
    const std::vector<int> ratings1 = {4, 5, 6, 5};
    const int id2 = 2;
    const std::string content2 = "scooby dooby man our pretty fan you should finger flip pa-pa-pam"s;
    const std::vector<int> ratings2 = {1, 2, 4};
    const int id3 = 1;
    const std::string content3 = "pretty woman with hulk"s;
    const std::vector<int> ratings3 = {4, 4, 4};

    {
        SearchServer search_server(""s);
        search_server.AddDocument(id1, content1, DocumentStatus::ACTUAL, ratings1);
        search_server.AddDocument(id2, content2, DocumentStatus::ACTUAL, ratings2);
        search_server.AddDocument(id3, content3, DocumentStatus::ACTUAL, ratings3);

        const int precise = 10e-6;

        std::vector<Document> answer = search_server.FindTopDocuments("spider man and hulk"s);

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