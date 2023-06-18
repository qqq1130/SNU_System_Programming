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
        *client_fdp = Accept(listenfd, &client_addr, &client_len); //second, third parameter can be NULL?
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
    char header[MAX_OBJECT_SIZE];
    char cache_buf[MAX_OBJECT_SIZE];
    char http_method[16];
    char http_version[16];
    char *host;
    char *port;
    char *path;
    int server_fd;
    int host_header_exists = 0;
    ssize_t n;
    ssize_t response_size = 0;

    Rio_readinitb(&client_rio, client_fd);
    if (!Rio_readlineb(&client_rio, buf, MAXBUF)) {
        return; /* reading client request failed */
    }
    sscanf(buf, "%s %s %s", http_method, uri, http_version);

    /* check if given uri exist inside the cache */
    pthread_rwlock_rdlock(&rwlock);
    node_t *temp_node;
    if (!(temp_node = search_cache(&cache, uri))) {
        /* if cache hit */
        Rio_writen(client_fd, temp_node->obj, temp_node->obj_size);
        pthread_rwlock_unlock(&rwlock);
        return;
    }
    pthread_rwlock_unlock(&rwlock);

    parse_uri(uri, &host, &port, &path);

    server_fd = Open_clientfd(host, port);
    Rio_readinitb(&server_rio, server_fd);

    sprintf(buf, "GET /%s HTTP/1.0\r\n", path);
    Rio_writen(server_fd, buf, strlen(buf));

    /* send headers */
    while ((n = Rio_readlineb(&client_rio, buf, MAXBUF))) {
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
	}
    if (!host_header_exists) {
        sprintf(buf, "Host: %s\r\n", host);
        strcat(header, buf);
    }
    strcat(header, "\r\n");
    Rio_writen(server_fd, header, strlen(header));

    /* receive response */
    while ((n = Rio_readlineb(&server_rio, buf, MAXBUF))) {
		Rio_writen(client_fd, buf, n);

        if (response_size + n + 1 <= MAX_OBJECT_SIZE) {
            strcpy(cache_buf + response_size, buf);
        }
        response_size += n;

		if (!strncmp(buf, "\r\n", 2)) {
            break;
        }
	}
    /* add to cache */
    pthread_rwlock_wrlock(&rwlock);
    if (response_size <= MAX_OBJECT_SIZE) {
        node_init(&cache, uri, cache_buf, response_size);
    }
    pthread_rwlock_unlock(&rwlock);
    Close(server_fd);
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