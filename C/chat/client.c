#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdbool.h>



int connect_to_server(int socket_fd, const struct addrinfo *res, char *port, char *ip);
char *get_ip_localhost (int *res);

int main(void) {
    struct addrinfo hints;
    struct addrinfo *res;
    char port[5] = "9002";

    memset(&hints, 0, sizeof hints); // Обнуляем все байты в структуре
    //char *ip = "255.255.255.255";

    int status_ip = 0;
    char *ip = get_ip_localhost(&status_ip);
    //char *ip = "192.168.98.129" //
    if (status_ip != 0 & ip == NULL) {
        fprintf(stderr, "Error resolving the ip localhost\n");
        return 1;
    }

    printf("%s\n", ip);
    getaddrinfo(ip, port, &hints, &res);

    char ans[7] = "";
    int i = 0;
    while (true) {
        i++;
        printf("итерация %d\n", i);

        char msg[7] = "";

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
        printf("Введите сообщение для отправки серверу, не более 7 символов: ");
        scanf("%7s", msg);
        int sended = send(socket_fd, msg, strlen(msg), 0);
        if (sended == -1) {
            perror("send");
            return 1;
        }
        printf("Отправлено байт в сокет: %d\n", sended);
        //quit - выход для сервера и клиента
        if (strcmp(msg, "quit") !=0) {
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

    freeaddrinfo(res);
    free(ip);
    return 0;
}

char *get_ip_localhost(int *res){
    char hostbuffer[256];
    //получаем ip localhost'а
    if (gethostname(hostbuffer, sizeof(hostbuffer)) == -1) {
        perror("gethostname");
        *res= 1;
    }
    const struct hostent *he = gethostbyname(hostbuffer);
    if (he == NULL) {
        perror("gethostbyname");
        *res= 1;
        return NULL;
    }
    char *ip_address_local = inet_ntoa(*((struct in_addr*)he->h_addr_list[0]));
    char *ip_address = malloc(sizeof(*ip_address)+1);
    if (ip_address == NULL) {
        perror("malloc");
        *res= 1;
        return NULL;
    }
    memccpy(ip_address, ip_address_local, 0,strlen(ip_address_local));
    *res= 0;
    return ip_address;
}

int connect_to_server(int socket_fd, const struct addrinfo *res, char *port, char *ip) {
    printf("%s %s:%s%s", "Попытка подключения к ", ip, port, "\n");
    if (connect(socket_fd, res->ai_addr, res->ai_addrlen) == -1) {
        perror("connection error");
        return 1;
    }
    printf("Успешно подключились к серверу \n");
    return 0;
}
