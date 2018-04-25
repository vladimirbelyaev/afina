#include "Worker.h"

#include <iostream>

#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <netdb.h>
#include <cstring>
#include <protocol/Parser.h>
#include <afina/execute/Command.h>
#include <map>

#include "Utils.h"
#define MAXEVENTS 100
#define EPOLLEXCLUSIVE 1 << 28
namespace Afina {
namespace Network {
namespace NonBlocking {

// See Worker.h
Worker::Worker(std::shared_ptr<Afina::Storage> ps): pStorage(ps) {
}

// See Worker.h
Worker::~Worker() {
}
void *Worker::OnRunProxy(void *p) {
    //Worker *srv = reinterpret_cast<Worker *>(p);
    auto data = reinterpret_cast<std::pair<Worker*,int>*>(p);
    try {
        std::cout << "In onrunproxy server socket is " << data->second << "\n";
        data->first->OnRun(&data->second);
    } catch (std::runtime_error &ex) {
        std::cerr << "Server fails: " << ex.what() << std::endl;
    }
    return nullptr;
}
// See Worker.h
void Worker::Start(int server_socket) {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    //running.store(true);
    socket = server_socket;
    std::cout << "In start socket is " << socket << "\n";
    auto data = new std::pair<Worker*,int>(this, socket);
    if (pthread_create(&thread, NULL, OnRunProxy, data) < 0) {
        throw std::runtime_error("Could not create server thread");
    }
    std::cout << "New worker at serv_sock_descriptor " << data->second << std::endl;
}

// See Worker.h
void Worker::Stop() {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    //running.store(false);
}

// See Worker.h
void Worker::Join() {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    pthread_join(thread, nullptr);
}

void Worker::SetGetRoutine(int socket, char* buffer, int* sizeofdata){
    auto buf_size = 1024;
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    Protocol::Parser parser;
    //char buffer[buf_size+1];
    std::string out;
    size_t parsed = 0;
    size_t curr_pos = 0;
    ssize_t n_read = 0;
    bool serv_disconnect = false;
    //try {
    //if(!running.load()) return;
    curr_pos = *sizeofdata;
    std::cout << "Buffer at first contains [" << buffer << "]\n";

    std::cout << "Start of loop\n";
    if ((n_read = recv(socket, buffer + curr_pos, buf_size, 0)) > 0 || parsed < curr_pos) {
        curr_pos += n_read;
        bool is_parsed = parser.Parse(buffer, curr_pos, parsed);
        if (is_parsed) {
            size_t body_read = curr_pos - parsed;//body_read - сколько дочитали
            memcpy(buffer, buffer + parsed, body_read);
            memset(buffer + body_read, 0, parsed); // Убираем все, что было связано с командой.
            curr_pos = body_read;

            //Сбор команды и аргументов
            uint32_t body_size;
            auto cmd = parser.Build(body_size);
            while (body_size > curr_pos) {
                n_read = recv(socket, buffer + curr_pos, buf_size, 0);
                curr_pos += n_read;
                if (n_read < 0) {
                    throw std::runtime_error("\nUser disconnected\n");
                }
            }
            char args[body_size + 1];
            memcpy(args, buffer, body_size);
            args[body_size] = '\0';
            // Запускаем
            try {
                cmd->Execute(*(pStorage.get()), args, out);
                out += "\r\n";
            } catch (std::runtime_error &err) { // Ошибка внутри поймается и отправится клиенту
                out = std::string("SERVER_ERROR : ") + err.what() + "\r\n";
            }

            if (body_size) {
                memcpy(buffer, buffer + body_size + 2, curr_pos - body_size - 2);
                memset(buffer + curr_pos - body_size - 2, 0, body_size);
                curr_pos -= body_size + 2;
            }
            if (send(socket, out.data(), out.size(), 0) <= 0) {
                throw std::runtime_error("Socket send() failed\n");
            }
            parser.Reset();

        }
        /*if (!running.load()) {
            serv_disconnect = true;
            break;
        }*/
    }else{
        //close(socket);
        //throw std::runtime_error("User disconnected");
    }
    *sizeofdata = curr_pos;

}

// See Worker.h
void* Worker::OnRun(void *args) {
        std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
        struct epoll_event ev;
        struct epoll_event events[MAXEVENTS];
        int res, events_catched, s;
        int efd = epoll_create(0xCAFE);
        std::map<int,char[1024]> fd_data;
        std::map<int,int> fd_len;
        if (efd == -1) {
            throw std::runtime_error("epoll_create");
        }
        socket = *reinterpret_cast<int*>(args);
        ev.data.fd = socket;
        std::cout << "Socket in onrun is " << socket << "\n";
        ev.events = EPOLLEXCLUSIVE | EPOLLIN | EPOLLHUP | EPOLLERR;
        res = epoll_ctl(efd, EPOLL_CTL_ADD, ev.data.fd, &ev);
        if (res == -1) {
            throw std::runtime_error("epoll ctl");
        }
        //while (this->running.load()) {
        std::cout << "Waiting for data\n";
        while(1){
            if ((events_catched = epoll_wait(efd, events, MAXEVENTS, -1)) == -1) {
                throw std::runtime_error("epoll wait");
            }
            std::cout << "SOME EVENTS CATCHED: "<< events_catched <<"\n";
            std::cout << "Epollerr " << EPOLLERR << std::endl;
            std::cout << "Epollhup " << EPOLLHUP << std::endl;
            std::cout << "Epollin " << EPOLLIN << std::endl;
            for (int i = 0; i < events_catched; i++) {
                std::cout << "Event " << events[i].events << std::endl;
                if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || (!(events[i].events & EPOLLIN))) {
                    /* An error has occured on this fd, or the socket is not
                       ready for reading (why were we notified then?) */
                    std::cout << "User on socket " << events[i].data.fd << " disconnected\n";
                    //fd_data[events[i].data.fd] = "";
                    fd_len[events[i].data.fd] = 0;
                    fprintf(stderr, "epoll error\n");
                    close(events[i].data.fd);
                    continue;
                } else if (socket == events[i].data.fd) {
                    /* We have a notification on the listening socket, which
                       means one or more incoming connections. */
                    while (1) {
                        struct sockaddr in_addr;
                        socklen_t in_len;
                        int infd;
                        char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

                        in_len = sizeof in_addr;
                        infd = accept(socket, &in_addr, &in_len);
                        if (infd == -1) {
                            if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                                /* We have processed all incoming
                                   connections. */
                                break;
                            } else {
                                perror("accept");
                                break;
                            }
                        }

                        s = getnameinfo(&in_addr, in_len, hbuf, sizeof hbuf, sbuf, sizeof sbuf,
                                        NI_NUMERICHOST | NI_NUMERICSERV);
                        if (s == 0) {
                            printf("Accepted connection on descriptor %d "
                                           "(host=%s, port=%s, epoll=%d)\n",
                                   infd, hbuf, sbuf, efd);
                        }

                        /* Make the incoming socket non-blocking and add it to the
                           list of fds to monitor. */
                        make_socket_non_blocking(infd);
                        if (s == -1)
                            abort();

                        ev.data.fd = infd;
                        ev.events = EPOLLIN | EPOLLET;
                        s = epoll_ctl(efd, EPOLL_CTL_ADD, infd, &ev);
                        if (s == -1) {
                            perror("epoll_ctl");
                            abort();
                        }

                    }
                    continue;
                } else { // We've got some new data. Process it, bitch!
                    std::cout << "Some data on fd " << events[i].data.fd << std::endl;
                    /* \\Debug
                    int n_read;
                    size_t max_len = 1024;
                    char readed[max_len];
                    if (n_read = recv(events[i].data.fd,readed,max_len,0) < 0){
                        close(events[i].data.fd);
                        std::cout << "Socket " << events[i].data.fd << " is closed with recv()\n";
                    }
                    std::cout << "Got data: [" << readed << "]\n";
                    memset(readed,0, max_len);*/
                    try {
                        SetGetRoutine(events[i].data.fd, fd_data[events[i].data.fd], &fd_len[events[i].data.fd]);
                        std::cout << "Unparsed data [" << fd_data[events[i].data.fd] << "]\n";
                    }catch (std::runtime_error &err){
                        std::cout << err.what() << "- error on fd " << events[i].data.fd << std::endl;
                        std::cout << "Unparsed data [" << fd_data[events[i].data.fd] << "]\n";
                        continue;
                    }
                    continue;
                    //close(events[i].data.fd);
                }

            }
        }

    }

} // namespace NonBlocking
} // namespace Network
} // namespace Afina
