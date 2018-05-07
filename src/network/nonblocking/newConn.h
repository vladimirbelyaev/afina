#ifndef AFINA_NEWCONN_H
#define AFINA_NEWCONN_H

#include <afina/Storage.h>
#include <protocol/Parser.h>
#include <afina/execute/Command.h>


namespace Afina {

// Forward declaration, see afina/Storage.h
    class Storage;

    namespace Network {
        namespace NonBlocking {
            class newConn {
                enum class State {
                    // Connection is fully operational, tasks could be added and get executed
                            kRun,

                    // Connection is on the way to be shutdown, no ned task could be added, but existing will be
                    // completed as requested
                            kStopping,

                    // Connection is stopped
                            kStopped
                };
            public:
                newConn();
                newConn(std::shared_ptr<Afina::Storage> ps, int sock);
                ~newConn();
                void routine();
            private:
                std::shared_ptr<Afina::Storage> pStorage;
                Protocol::Parser parser;
                State cState = State::kRun;
                int socket;
                bool is_parsed = false;
            };

        }
    }
}
#endif //AFINA_NEWCONN_H
