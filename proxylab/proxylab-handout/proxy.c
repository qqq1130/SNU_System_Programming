#include <stdio.h>
#include <string.h>
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

int main(int argc, char* argv[]) {
    int listenfd;
    int *connfdp;

    socklen_t client_len;
    struct sockaddr client_addr;
    pthread_t tid;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
		exit(1);
    }

    listenfd = Open_listenfd(argv[1]);

    while(1) {
        client_len = sizeof(client_addr);
        connfdp = Malloc(sizeof(int));
        *connfdp = Accept(listenfd, &client_addr, &client_len); //second, third parameter can be NULL?
        Pthread_create(&tid, NULL, serve, (void *)connfdp);
    }
    return 0;
}

void *serve(void *connfdp) {
	int connfd = *((int *)connfdp);
	Pthread_detach(Pthread_self());
	Free(connfdp);
	proxy(connfd);
	Close(connfd);
	return NULL;
}

void proxy(int connfd) {
    rio_t client_rio;
    char buf[MAXBUF];
    char uri[MAXURI];
    char http_method[16];
    char http_version[16];
    char *host;
    char *port;
    char *path;

    Rio_readinitb(&client_rio, connfd);
    if (!Rio_readlineb(&client_rio, buf, MAXBUF)) {
        return; /* reading client request failed */
    }
    sscanf(buf, "%s %s %s", http_method, uri, http_version);

    parse_uri(uri, &host, &port, &path);
}

void parse_uri(char *uri, char **host, char **port, char **path) {
    static const char *http = "http://";
    const char *default_port = "80";
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