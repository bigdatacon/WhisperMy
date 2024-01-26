#include <iostream>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <fstream>
#include <thread>
#include <atomic>


typedef websocketpp::client<websocketpp::config::asio_client> client;

void on_message(websocketpp::connection_hdl hdl, client::message_ptr msg) {
    std::cout << "Received Message: " << msg->get_payload() << std::endl;
}

bool is_connection_open(client& echo_client, websocketpp::connection_hdl hdl) {
    websocketpp::lib::error_code ec;
    auto con = echo_client.get_con_from_hdl(hdl, ec);
    return !ec && con->get_state() == websocketpp::session::state::open;
}

bool file_content_changed(const std::string& current_data, const std::string& previous_data) {
    return current_data != previous_data;
}



std::string previous_data;
std::atomic<bool> ready(false);

void send_data_thread(client& echo_client, websocketpp::connection_hdl hdl) {
    while (!ready) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    while (is_connection_open(echo_client, hdl)) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        std::ifstream myfile("myfile2.txt");
        std::string message((std::istreambuf_iterator<char>(myfile)), std::istreambuf_iterator<char>());
        myfile.close();

        std::cout << "msg: " << message << std::endl;

        if (file_content_changed(message, previous_data)) {
            std::string diff = message.substr(previous_data.length());
            std::cout << "Sending data DIFF: " << diff<< std::endl;
            echo_client.send(hdl, diff, websocketpp::frame::opcode::text);
            previous_data = message;
        }
        std::cout << "DID SEND DATA" << std::endl;
    }
}

int main() {
    using websocketpp::lib::placeholders::_1;
    using websocketpp::lib::placeholders::_2;
    using websocketpp::lib::bind;

    // Create a client endpoint
    client echo_client;

    // Set up a client
    echo_client.init_asio();

    // Register our message handler
    echo_client.set_message_handler(bind(&on_message, _1, _2));

    // Get a connection to the desired server
    std::cout << "TRY GET CONNECTION: " << std::endl;
    websocketpp::lib::error_code ec;
    client::connection_ptr con = echo_client.get_connection("ws://localhost:9002", ec);
    std::cout << " GET CONNECTION: " << std::endl;

    if (ec) {
        std::cout << "Could not create connection because: " << ec.message() << std::endl;
        return 1;
    }

    // Start the thread for sending data
    std::cout << "try send_thread: " << std::endl;
    std::thread send_thread(send_data_thread, std::ref(echo_client), con->get_handle());
    std::cout << "did send_thread: " << std::endl;

    // Connect to the server
    std::cout << "try connect " << std::endl;
    echo_client.connect(con);
    std::cout << "did connect " << std::endl;

    ready = true;  // Set the flag to indicate that the sending thread can start

    // Run the client
    std::cout << "try run " << std::endl;
    echo_client.run();
    std::cout << "did run " << std::endl;

    // Wait for the send thread to finish
    send_thread.join();
    std::cout << "ALL THREAD RUN: " << std::endl;

    return 0;
}
