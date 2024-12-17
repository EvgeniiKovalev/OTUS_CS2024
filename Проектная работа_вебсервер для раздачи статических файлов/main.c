#include <arpa/inet.h>
#include <ctype.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define BUFFER_SIZE (100*1024*1024)
#define MAX_HTTP_BUFFER_SIZE (2*1024*1024)

//структура с информацией о параметрах инициализации сервера
typedef struct {
    int port;
    char ipv4[INET_ADDRSTRLEN];
    int server_fd;
}init_server_data;

//структура с информацией о передаваемом клиенту файле
typedef struct {
    char * file_name;
    char * file_ext;
    size_t received_data;
} request_data;

//вернет расширение файла
const char *get_file_extension(const char *file_name) {
    const char *dot = strrchr(file_name, '.'); //вернет указатель на последнее вхождение точки в имя файла
    if (!dot || dot == file_name) { // если NULL или равна входной строке-параметру
        return "";
    }
    return dot + 1; //сдвигаем указатель на 1 элемент относительно последней точки, т.е. пропускаем ещё одну точку
}

// регистронезависмое сравнение строк
bool match_strings(const char *str1, const char *str2) {
    do {
        if (tolower(*str1) != tolower(*str2)){
            return false;
        }
    } while (*str1++ && *str2++);
    return true;
}

//возвращает наименование mime типа
const char *get_mime_type(const char *file_ext) {
    if (match_strings(file_ext, "html") || match_strings(file_ext, "htm")) {
        return "text/html\0";
    }
    if (match_strings(file_ext, "txt")) {
        return "text/plain\0";
    }
    if (match_strings(file_ext, "jpg") || match_strings(file_ext, "jpeg")) {
        return "image/jpeg\0";
    }
    if (match_strings(file_ext, "png")) {
        return "image/png\0";
    }
    return "application/octet-stream\0";
}

// удаление %20 из запрашиваемого URL адреса
int  url_decode(const char *src, char **result) {
    char * result_local = malloc(strlen(src) + 1);
    if (result_local == NULL) {
        perror("malloc");
        return 1;
    }

    int res_length = 0;
    do {
        //если нашли % заменяем его и следующие два справа от него символа на пробел, т.е. %20 на пробел
        if (*src == '%') {
            result_local[res_length++] = 32;//код символа пробел
            *src++;//пропуск 1 символа
            *src++;//пропуск 1 символа
        } else {result_local[res_length++] = *src;}
    } while(*src++);

    result_local[res_length++] = '\0';
    *result = result_local;

    return 0;
}

//добавит в текст ответа текст на 404 ошибку
void response_404(char *response_text,       //указатель с выделенной памятью на текст ответа
                  size_t *response_len) {    //указатель на текущую длину ответа
    //длина ответа в байтах
    char *response404 = "HTTP/1.1 404 Not Found\r\n"
                        "Content-Type: text/plain\r\n"
                        "\r\n"
                        "404 Not Found\0";
    size_t response404_len = strlen(response404);
    *response_len += response404_len;
    memcpy(response_text, response404, response404_len);
}

//вернет заголовок HTTP ответа
char * header_response(const char *file_ext)
{
    const char * mime_type = get_mime_type(file_ext);
    const char * header_shablon =   "HTTP/1.1 200 OK\r\n"
                                    "Content-Type: %s\r\n"
                                    "\r\n\0";
    const size_t max_header_len = strlen(header_shablon) + strlen(mime_type);
    char *header = (char *)malloc(max_header_len * sizeof(char));
    if (header == NULL) {
        perror("malloc");
        return NULL;
    }
    snprintf(header, max_header_len, header_shablon,mime_type);
    return header;
}

//Генерит HTTP ответ
void http_response(request_data * p_request_data ,
                         char *response,
                         size_t *response_len) {

    // если файл не найден, то ответ = 404 Not Found
    int file_fd = open(p_request_data->file_name, O_RDONLY);
    if (file_fd == -1) {
        response_404(response, response_len);
        return;
    }

    // HTTP заголовок
    p_request_data->file_ext = get_file_extension(p_request_data->file_name);    // расширение файла
    char * header = header_response(p_request_data->file_ext);
    // копируем заголовок в ответ
    *response_len = 0;
    memcpy(response, header, strlen(header));
    *response_len += strlen(header);
    free(header);

    // чтение файла в буфер, читаем не более чем осталось свободного места в буфере,
    // т.к. часть буфера уже потрачено на заголовок http
    // выполняется не буферизированное чтение (операционной системой)
    ssize_t bytes_read;
    while ((bytes_read = read(file_fd, response + *response_len, BUFFER_SIZE - *response_len)) > 0) {
        *response_len += bytes_read;
    }
    close(file_fd);
}

//вернет массив под результат поиска по регулярке
bool regex_match(char *source_string,  char *pattern, regmatch_t matches[], size_t num_matches) {
    if (pattern == NULL) {
        return false;
    }
    regex_t regex;
    //компиляция регулярного выражения
    regcomp(&regex, pattern, REG_EXTENDED);
    //выполнение регулярного выражения
    if (regexec(&regex, source_string, 2, matches, 0) == 0) {
        return true;
    }
    //освобождение памяти выделенной при компиляции регулярного выражения
    regfree(&regex);
    return false;
}

//проверка запроса клиента является ли get запросом
request_data * read_and_check_input_request(int *client_fd) {
    //выделяем память под запрос клиента
    char *buffer_received = (char*)malloc(MAX_HTTP_BUFFER_SIZE * sizeof(char));
    if (buffer_received == NULL) {
        perror("malloc");
        return NULL;
    }

    // чтение запроса от клиента в буфер
    ssize_t bytes_received = recv(*client_fd, buffer_received, BUFFER_SIZE, 0);
    if (bytes_received < 0) {
        perror("check_input_request.recv");
        return NULL;
    }
    //выделяем память под структуру с информацией о передаваемом клиенту файле
    request_data * result = (request_data*)malloc(sizeof(request_data));
    if (result == NULL) {
        perror("malloc result");
    }
    result->received_data = bytes_received;
    printf("Received %ld bytes\n", bytes_received);

    // проверка является ли запрос get запросом
    //пример отформатированного запроса(одна строка разделена на несколько)
    //"GET /home.html HTTP/1.1\r\nHost: localhost:9002\r\nConnection: keep-alive\r\nCache-Control:
    //max-age=0\r\nsec-ch-ua: \"Google Chrome\";v=\"131\", \"Chromium\";v=\"131\", \"Not_A Brand\";
    //v=\"24\"\r\nsec-ch-ua-mobile: ?0\r\nsec-ch-ua-platform: \"Linux\"\r\nUpgrade-Insecure-Requests: 1\r
    //\nUser-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko)
    //Chrome/131.0.0.0 Safari/537.36\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,
    //image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7\r\nSec-Fetch-Site:
    //none\r\nSec-Fetch-Mode: navigate\r\nSec-Fetch-User: ?1\r\nSec-Fetch-Dest: document\r\nAccept-Encoding: gzip, deflate,
    //br, zstd\r\nAccept-Language: en-US,en;q=0.9\r\n\r\n"



    size_t num_matches = 2;
    regmatch_t matches[num_matches]; //массив под результат поиска по регулярке
    if (regex_match(buffer_received, "^GET /([^ ]*) HTTP/1", matches, num_matches) == false) {
        free(buffer_received);
        return NULL;
    }

    // извлечем имя файла из запроса клиента и заменим %20 на пробел
    buffer_received[matches[1].rm_eo] = '\0';
    const char *url_encoded_file_name = buffer_received + matches[1].rm_so;

    if (url_decode(url_encoded_file_name, &result->file_name) == EXIT_FAILURE) {
        perror("url_decode");
        free(buffer_received);
        return NULL;
    }

    free(buffer_received);
    return result;
}

// обработка установленного соединения с клиентом
void *handle_client(int *client_fd) {

    request_data * p_request_data = read_and_check_input_request(client_fd);
    if (p_request_data == NULL) {
        return NULL;
    }

    //HTTP ответ, для упрощения сразу выделяем максимальное количество памяти под ответ клиенту
    char *response = (char *)malloc(BUFFER_SIZE  * sizeof(char));
    if (response == NULL) {
        perror("malloc");
        return NULL;
    }
    size_t response_len = 0;
    http_response(p_request_data ,response, &response_len);

    //отправка HTTP ответа клиенту
    size_t send_len = send(*client_fd, response, response_len, 0);
    free(response);

    if (send_len == -1) {
        perror("send fail");
    }
    if (send_len != response_len) {
        perror("not all data was transmitted");
    }
    printf("Sending %lu bytes\n", send_len);

    free(p_request_data);
    close(*client_fd);
    return NULL;
}

//инициализация HTTP сервера
init_server_data * init_server(const int port, const int reuse_port, const int num_connections, char * error_message) {
    //выделяем память под структуру для инициализации сервера
    init_server_data * result = (init_server_data*)malloc(sizeof(init_server_data));
    if (result == NULL) {
        *error_message = "malloc init_server";
        return NULL;
    }

    result->port = port;

    //создание сокета
    result->server_fd = socket(
            AF_INET,       /* TCP IP VERSION 4 - IPv4 */
            SOCK_STREAM,     /* TCP (Stream Sockets) */
            IPPROTO_TCP); /* TCP PROTOCOL */
    if (result->server_fd == -1) {
        *error_message = "socket failed";
        return NULL;
    }

    //настраиваем сокет чтобы после падения приложения порт можно было повторно использовать, обычно порт ещё некоторое время занят
    if (setsockopt(result->server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_port, sizeof(reuse_port)) < 0) {
        *error_message = "SO_REUSEADDR failed";
        return NULL;
    }

    // настройка сокета
    struct sockaddr_in server_addr = {  .sin_family = AF_INET,
                                        .sin_port = htons(result->port),
                                        .sin_addr = { htonl(INADDR_ANY) },   //на всех интерфейсах системы
                                        //.sin_addr = { htonl(INADDR_LOOPBACK) },    //127.0.0.1
                                        };

    // привязка сокета к порту
    if (bind(result->server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
        *error_message = "bind failed";
        return NULL;
    }

    // ожидание подключения клиента, в очереди не более 10 подключений
    if (listen(result->server_fd, num_connections) != 0) {
        *error_message = "listen failed";
        return NULL;
    }

    //преобразование в строку выбранного IP
    inet_ntop(AF_INET, &(server_addr.sin_addr), &result->ipv4, INET_ADDRSTRLEN);
    return result;
}

void main(void) {
    char *error_message= NULL;
    printf("Starting server...");
    init_server_data * server_data = init_server(9002, 1, 10, error_message);
    if (server_data == NULL) {
        printf("errors init server: %s", error_message);
        perror("init_server");
        exit(EXIT_FAILURE);
    }
    printf("successfully\n");
    printf("Server listening on ip:port %s:%d \n", server_data->ipv4, server_data->port);

    int count_cycle = 0;
    do {
        printf("Итерация %d\n", count_cycle);
        // информация о клиенте
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int *client_fd = malloc(sizeof(int));

        // принятие подключения клиента
        // проверка на отрицательное значение *client_fd и присваивание значения *client_fd происходит
        // внутри условия if
        if ((*client_fd = accept(server_data->server_fd, (struct sockaddr *)&client_addr, &client_addr_len)) < 0) {
            perror("accept failed");
            continue;
        }

        // обработка запроса клиента
        //без использования потоков
        //handle_client(client_fd);

        //c использованием потоков
        pthread_t thread_id;
        //создание POSIX потока
        if (pthread_create(&thread_id, NULL, handle_client, client_fd) != 0) {
            perror("pthread_create failed");
        }
        //отсоединение потока, завершится по завершении работы handle_client
        if (pthread_detach(thread_id) != 0) {
            perror("pthread_detach failed");
        }
        count_cycle++;
    } while (count_cycle < 20);

    close(server_data->server_fd); //закрываем сокет
}