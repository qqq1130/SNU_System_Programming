#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "csapp.h"
#include "cache.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define MAXURI 1024

void *serve(void *connfdp);
void proxy(int connfd);
void parse_uri(char *uri, char **host, char **port, char **path);
void forward_header(rio_t *rio, int connfd, char *uri);
void forward_response(rio_t *rio, int connfd, char *uri, char *path);


/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static cache_t cache;
static pthread_rwlock_t rwlock;

int main(int argc, char* argv[]) {
    int listenfd;
    int *client_fdp;

    socklen_t client_len;
    struct sockaddr client_addr;
    pthread_t tid;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
		exit(1);
    }

    cache_init(&cache);
    pthread_rwlock_init(&rwlock, NULL);

    listenfd = Open_listenfd(argv[1]);

    while(1) {
        client_len = sizeof(client_addr);
        client_fdp = Malloc(sizeof(int));
        *client_fdp = Accept(listenfd, &client_addr, &client_len);
        Pthread_create(&tid, NULL, serve, (void *)client_fdp);
    }
    return 0;
}

void *serve(void *client_fdp) {
	int client_fd = *((int *)client_fdp);
	Pthread_detach(Pthread_self());
	Free(client_fdp);
	proxy(client_fd);
	Close(client_fd);
	return NULL;
}

void proxy(int client_fd) {
    rio_t client_rio;
    rio_t server_rio;
    char buf[MAXBUF];
    char uri[MAXURI];
    char http_method[16];
    char http_version[16];
    char *host;
    char *port;
    char *path;
    int server_fd;

    Rio_readinitb(&client_rio, client_fd);
    if (!Rio_readlineb(&client_rio, buf, MAXBUF)) {
        return; /* reading client request failed */
    }
    sscanf(buf, "%s %s %s", http_method, uri, http_version);

    parse_uri(uri, &host, &port, &path);

    /* check if given uri exist inside the cache */
    pthread_rwlock_rdlock(&rwlock);
    node_t *temp_node;
    if ((temp_node = search_cache(&cache, uri, path))) {
        /* if cache hit */
        Rio_writen(client_fd, temp_node->obj, temp_node->obj_size);
        pthread_rwlock_unlock(&rwlock);
        return;
    }
    pthread_rwlock_unlock(&rwlock);

    server_fd = Open_clientfd(host, port);
    Rio_readinitb(&server_rio, server_fd);

    sprintf(buf, "GET /%s HTTP/1.0\r\n", path);
    Rio_writen(server_fd, buf, strlen(buf));

    /* send headers */
    forward_header(&client_rio, server_fd, host);

    /* receive response */
    forward_response(&server_rio, client_fd, uri, path);
}

void parse_uri(char *uri, char **host, char **port, char **path) {
    static const char *http = "http://";
    static char *default_port = "80";
    char *next_ptr;

    *host = uri + strlen(http);
    *host = strtok_r(*host, "/", &next_ptr);
    *path = strtok_r(NULL, "\0", &next_ptr);
    *host = strtok_r(*host, ":", &next_ptr);

    if (!(*port = strtok_r(NULL, ":", &next_ptr))) {
        *port = default_port;
    }

    return;
}

void forward_header(rio_t *rio, int connfd, char *host) {
    char buf[MAX_OBJECT_SIZE];
    char header[MAX_OBJECT_SIZE];
    int host_header_exists = 0;

    size_t size = 0;
    size_t read = 0;

    while ((size = Rio_readlineb(rio, buf, MAXBUF))) {
		if (!strncmp(buf, "\r\n", 2)) {
            break;
        }
        if (strstr(buf, "Connection:") == buf) {
            sprintf(buf, "Connection: close\r\n");
        } else if (strstr(buf, "Proxy-Connection:") == buf) {
            sprintf(buf, "Proxy-Connection: close\r\n");
        } else if (strstr(buf, "Host:") == buf) {
            host_header_exists = 1;
        }
        strcat(header, buf);
        read += size;
	}

    if (!host_header_exists) {
        sprintf(buf, "Host: %s\r\n", host);
        strcat(header, buf);
        read += size;
    }
    strcat(header, "\r\n");
    Rio_writen(connfd, header, read);
}

void forward_response(rio_t *rio, int connfd, char *uri, char *path){
	char buf[MAX_OBJECT_SIZE];
    char payload[MAX_OBJECT_SIZE];

    size_t size = 0;
    size_t read = 0;

	while ((size = Rio_readnb(rio, buf, MAXLINE))) {
		Rio_writen(connfd, buf, size);
        strcat(payload, buf);
        read += size;
	}

    pthread_rwlock_wrlock(&rwlock);
    node_init(&cache, uri, path, payload, read);
    pthread_rwlock_unlock(&rwlock);
	return;
}