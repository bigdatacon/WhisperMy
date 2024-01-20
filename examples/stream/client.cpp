#include <iostream>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

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

    // Register our message handler
    echo_client.set_message_handler(bind([&](websocketpp::connection_hdl hdl, client::message_ptr msg) {
        std::cout << "Received Message: " << msg->get_payload() << std::endl;
    }, _1, _2));

    // Get a connection to the desired server
    websocketpp::lib::error_code ec;
    client::connection_ptr con = echo_client.get_connection("ws://localhost:9002", ec);

    if (ec) {
        std::cout << "Could not create connection because: " << ec.message() << std::endl;
        return 1;
    }

    // Connect to the server
    echo_client.connect(con);

    // Run the client
    echo_client.run();

    return 0;
}
