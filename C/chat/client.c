#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdbool.h>
#include <unistd.h>

int connect_to_server(int socket_fd, const struct addrinfo *res, char *port, char *ip) {
    printf("%s %s:%s%s", "Попытка подключения к ", ip, port, "\n");
    if (connect(socket_fd, res->ai_addr, res->ai_addrlen) == -1) {
        perror("connection error");
        return 1;
    }
    printf("Успешно подключились к серверу \n");
    return 0;
}

int main() {
    struct addrinfo hints;
    struct addrinfo *res;
    char port[5] = "9002";
    char hostbuffer[256];

    memset(&hints, 0, sizeof hints); // Обнуляем все байты в структуре

    //получаем ip localhost'а
    if (gethostname(hostbuffer, sizeof(hostbuffer)) == -1) {
        perror("gethostname");
        return 1;
    }
    const struct hostent *he = gethostbyname(hostbuffer);
    if (he == NULL) {
        perror("gethostbyname");
        return 1;
    }
    char *ip = inet_ntoa(*((struct in_addr*)he->h_addr_list[0]));
    //char *ip = "192.168.98.129"; //пример ip отличного от localhost


    hints.ai_family = AF_INET;       // ipv4
    hints.ai_socktype = SOCK_STREAM; // TCP (Stream Sockets)

    getaddrinfo(ip, port, &hints, &res);

    char ans[64] = "";
    int i = 0;
    while (true) {
        i++;
        printf(" i = %d\n", i);

        char msg[64] = "";

        int socket_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (socket_fd == -1) {
            perror("socket");
            return 1;
        }
        if (connect_to_server(socket_fd, res, port, ip) != 0) {
            perror("connect");
            return 1;
        }
        //sleep(10); //проверка таймаута на стороне сервера
        printf("Введите сообщение для отправки серверу, не более 64 символов: ");
        scanf("%s", msg);
        int sended = send(socket_fd, msg, strlen(msg), 0);
        if (sended == -1) {
            perror("send");
            return 1;
        }
        printf("Отправлено байт в сокет %d\n", sended);
        //quit - выход для сервера и клиента
        int cmp = strcmp(msg, "quit");
        if (cmp !=0) {
            int received = recv(socket_fd, ans, sizeof(ans), 0);
            if (received == -1) {
                perror("recv");
                return 1;
            }
            printf("ответ сервера: %s \n", ans);
            close(socket_fd);
        }
        else {
            printf("Выходим, т.к. отправлена команда завершения работы сервера и клиента");
            close(socket_fd);
            break;
        }
    }

    freeaddrinfo(res); // Очищаем результат
    return 0;
}