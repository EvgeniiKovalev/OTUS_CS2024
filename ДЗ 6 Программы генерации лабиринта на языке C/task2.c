#include <stdio.h>
#include <stdlib.h>
#include <time.h>

void print_array(const unsigned char* array_char, const int length)
//вывод строки симолов на консоль
{
    for(int i = 0; i <= length-1; i++) {printf("%c", array_char[i]);};
    printf("%c\n", array_char[length]);
}

int main(void)
{
    int count_row = 100, count_col = 100; //колво клеток лабиринта по горизонтали и вертикали
    // размерность лабиринта в текстовой консоли с учетом, что клетка лабиринта и её границы это отдельный символ в консоли
    // символ # - стена, символ пробел - проход
    int maxy = 2*count_col+1;
    int maxx = 2*count_row+1;
    unsigned char string_labyrinth_to_console[maxx];     //строка лабиринта

    srand(time(NULL)); // Инициализация генератора случайных чисел
    // вывод лабиринта на экран
    for (int i = 0; i <= maxy; i++) {
        for (int j = 0; j <= maxx; j++) {
            if ((i == 0) || (i == maxy)) {
                string_labyrinth_to_console[j] = '#';
            }
            else if ((j == maxx) || (j == 0)) {
                string_labyrinth_to_console[j] = '#';
            }
            else if ((j > 0) || (j < maxx-1)) {
                    string_labyrinth_to_console[j] = rand() % 10 == 0 ? '#' : ' ';
            }
        }
        print_array(string_labyrinth_to_console, maxx);
    }
    return 0;
}