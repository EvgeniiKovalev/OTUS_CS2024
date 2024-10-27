#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdbool.h>
#include <time.h>

int main(int argc, char *argv[]) {
    struct addrinfo hints;
    struct addrinfo *res;
    char port[5] = "9002";

    printf("%s %s%s", "старт, имя запущенного файла: ", argv[0],"\n");
    memset(&hints, 0, sizeof hints); // Обнуляем все байты в структуре

    hints.ai_family = AF_INET;       // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP (Stream Sockets)
    hints.ai_flags = AI_PASSIVE;     // Автоматическое определение нашего IP

    getaddrinfo(NULL,  port, &hints, &res);

    int socket_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if  (bind(socket_fd, res->ai_addr, res->ai_addrlen) == -1) {
        perror("bind");
        return 1;
    };
    if (listen(socket_fd, 2) == -1) {
        printf("Не удалось начать прослушивание\n");
        return 1;
    }
    printf("%s %s%s","Успешно начато прослушивание на порту", port,"\n");;

    struct addrinfo their_addr;
    socklen_t addr_size = sizeof their_addr;
    int i = 0;
    char ans[64] = "";
    while (true) {
        i++;
        printf(" i = %d\n", i);
        char msg[64] = "";
        int connection_fd = accept(socket_fd, (struct sockaddr *)&their_addr, &addr_size);
        if (connection_fd == -1) {
            printf("Не удалось открыть сокет для обслуживания входящего соединения с клиентом");
            return 1;
        }
        printf("Подключился клиент\n");;

        //установка таймаута на соединение подключившегося клиента к серверу
        int timeout = 7; //секунд
        time_t seconds = time(NULL);
        int received = recv(connection_fd, msg, 64, 0);
        if (received == -1) {
            printf("Ошибка чтения данных из сокета");
            return 1;
        }
        if (time(NULL) - seconds > timeout) {
            close(connection_fd);
            printf("Таймаут %d", timeout);
            continue; //ожидаем нового подключения
        }


        printf("длина сообщения = %lu \n", strlen(msg));
        printf("получено сообщение от клиента: %s\n", msg);
        int cmp = strcmp(msg, "quit");
        if (cmp !=0) {
            printf("введите сообщение для ответа клиенту, не более 64 символов:");
            scanf("%s", ans);
            if (send(connection_fd, ans, strlen(ans), 0) == -1) {
                perror("send");
                return 1;
            }

        }
        else {
            printf("Выходим, т.к. получена команда завершения работы сервера и клиента");
            close(connection_fd);
            freeaddrinfo(res); // Очищаем результат
            close(socket_fd);
            break;
        }
        close(connection_fd);
    }
    return 0;
}
