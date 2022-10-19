#pragma once
#include <string>
#include <iostream>
#include <vector>
#include <tuple>
#include <utility>
#include "search_server.h"

using namespace std::literals;

template<typename K, typename V>
std::ostream& operator<<(std::ostream& output, const std::pair<K, V>& p) {

    output << p.first << ": " << p.second;

    return output;
}

template<typename ContainerType>
void PrintContainer(std::ostream& output, const ContainerType& container) {
    bool is_first = true;
    for (auto elem : container) {
        
        if (!is_first) {
            output << ", ";
        }

        is_first = false;

        output << elem;
    }
}

template<typename K, typename V>
std::ostream& operator<<(std::ostream& output, const std::map<K, V>& m) {

    output << '{';
    PrintContainer(output, m);
    output << '}';

    return output;
}

template<typename T>
std::ostream& operator<<(std::ostream& output, const std::vector<T>& v) {

    output << '[';
    PrintContainer(output, v);
    output << ']';

    return output;
}

template<typename T>
std::ostream& operator<<(std::ostream& output, const std::set<T>& s) {

    output << '{';
    PrintContainer(output, s);
    output << '}';

    return output;
}

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const std::string& t_str, const std::string& u_str, const std::string& file,
                     const std::string& func, unsigned line, const std::string& hint) {
    if (t != u) {
        std::cerr << std::boolalpha;
        std::cerr << file << "("s << line << "): "s << func << ": "s;
        std::cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        std::cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            std::cerr << " Hint: "s << hint;
        }
        std::cerr << std::endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const std::string& expr_str, const std::string& file, const std::string& func, unsigned line,
                const std::string& hint);

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename ReturnedType>
void RunTestImpl(ReturnedType func, const std::string& func_name) {
    func();
    std::cerr << func_name << " OK"s << std::endl;
}

#define RUN_TEST(func) RunTestImpl((func), #func)

// -------- Начало модульных тестов поисковой системы ----------

void TestAddingDocuments();

void TestExcludingStopWords();

void TestExcludingMinusWords();

void TestMatchingDocument();

void TestSortingByRelevance();

void TestSortingByRating();

void TestCalculateRating();

void TestPredicateAsFilter();

void TestGivenStatusAsFilter();

void TestIsCorrectRelevance();

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer();

// --------- Окончание модульных тестов поисковой системы -----------