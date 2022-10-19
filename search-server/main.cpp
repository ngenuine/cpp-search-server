#include "document.h"
#include "paginator.h"
#include "request_queue.h"
#include "test_example_functions.h"

using namespace std::literals;

int main() {
    {
        SearchServer search_server("and in at"s);
        RequestQueue request_queue(search_server);
        search_server.AddDocument(1, "curly cat curly tail"s, DocumentStatus::ACTUAL, {7, 2, 7});
        search_server.AddDocument(2, "curly dog and fancy collar"s, DocumentStatus::ACTUAL, {1, 2, 3});
        search_server.AddDocument(3, "big cat fancy collar "s, DocumentStatus::ACTUAL, {1, 2, 8});
        search_server.AddDocument(4, "big dog sparrow Eugene"s, DocumentStatus::ACTUAL, {1, 3, 2});
        search_server.AddDocument(5, "big dog sparrow Vasiliy"s, DocumentStatus::ACTUAL, {1, 1, 1});
        // 1439 запросов с нулевым результатом
        for (int i = 0; i < 1; ++i) {
            request_queue.AddFindRequest("empty request"s);
        }
        /* // первый запрос удален, 1437 запросов с нулевым результатом
        request_queue.AddFindRequest("sparrow"s);
        // все еще 1439 запросов с нулевым результатом
        request_queue.AddFindRequest("curly dog"s);
        // новые сутки, первый запрос удален, 1438 запросов с нулевым результатом */
        request_queue.AddFindRequest("big collar"s, DocumentStatus::BANNED);
        std::cout << "Total empty requests: "s << request_queue.GetNoResultRequests() << std::endl;  // Total empty requests: 1437 
    }

    {
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
            std::cout << *page << std::endl;
            std::cout << "Page break"s << std::endl;
        }
    }

    TestSearchServer();
    std::cerr << "Search server testing finished"s << std::endl;

    return 0;
}