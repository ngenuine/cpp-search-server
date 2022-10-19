#include "request_queue.h"
#include "search_server.h"

RequestQueue::RequestQueue(const SearchServer& search_server) : search_s_(search_server) {}

// о, прикольно; параметр по умолчанию есть в объявлении -- следовательно в определении не нужен! иначе ошибка "default argument given for parameter"
// для прозрачности можно закомментить
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentStatus given_status /* = DocumentStatus::ACTUAL */) {
    return AddFindRequest(raw_query, [given_status](int document_id, DocumentStatus status, int rating) {return status == given_status;});
}

int RequestQueue::GetNoResultRequests() const {
    return empty_requests_;
}

void RequestQueue::QueryRevision(std::deque<QueryResult>& requests, const QueryResult& res) {
    requests.push_back(res);
    if (res.content.empty()) {
        ++empty_requests_;
    }

    if (requests.size() > min_in_day_) {
        if (requests.front().content.empty()) {
            --empty_requests_;
        }

        requests.pop_front();
    }
}