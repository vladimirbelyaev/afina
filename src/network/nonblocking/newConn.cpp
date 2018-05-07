//
// Created by vladimir on 05.05.18.
//

#include <iostream>
#include <sys/socket.h>
#include <cstring>
#include "newConn.h"
#include <unistd.h>


namespace Afina {

// Forward declaration, see afina/Storage.h

    namespace Network {
        namespace NonBlocking {
            newConn::newConn(std::shared_ptr<Afina::Storage> ps, int sock): pStorage(ps), socket(sock) {
                parser = Protocol::Parser();
                is_parsed = false;
                cState = State::kRun;
                std::cout << "Init pStorage for connection at ptr " << ps.get() << std::endl;
            }
            newConn::newConn() {
                std::cout << "Used default constructor\n";
            }
            newConn::~newConn(){}

            void newConn::routine() {
                std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
                auto buf_size = 1024;
                char buffer[buf_size];
                std::string out;
                size_t parsed = 0;
                size_t curr_pos = 0;
                ssize_t n_read = 0;
                uint32_t body_size = 0;
                //std::cout << "Trying to receive\n";


                // Делать 2 recv бессмысленно, мы ж nonblocking. Раз прочитали, запомнили состояние.
                n_read = recv(socket, buffer + curr_pos, buf_size - curr_pos, 0);
                if (n_read == 0){
                    close(socket);
                    throw std::runtime_error("User respectively disconnected");
                }
                if (n_read == -1){
                    if (errno == EAGAIN || errno == EWOULDBLOCK){ // In data ended.
                        return;
                    }else{
                        close(socket);
                        throw std::runtime_error("User irrespectively disconnected");
                    }
                }
                //std::cout << "Descriptor " << socket << " n_read is " << n_read << std::endl;
                curr_pos += n_read;


                while (parsed < curr_pos) {
                    is_parsed = parser.Parse(buffer, curr_pos, parsed);
                    if (is_parsed) {
                        size_t body_read = curr_pos - parsed;//body_read - сколько дочитали
                        memcpy(buffer, buffer + parsed, body_read);
                        memset(buffer + body_read, 0, parsed); // Убираем все, что было связано с командой.
                        curr_pos = body_read;

                        //Сбор команды и аргументов
                        auto cmd = parser.Build(body_size);

                        // Проверка на возможность дочитать команду. Если дочитали - то собрать и отправить результат.
                        if (body_size <= curr_pos) {
                            char args[body_size + 1];
                            memcpy(args, buffer, body_size);
                            args[body_size] = '\0';
                            if (body_size) {
                                memcpy(buffer, buffer + body_size + 2, curr_pos - body_size - 2);
                                memset(buffer + curr_pos - body_size - 2, 0, body_size);
                                curr_pos -= body_size + 2;
                            }
                            try {
                                cmd->Execute(*(pStorage.get()), args, out); // Должно передаваться без копирования
                                out += "\r\n";
                            } catch (std::runtime_error &err) { // Ошибка внутри поймается и отправится клиенту
                                out = std::string("SERVER_ERROR : ") + err.what() + "\r\n";
                            }
                            if (send(socket, out.data(), out.size(), 0) <= 0) {
                                throw std::runtime_error("Socket send() failed\n");
                            }
                            parser.Reset();
                            is_parsed = false;
                        }
                    }
                }

            }
        }//Nonblocking
    }//Network
}//Afina