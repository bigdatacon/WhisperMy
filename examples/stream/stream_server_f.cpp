#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <iostream>
#include <chrono>
#include <thread>

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
    std::cout << "START SERV " << std::endl;

    // Установка обработчика сообщений
    echo_server.set_message_handler(bind(on_message, &echo_server, placeholders::_1, placeholders::_2));

    // Запуск сервера в отдельном потоке
    thread server_thread([&]() {
        // Запуск сервера
        echo_server.init_asio();
        echo_server.listen(9002);
        echo_server.start_accept();
        std::cout << "RUN SERV " << std::endl;
        echo_server.run();
    });

    // Ожидание завершения потока сервера
    server_thread.join();

    return 0;
}

