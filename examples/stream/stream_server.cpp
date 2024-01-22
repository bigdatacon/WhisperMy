#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <iostream>
#include <chrono>
#include <thread>



//sudo apt-get install libboost-all-dev
//g++ -o server -std=c++11 server.cpp -lwebsocketpp -lboost_system -lpthread
//g++ -o client -std=c++11 client.cpp -lwebsocketpp -lboost_system -lpthread

//g++ -o stream_server stream_server.cpp -I/usr/local/include
//g++ -o stream_client stream_server.cpp -I/usr/local/include

using namespace std;

typedef websocketpp::server<websocketpp::config::asio> server;

void on_message(server* s, websocketpp::connection_hdl hdl, server::message_ptr msg) {
    cout << "Received Message: " << msg->get_payload() << endl;

    // Отправка ответа обратно клиенту
    try {
        s->send(hdl, msg->get_payload(), msg->get_opcode());
    } catch (const websocketpp::lib::error_code& e) {
        cout << "Echo failed because: " << e << "(" << e.message() << ")" << endl;
    }
}

int main() {
    server echo_server;

    // Установка обработчика сообщений
    echo_server.set_message_handler(std::bind(on_message, &echo_server, std::placeholders::_1, std::placeholders::_2));

    std::cout << "START RUNNING " << std::endl;
    // Запуск сервера
    echo_server.init_asio();
    echo_server.listen(9002);
    echo_server.start_accept();
    echo_server.run();
    std::cout << "SERVER IS RUNNING " << std::endl;

    return 0;
}
