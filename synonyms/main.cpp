#include <cassert>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>

using namespace std;

class Synonyms {
public:
    void Add(const string& first_word, const string& second_word) {
        synonyms_[first_word].insert(second_word);
        synonyms_[second_word].insert(first_word);
    }

    size_t GetSynonymCount(const string& word) const {
        if (synonyms_.count(word) != 0) {
            return synonyms_.at(word).size();
        }
        return 0;
    }

    bool AreSynonyms(const string& first_word, const string& second_word) const {
        if (synonyms_.count(first_word) == 1) {
            if (synonyms_.at(first_word).count(second_word) == 1) {
                return true;
            }
        }
        return false;
    }

private:
    map<string, set<string>> synonyms_;
};

void TestAddingSynonymsIncreasesTheirCount() {
    Synonyms synonyms;
    assert(synonyms.GetSynonymCount("music"s) == 0);
    assert(synonyms.GetSynonymCount("melody"s) == 0);

    synonyms.Add("music"s, "melody"s);
    assert(synonyms.GetSynonymCount("music"s) == 1);
    assert(synonyms.GetSynonymCount("melody"s) == 1);

    synonyms.Add("music"s, "tune"s);
    assert(synonyms.GetSynonymCount("music"s) == 2);
    
    assert(synonyms.GetSynonymCount("tune"s) == 1);
    assert(synonyms.GetSynonymCount("melody"s) == 1);
}

void TestAreSynonyms() {
    Synonyms synonyms;

    assert(synonyms.GetSynonymCount("dog"s) == 0);
    assert(synonyms.GetSynonymCount("sobaken"s) == 0);

    synonyms.Add("dog"s, "sobaken"s);
    assert(synonyms.GetSynonymCount("dog"s) == 1);
    assert(synonyms.GetSynonymCount("sobaken"s) == 1);
    
    synonyms.Add("sobaken"s, "dog_as_friend"s);
    assert(synonyms.GetSynonymCount("sobaken"s) == 2);
    assert(synonyms.GetSynonymCount("dog_as_friend"s) == 1);
    assert(synonyms.GetSynonymCount("dog"s) == 1);

    assert(synonyms.AreSynonyms("dog"s, "dog_as_friend"s) == false);
    assert(synonyms.AreSynonyms("sobaken"s, "cat"s) == false);
    assert(synonyms.AreSynonyms("sobaken"s, "dog"s) == true);
}

void TestSynonyms() {
    TestAddingSynonymsIncreasesTheirCount();
    TestAreSynonyms();
}

int main() {

    TestSynonyms();

    Synonyms synonyms;

    string line;
    while (getline(cin, line)) {
        istringstream command(line);
        string action;
        command >> action;

        if (action == "ADD"s) {
            string first_word, second_word;
            command >> first_word >> second_word;
            synonyms.Add(first_word, second_word);
        } else if (action == "COUNT"s) {
            string word;
            command >> word;
            cout << synonyms.GetSynonymCount(word) << endl;
        } else if (action == "CHECK"s) {
            string first_word, second_word;
            command >> first_word >> second_word;
            if (synonyms.AreSynonyms(first_word, second_word)) {
                cout << "YES"s << endl;
            } else {
                cout << "NO"s << endl;
            }
        } else if (action == "EXIT"s) {
            break;
        }
    }
}