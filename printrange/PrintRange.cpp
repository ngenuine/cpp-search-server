#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>

using namespace std;

template <typename It>
void PrintRange(It range_begin, It range_end) {
    if (range_begin != range_end) {
        for (auto it = range_begin; it != range_end; ++it) {
            cout << *it << " "s;
        }

    }
    cout << endl;
}

template <typename С, typename P>
void FindAndPrint(С container, P pivot) {
    auto start = container.begin();
    auto end = container.end();

    if (start != end && *start != pivot) {
        for (auto element : container) {
            if (element == pivot) {
                break;
            }
            cout << element << " "s;
            ++start;
        }

        cout << endl;
        
    }
    
    PrintRange(start, end);
}

int main() {
    set<int> test1 = {1, 1, 1, 2, 3, 4, 5, 5};
    cout << "Test1.1"s << endl;
    FindAndPrint(test1, 3);
    cout << "Test1.2"s << endl;
    FindAndPrint(test1, 0); // элемента 0 нет в контейнере
    vector<int> test2 = {10, 9, 8};
    cout << "Test2.1"s << endl;
    FindAndPrint(test2, 9);
    cout << "Test2.2"s << endl;
    FindAndPrint(test2, 10);
    cout << "Test2.3"s << endl;
    FindAndPrint(test2, 8);
    cout << "Test2.4"s << endl;
    FindAndPrint(test2, 7);
    string test3 = "abacabn";
    cout << "Test3.1"s << endl;
    FindAndPrint(test3, 'a');
    cout << "Test3.2"s << endl;
    FindAndPrint(test3, 'n');
    cout << "Test3.3"s << endl;
    FindAndPrint(test3, 'g');
    vector<int> test4 = {}; // пустой контейнер
    cout << "Test4.1"s << endl;
    FindAndPrint(test4, 0);
    cout << "Test4.2"s << endl;
    FindAndPrint(test4, 1);
    cout << "Test4.3"s << endl;
    FindAndPrint(test4, 2);
    vector<double> test5 = {10.1, 9.2, 8.3};
    cout << "Test5.1"s << endl;
    FindAndPrint(test5, 10.1);
    cout << "Test5.2"s << endl;
    FindAndPrint(test5, 9.2);
    cout << "Test5.3"s << endl;
    FindAndPrint(test5, 8.3);
    cout << "Test5.4"s << endl;
    FindAndPrint(test5, 12.1);
    cout << "End of tests"s << endl;
}