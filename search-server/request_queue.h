#pragma once
#include <vector>
#include <string>
#include <deque>
#include "document.h"
#include "search_server.h"

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server);

    // сделаем "обёртки" для всех методов поиска, чтобы сохранять результаты для нашей статистики
    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
        
        QueryResult search_result = {search_s_.FindTopDocuments(raw_query, document_predicate)};
        QueryRevision(requests_, search_result);

        return search_result.content;
    }

    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus given_status = DocumentStatus::ACTUAL);

    int GetNoResultRequests() const;

private:

    struct QueryResult {
        std::vector<Document> content;
    };

    void QueryRevision(std::deque<QueryResult>& requests, const QueryResult& res);

    std::deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
    int empty_requests_ = 0;
    const SearchServer& search_s_;
};