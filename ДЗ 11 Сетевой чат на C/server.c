#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdbool.h>
#define TIMEOUT_SEC 7

int handle_client(const int connection_fd, int timeout);

int main(int argc, char *argv[]) {
    struct addrinfo hints;
    struct addrinfo *res;
    char port[5] = "9002";

    printf("%s %s%s", "старт, имя запущенного файла: ", argv[0],"\n");
    memset(&hints, 0, sizeof hints); // Обнуляем все байты в структуре

    hints.ai_family = AF_INET;       // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP (Stream Sockets)
    hints.ai_flags = AI_PASSIVE;     // Автоматическое определение нашего IP

    struct addrinfo their_addr;
    socklen_t addr_size = sizeof their_addr;

    getaddrinfo(NULL,  port, &hints, &res);

    int socket_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (socket_fd == -1) {
        perror("socket");
        return 1;
    }

    if  (bind(socket_fd, res->ai_addr, res->ai_addrlen) == -1) {
        perror("bind");
        return 1;
    }
    if (listen(socket_fd, 2) == -1) {
        printf("Не удалось начать прослушивание\n");
        return 1;
    }
    printf("%s %s%s","Успешно начато прослушивание на порту: ", port,"\n");

    int i = 0;
    while (true) {
        i++;
        printf("итерация %d\n", i);
        int connection_fd = accept(socket_fd, (struct sockaddr *)&their_addr, &addr_size);
        if (connection_fd == -1) {
            printf("Не удалось открыть сокет для обслуживания входящего соединения с клиентом");
            return 1;
        }
        int res_handle_client = handle_client(connection_fd, TIMEOUT_SEC);
        if (res_handle_client == 3) {
            continue; //таймаут, итерация для нового подключения
        }
        if (res_handle_client == 1) {
            return res_handle_client;//ошибка
        }
        if (res_handle_client == 2) {
            printf("Выходим, т.к. получена команда завершения работы сервера и клиента");
            close(connection_fd);
            freeaddrinfo(res); // Очищаем результат
            close(socket_fd);
            break;
        }
    }
    return 0;
}

int handle_client(const int connection_fd, int timeout) {
    printf("Подключился клиент, установлен таймаут подключения: %d\n", timeout);
    char msg[7] = "";
    //установка таймаута на соединение подключившегося клиента к серверу
    time_t seconds = time(NULL);
    if (recv(connection_fd, msg, 7, 0) == -1) {
        printf("Ошибка чтения данных из сокета");
        return 1;
    }
    if (time(NULL) - seconds > timeout) {
        close(connection_fd);
        printf("Не уложились в таймаут: %d\n", timeout);
        return 3;
    }

    printf("длина сообщения = %d \n", strlen(msg));
    printf("получено сообщение от клиента: %s\n", msg);
    if (strcmp(msg, "quit") !=0) {
        char ans[7]= "";
        printf("введите сообщение для ответа клиенту, не более 7 символов:");
        scanf("%7s", ans);
        if (send(connection_fd, ans, strlen(ans), 0) == -1) {
            perror("send");
            return 1;
        }
    }
    else {
        return 2;//завершение по quit
    }
    return 0;
}