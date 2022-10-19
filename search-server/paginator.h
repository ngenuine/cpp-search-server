#pragma once
#include <iostream>
#include <vector>

template <typename Iterator>
class Page {
public:

    Page(Iterator begin, Iterator end) : begin_(begin), end_(end) {}

    Iterator begin() const { return begin_; }
    Iterator end() const { return end_; }
    int size() const { return distance(end_, begin_); }

private:
    const Iterator begin_;
    const Iterator end_;
};

template <typename Iterator>
class Paginator {
public:
    Paginator(Iterator range_begin, Iterator range_end, int page_size) {

        auto page_start = range_begin;

        while (page_start != range_end) {
            
            auto page_end = page_start;
            if (distance(page_start, range_end) >= page_size) {
                advance(page_end, page_size);
            } else {
                page_end = range_end;
            }

            pages_.push_back(Page(page_start, page_end));
            
            page_start = page_end;
        }
    }

    auto begin() const { return pages_.begin(); }

    auto end() const { return pages_.end(); }

private:
    std::vector<Page<Iterator>> pages_;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}

template<typename Iterator>
std::ostream& operator<<(std::ostream& output, const Page<Iterator>& page) {

    for (auto it = page.begin(); it != page.end(); ++it)
    output << *it;

    return output;
}