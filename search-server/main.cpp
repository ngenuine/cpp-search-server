// Решите загадку: Сколько чисел от 1 до 1000 содержат как минимум одну цифру 3?
// Напишите ответ здесь: 271

// Закомитьте изменения и отправьте их в свой репозиторий.

#include <iostream>
#include <string>

using namespace std;

int main() {
    int counter = 0;

    for (int num = 0; num <= 1000; ++num) {
        string number = to_string(num);
        for (char c : number) {
            if (c == '3') {
                ++counter;
                break;
            }
        }
    }

    cout << counter << endl; // 271 тройка, пишет :)

}