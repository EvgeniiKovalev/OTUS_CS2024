#include <stdio.h>

void main(void) {
  int a1 = 100;//загаданное число
  int a = 0;

  //printf("a1 = %d\n", a1);
  do {
    printf("Введите целое число не больше 32000: ");
    scanf("%d", &a);
    if (a < a1) {printf("Мешьше\n");}
    else {
      if (a > a1) {printf("Больше\n");}
      else {printf("Угадали!\n");}
    }
  }  while (a != a1);
}