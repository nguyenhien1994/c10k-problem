#include <bits/stdc++.h>
#include <netdb.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "BlockingQueue.h"
#include "Expression.h"

#define BUFFERSIZE (8 * 1024)

/* Used by acceptorb threads */
typedef struct {
    int nbrCores;
    int sockFd;
    int nbrThreads;
} acceptorArgs;

/* Used by client handler threads */
typedef struct {
    int nbrCores;
    BlockingQueue<int> queue;
    std::vector<pthread_t> clients;
} handlerArgs;

std::vector<pthread_t> acceptorThreads;

/* Clean-up when receiving a INT signal */
static void* sigHandler(void* arg) {
    sigset_t* set = (sigset_t*)arg;
    int sig;

    /* Wait for the INT signal */
    if (sigwait(set, &sig) != 0) {
        std::cerr << "sigwait" << std::endl;
        exit(EXIT_FAILURE);
    }

    printf("Caught signal %d, good bye!\n", sig);

    /* Cleanup threads */
    for (auto& thread : acceptorThreads) {
        pthread_cancel(thread);
        pthread_join(thread, NULL);
    }

    return NULL;
}

int openTCPSocket(char* port) {
    int sockFd;
    struct addrinfo hints;
    struct addrinfo* servinfo;
    struct addrinfo* tmpinfo;
    int reuse = 1;
    int rv;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
        std::cerr << "getaddrinfo: " << gai_strerror(rv) << std::endl;
        return -1;
    }

    /* getaddrinfo() returns a list of address structures.
     * Try each address until we successfully bind(2).
     * If socket(2) (or bind(2)) fails, we (close the socket
     * and) try the next address.
     */
    for (tmpinfo = servinfo; tmpinfo != NULL; tmpinfo = tmpinfo->ai_next) {
        if ((sockFd = socket(tmpinfo->ai_family, tmpinfo->ai_socktype, tmpinfo->ai_protocol)) ==
            -1) {
            std::cerr << "socket: " << errno << std::endl;
            continue;
        }

        if (setsockopt(sockFd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)) == -1) {
            std::cerr << "setsockopt: " << errno << std::endl;
            return -1;
        }

        if (bind(sockFd, tmpinfo->ai_addr, tmpinfo->ai_addrlen) == -1) {
            ::close(sockFd);
            std::cerr << "bind: " << errno << std::endl;
            continue;
        }

        break;
    }

    /* No address succeeded */
    if (tmpinfo == NULL) {
        fprintf(stderr, "failed to bind\n");
        std::cerr << "failed to bind: " << errno << std::endl;
        return -1;
    }

    /* No longer needed */
    freeaddrinfo(servinfo);

    return sockFd;
}

void* clientHandler(void* arg) {
    handlerArgs* h_args = (handlerArgs*)arg;
    assert(h_args);
    int clientFd = 0;

    while (1) {
        try {
            /* Wait until there is available client pushed to the queue */
            clientFd = h_args->queue.pop();
        } catch (const InterruptedException& e) {
            /* Got interrupt exception, return */
            return NULL;
        }

        auto buffer = std::unique_ptr<char>(new char[BUFFERSIZE]);
        ssize_t nDataLength;
        std::string exprStr;

        /* Receive full expression before calculating */
        while ((nDataLength = ::read(clientFd, buffer.get(), BUFFERSIZE)) > 0) {
            if (nDataLength < 0) {
                std::cerr << "failed to read: " << errno << std::endl;
                break;
            }
            exprStr.append(buffer.get(), nDataLength);

            /* The expression short than BUFFERSIZE */
            if (nDataLength < BUFFERSIZE) {
                break;
            }
        }

        /* Calculate the expression and send back the result to client */
        if (exprStr.length() > 0) {
            try {
                auto expr = Expression(exprStr);

                auto result = std::to_string(expr.getResult()) + '\n';
                // printf("exprStr: %s = %s\n", exprStr.c_str(), result);

                /* Send back the result */
                if (write(clientFd, result.c_str(), result.length()) <= 0) {
                    std::cerr << "failed to write: " << errno << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << e.what() << std::endl;
            }
        }
        ::close(clientFd);
    }

    return NULL;
}

void threadCleanup(void* arg) {
    handlerArgs* h_args = (handlerArgs*)arg;
    assert(h_args);

    /* Set the interrupt flag to true and notify all threads that waiting the queue */
    h_args->queue.interrupt();
}

void* acceptorHandler(void* arg) {
    acceptorArgs* a_args = (acceptorArgs*)arg;
    assert(a_args);

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN);

    handlerArgs h_args;
    h_args.nbrCores = a_args->nbrCores;

    pthread_cleanup_push(threadCleanup, &h_args);

    /* Create threads to handler client data */
    for (int i = 0; i < a_args->nbrThreads; ++i) {
        pthread_t t;
        if (pthread_create(&t, &attr, clientHandler, (void*)&h_args) < 0) {
            std::cerr << "pthread_create" << std::endl;
            exit(EXIT_FAILURE);
        }

        h_args.clients.push_back(t);
    }

    printf("acceptorHandler %d is ready\n", a_args->nbrCores);

    struct addrinfo clientAddr;
    socklen_t addrLen = sizeof(clientAddr);

    /* Wait to accept the new connection and push to the queue */
    while (1) {
        int connection = accept(a_args->sockFd, (struct sockaddr*)&clientAddr, &addrLen);

        if (connection < 0) {
            std::cerr << "accept" << std::endl;
            continue;
        }

        h_args.queue.push(connection);
    }

    pthread_cleanup_pop(0);

    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "USAGE: " << argv[0] << " <port> <threads-per-nbrCores>" << std::endl;
        return EXIT_FAILURE;
    }

    pthread_t thread;
    sigset_t set;

    sigemptyset(&set);
    sigaddset(&set, SIGINT);

    if (pthread_sigmask(SIG_BLOCK, &set, NULL) < 0) {
        std::cerr << "pthread_sigmask: " << errno << std::endl;
        return EXIT_FAILURE;
    }

    if (pthread_create(&thread, NULL, sigHandler, (void*)&set) < 0) {
        std::cerr << "pthread_create: " << errno << std::endl;
        return EXIT_FAILURE;
    }

    int nbrThreads = atoi(argv[2]);

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    /* Set per-thread stack to MIN to handle the connection as much as possible */
    pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN);

    /* Open TCP socket with port */
    int sockFd = openTCPSocket(argv[1]);
    if (sockFd == -1) {
        return EXIT_FAILURE;
    }

    if (listen(sockFd, SOMAXCONN) == -1) {
        std::cerr << "listen: " << errno << std::endl;
        return EXIT_FAILURE;
    }

    /* Get number of cores */
    int nbrCores = sysconf(_SC_NPROCESSORS_ONLN);
    acceptorArgs args[nbrCores];

    for (int i = 0; i < nbrCores; ++i) {
        args[i].nbrCores = i;
        args[i].sockFd = sockFd;
        args[i].nbrThreads = nbrThreads;

        pthread_t t;
        /* Create threads to accept client connection */
        if (pthread_create(&t, &attr, acceptorHandler, (void*)&args[i]) < 0) {
            std::cerr << "pthread_create" << std::endl;
            exit(EXIT_FAILURE);
        }

        acceptorThreads.push_back(t);
    }

    for (auto& thread : acceptorThreads) {
        pthread_join(thread, NULL);
    }

    close(sockFd);

    return EXIT_SUCCESS;
}