#include <iostream>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

// так тоже можн поднять сервер
//g++ -o stream_server -std=c++11 stream_server.cpp -I./websocketpp -lboost_system -lpthread
//g++ -o stream_client -std=c++11 stream_client.cpp -I./websocketpp -lboost_system -lpthread
//./stream_client
//./stream_server

//https://echo.websocket.org/.ws

int main() {
    using websocketpp::lib::placeholders::_1;
    using websocketpp::lib::placeholders::_2;
    using websocketpp::lib::bind;

    // Create a client endpoint
    typedef websocketpp::client<websocketpp::config::asio_client> client;

    // Set up a client
    client echo_client;

    // Initialize the client
    echo_client.init_asio();
    std::cout << "START CLIENT" << std::endl;

    // Register our message handler
    echo_client.set_message_handler(bind([&](websocketpp::connection_hdl hdl, client::message_ptr msg) {
        std::cout << "Received Message: " << msg->get_payload() << std::endl;
    }, _1, _2));

    // Отправить сообщение на сервер после успешного подключения
    echo_client.set_open_handler([&](websocketpp::connection_hdl hdl) {
        std::string message = "Hello, server 444444444!";
        echo_client.send(hdl, message, websocketpp::frame::opcode::text);
    });

    // Get a connection to the desired server
    websocketpp::lib::error_code ec;
    client::connection_ptr con = echo_client.get_connection("ws://localhost:9002", ec);

    if (ec) {
        std::cout << "Could not create connection because: " << ec.message() << std::endl;
        return 1;
    }

    // Создаем поток для чтения ввода с клавиатуры и отправки на сервер
    std::thread input_thread([&]() {
        std::string input;
        while (true) {
            std::getline(std::cin, input);

            // Отправляем введенный текст на сервер
            echo_client.send(con->get_handle(), input, websocketpp::frame::opcode::text);
        }
    });


    // Connect to the server
    echo_client.connect(con);

    // Run the client
    echo_client.run();




    return 0;
}
