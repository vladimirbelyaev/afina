#include "ServerImpl.h"

#include <cassert>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>

#include <pthread.h>
#include <signal.h>

#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

#include <afina/Storage.h>
#include <protocol/Parser.h>
#include <afina/execute/Command.h>

namespace Afina {
namespace Network {
namespace Blocking {

void *ServerImpl::RunAcceptorProxy(void *p) {
    ServerImpl *srv = reinterpret_cast<ServerImpl *>(p);
    try {
        srv->RunAcceptor();
    } catch (std::runtime_error &ex) {
        std::cerr << "Server fails: " << ex.what() << std::endl;
    }
    return 0;
}

void *ServerImpl::RunConnectionProxy(void *p){
    ServerImpl* server;
    int socket;
    std::tie(server, socket) = *reinterpret_cast<std::pair<ServerImpl*, int>*>(p);
    try {
        server->RunConnection(socket);
    } catch (std::runtime_error &err) {
        std::cout << "Connection interrupt: " << err.what() << std::endl;
        close(socket);
    }
    std::cout << "\nDisconnecting\n";
    {
        std::lock_guard<std::mutex> lock(server->connections_mutex);
        server->connections.erase(pthread_self());
    }
    return nullptr;
}

// See Server.h
ServerImpl::ServerImpl(std::shared_ptr<Afina::Storage> ps) : Server(ps) {}

// See Server.h
ServerImpl::~ServerImpl() {}

// See Server.h
void ServerImpl::Start(uint32_t port, uint16_t n_workers) {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;

    // If a client closes a connection, this will generally produce a SIGPIPE
    // signal that will kill the process. We want to ignore this signal, so send()
    // just returns -1 when this happens.
    sigset_t sig_mask;
    sigemptyset(&sig_mask);
    sigaddset(&sig_mask, SIGPIPE);
    if (pthread_sigmask(SIG_BLOCK, &sig_mask, NULL) != 0) {
        throw std::runtime_error("Unable to mask SIGPIPE");
    }

    // Setup server parameters BEFORE thread created, that will guarantee
    // variable value visibility
    max_workers = n_workers;
    listen_port = port;

    // The pthread_create function creates a new thread.
    //
    // The first parameter is a pointer to a pthread_t variable, which we can use
    // in the remainder of the program to manage this thread.
    //
    // The second parameter is used to specify the attributes of this new thread
    // (e.g., its stack size). We can leave it NULL here.
    //
    // The third parameter is the function this thread will run. This function *must*
    // have the following prototype:
    //    void *f(void *args);
    //
    // Note how the function expects a single parameter of type void*. We are using it to
    // pass this pointer in order to proxy call to the class member function. The fourth
    // parameter to pthread_create is used to specify this parameter value.
    //
    // The thread we are creating here is the "server thread", which will be
    // responsible for listening on port 23300 for incoming connections. This thread,
    // in turn, will spawn threads to service each incoming connection, allowing
    // multiple clients to connect simultaneously.
    // Note that, in this particular example, creating a "server thread" is redundant,
    // since there will only be one server thread, and the program's main thread (the
    // one running main()) could fulfill this purpose.
    running.store(true);
    if (pthread_create(&accept_thread, NULL, ServerImpl::RunAcceptorProxy, this) < 0) {
        throw std::runtime_error("Could not create server thread");
    }
}

// See Server.h
void ServerImpl::Stop() {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    running.store(false);
}

// See Server.h
void ServerImpl::Join() {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    pthread_join(accept_thread, nullptr);
}

// See Server.h
void ServerImpl::RunAcceptor() {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;

    // For IPv4 we use struct sockaddr_in:
    // struct sockaddr_in {
    //     short int          sin_family;  // Address family, AF_INET
    //     unsigned short int sin_port;    // Port number
    //     struct in_addr     sin_addr;    // Internet address
    //     unsigned char      sin_zero[8]; // Same size as struct sockaddr
    // };
    //
    // Note we need to convert the port to network order

    struct sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;          // IPv4
    server_addr.sin_port = htons(listen_port); // TCP port number
    server_addr.sin_addr.s_addr = INADDR_ANY;  // Bind to any address

    // Arguments are:
    // - Family: IPv4
    // - Type: Full-duplex stream (reliable)
    // - Protocol: TCP
    int server_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == -1) {
        throw std::runtime_error("Failed to open socket");
    }

    // when the server closes the socket,the connection must stay in the TIME_WAIT state to
    // make sure the client received the acknowledgement that the connection has been terminated.
    // During this time, this port is unavailable to other processes, unless we specify this option
    //
    // This option let kernel knows that we are OK that multiple threads/processes are listen on the
    // same port. In a such case kernel will balance input traffic between all listeners (except those who
    // are closed already)
    int opts = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opts, sizeof(opts)) == -1) {
        close(server_socket);
        throw std::runtime_error("Socket setsockopt() failed");
    }

    // Bind the socket to the address. In other words let kernel know data for what address we'd
    // like to see in the socket
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        close(server_socket);
        throw std::runtime_error("Socket bind() failed");
    }

    // Start listening. The second parameter is the "backlog", or the maximum number of
    // connections that we'll allow to queue up. Note that listen() doesn't block until
    // incoming connections arrive. It just makesthe OS aware that this process is willing
    // to accept connections on this socket (which is bound to a specific IP and port)
    if (listen(server_socket, 5) == -1) {
        close(server_socket);
        throw std::runtime_error("Socket listen() failed");
    }

    int client_socket;
    struct sockaddr_in client_addr;
    socklen_t sinSize = sizeof(struct sockaddr_in);
    while (running.load()) {
        std::cout << "network debug: waiting for connection..." << std::endl;

        // When an incoming connection arrives, accept it. The call to accept() blocks until
        // the incoming connection arrives
        if ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &sinSize)) == -1) {
            close(server_socket);
            throw std::runtime_error("Socket accept() failed");
        }
        {
            std::lock_guard<std::mutex> lock(connections_mutex);
            if (connections.size() >= max_workers) {
                std::string msg = "Too many workers\n";
                if (send(client_socket, msg.data(), msg.size(), 0) <= 0) {
                    close(client_socket);
                    // Считаем, что юзер в таком случае сам пенек и успел отключиться.
                    std::cout << "Socket send() failed\n";

                    //close(server_socket);
                    //throw std::runtime_error("Socket send() failed");
                }
                else {
                    close(client_socket);
                    std::cout << "User has been disconnected due to workers overflow\n";
                }
            } else {
                pthread_t worker;
                auto args = std::make_pair(this, client_socket);
                if (pthread_create(&worker, NULL, ServerImpl::RunConnectionProxy, &args) != 0) {
                    throw std::runtime_error("Thread create() failed");
                }
                connections.insert(worker);
                std::cout << "\n" << "\nUser " << connections.size()-1 << " connected\n"<< '\n';
            }
        }
    }
    // Cleanup on exit...
    close(server_socket);
}

// See Server.h
void ServerImpl::RunConnection(int socket) {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    Protocol::Parser parser;
    char buffer[buf_size+1];
    std::string out;
    size_t parsed = 0;
    size_t curr_pos = 0;
    ssize_t n_read = 0;
    bool serv_disconnect = false;
    //try {
        if(!running.load()) return;

        std::cout << "Start of loop\n";
        while ((n_read = recv(socket, buffer + curr_pos, buf_size, 0)) > 0 || parsed < curr_pos) {
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
                    cmd->Execute(*pStorage, args, out);
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
            if (!running.load()) {
                serv_disconnect = true;
                break;
            }
        }

        // Сюда можно выйти по двум причинам - либо не выполняется условие(т.е. пользователь отсоединился), либо
        // если сервер затормозил.
        if (serv_disconnect) { // Те же команды, только recv удаляем
            while (parsed < curr_pos) {
                bool is_parsed = parser.Parse(buffer, curr_pos, parsed);
                if (is_parsed) {
                    size_t body_read = curr_pos - parsed;//body_read - сколько дочитали
                    memcpy(buffer, buffer + parsed, body_read);
                    memset(buffer + body_read, 0, parsed); // Убираем все, что было связано с командой.
                    curr_pos = body_read;

                    //Сбор команды и аргументов
                    uint32_t body_size;
                    auto cmd = parser.Build(body_size);
                    if (body_size > curr_pos){
                        break;
                    }
                    char args[body_size + 1];
                    memcpy(args, buffer, body_size);
                    args[body_size] = '\0';

                    // Запускаем
                    try {
                        cmd->Execute(*pStorage, args, out);
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
            }
            out = std::string("SERVER_DISCONNECTING") + "\r\n";
            if (send(socket, out.data(), out.size(), 0) <= 0) {
                throw std::runtime_error("Socket send() failed\n");
            }

        }else {
            throw std::runtime_error("User disconnected\n");
        }



}

} // namespace Blocking
} // namespace Network
} // namespace Afina
