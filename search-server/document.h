#pragma once
#include <iostream>

enum class DocumentStatus {
        ACTUAL,
        IRRELEVANT,
        BANNED,
        REMOVED,
};

struct Document {

    Document();

    Document(const int given_id, const double given_relevance, const int given_rating);

    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

std::ostream& operator<<(std::ostream& output, const Document& doc);