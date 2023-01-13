#include "document.h"
using namespace std::literals;

Document::Document() = default;

Document::Document(const int given_id, const double given_relevance, const int given_rating)
    : id(given_id)
    , relevance(given_relevance)
    , rating(given_rating) {
        
    }

std::ostream& operator<<(std::ostream& output, const Document& doc) {

    output
    << "{ document_id = "sv << doc.id
    << ", relevance = "sv << doc.relevance
    << ", rating = "sv << doc.rating << " }"sv;

    return output;
}