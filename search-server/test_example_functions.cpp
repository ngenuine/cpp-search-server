#include "document.h"
#include "test_example_functions.h"
#include <stdexcept>

using namespace std::string_literals;
using namespace std::string_view_literals;


void AssertImpl(bool value, std::string_view expr_str, std::string_view file, std::string_view func, unsigned line,
                std::string_view hint) {
    if (!value) {
        std::cerr << file << "("sv << line << "): "sv << func << ": "sv;
        std::cerr << "ASSERT("sv << expr_str << ") failed."sv;
        if (!hint.empty()) {
            std::cerr << " Hint: "sv << hint;
        }
        std::cerr << std::endl;
        abort();
    }
}

void TestAddingDocuments() {
    const int id1 = 35;
    std::string_view content1 = "spider man and doctor stiven strange with hulk"sv;
    const std::vector<int> ratings1 = {4, 5, 6, 5};

    {
        std::vector<std::string_view> stop_words{};
        SearchServer search_server(""sv);
        search_server.AddDocument(id1, content1, DocumentStatus::ACTUAL, ratings1);
        std::vector<Document> answer = search_server.FindTopDocuments("stiven"sv);
        ASSERT_EQUAL_HINT(static_cast<int>(answer.size()), 1, "Size of search results should be 1"sv);
        ASSERT_EQUAL(answer[0].id, id1);
    }

    {
        SearchServer search_server("and with"sv);

        AddDocument(search_server, 1, "funny pet and nasty rat"sv, DocumentStatus::ACTUAL, {7, 2, 7});
        AddDocument(search_server, 2, "funny pet with curly hair"sv, DocumentStatus::ACTUAL, {1, 2});
        
        int number_of_documents_in_the_search_server = 2;
        ASSERT_HINT(search_server.GetDocumentCount() == number_of_documents_in_the_search_server, "Not all documents have been added"sv);
    }
}

void TestExcludingStopWords() {
    const int id1 = 35;
    std::string_view content1 = "spider man and doctor stiven strange with hulk"sv;
    const std::vector<int> ratings1 = {4, 5, 6, 5};

    {
        SearchServer search_server("with and"sv);
        search_server.AddDocument(id1, content1, DocumentStatus::ACTUAL, ratings1);
        ASSERT_HINT(search_server.FindTopDocuments("and with"sv).empty(), "Search result should be empty"sv);
    }
}

void TestExcludingMinusWords() {
    const int id1 = 35;
    std::string_view content1 = "spider man and doctor stiven strange with hulk"sv;
    const std::vector<int> ratings1 = {4, 5, 6, 5};

    const int id2 = 45;
    std::string_view content2 = "spider man and doctor stiven strange with neo"sv;
    const std::vector<int> ratings2 = {4, 5, 1};

    {
        SearchServer search_server(""sv);
        search_server.AddDocument(id1, content1, DocumentStatus::ACTUAL, ratings1);
        search_server.AddDocument(id2, content2, DocumentStatus::ACTUAL, ratings2);
        std::vector<Document> answer = search_server.FindTopDocuments("spider man -hulk"sv);
        ASSERT_EQUAL_HINT(static_cast<int>(answer.size()), 1, "Search engine doesn't exclude query witn minus-words");
        ASSERT_EQUAL(answer[0].id, id2);
    }
}

void TestMatchingDocument() {
    const int id1 = 35;
    std::string_view content1 = "spider man and doctor stiven strange with hulk"sv;
    const std::vector<int> ratings1 = {4, 5, 6, 5};

    {
        SearchServer search_server(""sv);
        search_server.AddDocument(id1, content1, DocumentStatus::ACTUAL, ratings1);
        std::tuple<std::vector<std::string_view>, DocumentStatus> answer1 = search_server.MatchDocument("spider man -hulk"sv, id1);
        ASSERT_EQUAL(static_cast<int>(std::get<0>(answer1).size()), 0);
        
        std::tuple<std::vector<std::string_view>, DocumentStatus> answer2 = search_server.MatchDocument("spider hulk"sv, id1);
        const std::vector<std::string_view> intersection = {"hulk"sv, "spider"sv};
        ASSERT_EQUAL(std::get<0>(answer2), intersection);
    }
}

void TestSortingByRelevance() {
    
    {
        const int id1 = 3;
        std::string_view content1 = "spider man and doctor stiven strange with hulk"sv;
        const std::vector<int> ratings1 = {4, 5, 6, 5};
        const int id2 = 2;
        std::string_view content2 = "scooby dooby man our pretty fan you should finger flip pa-pa-pam"sv;
        const std::vector<int> ratings2 = {1, 2, 4};
        const int id3 = 1;
        std::string_view content3 = "pretty woman with hulk"sv;
        const std::vector<int> ratings3 = {4, 4, 4};

        SearchServer search_server(""sv);
        search_server.AddDocument(id1, content1, DocumentStatus::ACTUAL, ratings1);
        search_server.AddDocument(id2, content2, DocumentStatus::ACTUAL, ratings2);
        search_server.AddDocument(id3, content3, DocumentStatus::ACTUAL, ratings3);

        std::vector<Document> answer = search_server.FindTopDocuments("spider man and hulk"sv);
        ASSERT_EQUAL(answer[0].id, id1);
        ASSERT_EQUAL(answer[1].id, id3);
        ASSERT_EQUAL(answer[2].id, id2);
    }
}

void TestSortingByRating() {

    {
        const int id1 = 3;
        std::string_view content1 = "spider man and doctor stiven strange with hulk"sv;
        const std::vector<int> ratings1 = {};
        const int id2 = 15;
        std::string_view content2 = "spider man and doctor stiven strange with hulk"sv;
        const std::vector<int> ratings2 = {100};
        const int id3 = 400;
        std::string_view content3 = "spider man and doctor stiven strange with hulk"sv;
        const std::vector<int> ratings3 = {500};

        SearchServer search_server(""sv);
        search_server.AddDocument(id1, content1, DocumentStatus::ACTUAL, ratings1);
        search_server.AddDocument(id2, content2, DocumentStatus::ACTUAL, ratings2);
        search_server.AddDocument(id3, content3, DocumentStatus::ACTUAL, ratings3);

        std::vector<Document> answer = search_server.FindTopDocuments("spider scooby pretty"sv);
        ASSERT_EQUAL(answer[0].id, id3);
        ASSERT_EQUAL(answer[1].id, id2);
        ASSERT_EQUAL(answer[2].id, id1);
    }
}

void TestCalculateRating() {
    const int id1 = 3;
    std::string_view content1 = "spider man and doctor stiven strange with hulk"sv;
    const std::vector<int> ratings1 = {4, 5, 6, 5};
    const int id2 = 2;
    std::string_view content2 = "black cat and harry potter play hard"sv;
    const std::vector<int> ratings2 = {};

    const int ZERO_RATING = 0;

    int expected_rating1 = (4 + 5 + 6 + 5) / 4;
    int expected_rating2 = ZERO_RATING;

    {
        SearchServer search_server(""sv);
        search_server.AddDocument(id1, content1, DocumentStatus::ACTUAL, ratings1);
        search_server.AddDocument(id2, content2, DocumentStatus::ACTUAL, ratings2);

        std::vector<Document> answer = search_server.FindTopDocuments("spider man and hulk"sv);
        ASSERT_EQUAL(answer[0].rating, expected_rating1);
        ASSERT_EQUAL(answer[1].rating, expected_rating2);
    }
}

void TestPredicateAsFilter() {
    const int id1 = 3;
    std::string_view content1 = "spider man and doctor stiven strange with hulk"sv;
    const std::vector<int> ratings1 = {4, 5, 6, 5};
    const int id2 = 2;
    std::string_view content2 = "scooby dooby man our pretty fan you should finger flip pa-pa-pam"sv;
    const std::vector<int> ratings2 = {1, 2, 4};
    const int id3 = 1;
    std::string_view content3 = "pretty woman with hulk"sv;
    const std::vector<int> ratings3 = {4, 4, 4};

    {
        SearchServer search_server(""sv);
        search_server.AddDocument(id1, content1, DocumentStatus::ACTUAL, ratings1);
        search_server.AddDocument(id2, content2, DocumentStatus::ACTUAL, ratings2);
        search_server.AddDocument(id3, content3, DocumentStatus::ACTUAL, ratings3);

        auto predicate = [](const int& id, const DocumentStatus& status, const int& rating) {
            return id > 2;
        };

        std::vector<Document> answer = search_server.FindTopDocuments("spider man and hulk"sv, predicate);
        ASSERT_EQUAL(static_cast<int>(answer.size()), 1);
    }
}

void TestGivenStatusAsFilter() {
    const int id1 = 3;
    std::string_view content1 = "spider man and doctor stiven strange with hulk"sv;
    const std::vector<int> ratings1 = {4, 5, 6, 5};
    const int id2 = 2;
    std::string_view content2 = "scooby dooby man our pretty fan you should finger flip pa-pa-pam"sv;
    const std::vector<int> ratings2 = {1, 2, 4};
    const int id3 = 1;
    std::string_view content3 = "pretty woman with hulk"sv;
    const std::vector<int> ratings3 = {4, 4, 4};

    {
        SearchServer search_server(""sv);
        search_server.AddDocument(id1, content1, DocumentStatus::IRRELEVANT, ratings1);
        search_server.AddDocument(id2, content2, DocumentStatus::BANNED, ratings2);
        search_server.AddDocument(id3, content3, DocumentStatus::ACTUAL, ratings3);

        std::vector<Document> answer1 = search_server.FindTopDocuments("spider man and hulk"sv, DocumentStatus::IRRELEVANT);
        ASSERT_EQUAL(answer1[0].id, id1);
    }
}

void TestIsCorrectRelevance() {
    const int id1 = 3;
    std::string_view content1 = "spider man and doctor stiven strange with hulk"sv;
    const std::vector<int> ratings1 = {4, 5, 6, 5};
    const int id2 = 2;
    std::string_view content2 = "scooby dooby man our pretty fan you should finger flip pa-pa-pam"sv;
    const std::vector<int> ratings2 = {1, 2, 4};
    const int id3 = 1;
    std::string_view content3 = "pretty woman with hulk"sv;
    const std::vector<int> ratings3 = {4, 4, 4};

    {
        SearchServer search_server(""sv);
        search_server.AddDocument(id1, content1, DocumentStatus::ACTUAL, ratings1);
        search_server.AddDocument(id2, content2, DocumentStatus::ACTUAL, ratings2);
        search_server.AddDocument(id3, content3, DocumentStatus::ACTUAL, ratings3);

        const int precise = 10e-6;

        std::vector<Document> answer = search_server.FindTopDocuments("spider man and hulk"sv);

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

void TestGetWordFrequencies() {
    {
        SearchServer search_server("and with"sv);

        int id = 75;
        std::string_view content = "funny pet and nasty rat"sv;
        DocumentStatus status = DocumentStatus::ACTUAL;
        std::vector<int> rating = {7, 2, 7};

        search_server.AddDocument(id, content, status, rating);
        std::map<std::string_view, double> word_freqs_at_document = {
            {"funny"sv, 0.25},
            {"nasty"sv, 0.25},
            {"pet"sv, 0.25},
            {"rat"sv, 0.25}
        };

        ASSERT_EQUAL(search_server.GetWordFrequencies(id), word_freqs_at_document);

        int non_existent_document_id = 1;
        std::map<std::string_view, double> empty_map{};
        ASSERT_EQUAL(search_server.GetWordFrequencies(non_existent_document_id), empty_map);
    }
}

void TestRemoveDocument() {
    {
        SearchServer search_server("and with"sv);

        int id = 75;
        std::string_view content = "funny pet and nasty rat"sv;
        DocumentStatus status = DocumentStatus::ACTUAL;
        std::vector<int> rating = {7, 2, 7};

        search_server.AddDocument(id, content, status, rating);
        search_server.RemoveDocument(75);

        int number_of_document = 0;

        ASSERT_EQUAL(search_server.GetDocumentCount(), number_of_document);
    }

}

void TestRemoveDuplicates() {
    {
        SearchServer search_server("and with"sv);
        search_server.AddDocument(75, "funny pet and nasty rat"sv, DocumentStatus::ACTUAL, {7, 2, 7});
        search_server.AddDocument(57, "rat nasty pet funny"sv, DocumentStatus::REMOVED, {5, 1});
        search_server.AddDocument(1, "rat nasty funny"sv, DocumentStatus::IRRELEVANT, {1});


        RemoveDuplicates(search_server);

        int number_of_document = 2;
        ASSERT_EQUAL(search_server.GetDocumentCount(), number_of_document);
    }
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestGetWordFrequencies);
    RUN_TEST(TestRemoveDocument);
    RUN_TEST(TestRemoveDuplicates);
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