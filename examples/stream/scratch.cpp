#include <iostream>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <fstream>
#include <streambuf>

// так тоже можн поднять сервер
//g++ -o stream_server -std=c++11 stream_server.cpp -I./websocketpp -lboost_system -lpthread
//g++ -o stream_client -std=c++11 stream_client.cpp -I./websocketpp -lboost_system -lpthread
//./stream_server

//https://echo.websocket.org/.ws



#include <chrono>
#include <thread>
#include <iostream>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <fstream>
#include <streambuf>

#include <chrono>
#include <future>

#include <iostream>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <fstream>
#include <streambuf>

#include <chrono>
#include <thread>

#include <iostream>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <fstream>
#include <streambuf>

#include <chrono>
#include <thread>

#include <iostream>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <fstream>
#include <streambuf>

#include <chrono>
#include <thread>

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

    std::ifstream myfile("myfile2.txt");


    // Variable to store the previous content of the file
    static std::string previous_file_content;

    // Define a function to send changes to the server
    auto send_changes_to_server = [&echo_client, &previous_file_content](websocketpp::connection_hdl hdl) {
        std::ifstream myfile("myfile2.txt");
        if (myfile.is_open()) {
            // Read the content of the file
            std::string current_file_content((std::istreambuf_iterator<char>(myfile)), std::istreambuf_iterator<char>());
            myfile.close();

            // Find differences relative to the previous content
            std::size_t pos = previous_file_content.size();
            std::string changes = current_file_content.substr(pos);

            // Send changes to the server
            if (!changes.empty()) {
                echo_client.send(hdl, changes, websocketpp::frame::opcode::text);
                std::cout << "Sent changes to server." << std::endl;
            } else {
                std::cout << "No changes to send." << std::endl;
            }

            // Update the previous content
            previous_file_content = current_file_content;
        } else {
            std::cerr << "Error opening myfile2.txt" << std::endl;
        }
    };

    // Send a message to the server after successful connection
    echo_client.set_open_handler([&](websocketpp::connection_hdl hdl) {
        std::cout << "Connected to the server" << std::endl;

        // Send changes to the server
        send_changes_to_server(hdl);
    });

    websocketpp::connection_hdl hdl;
    // Create a thread to check the file and send changes to the server every 15 seconds
    std::thread file_check_thread([&]() {
        while (true) {
            // Wait for 15 seconds
            std::this_thread::sleep_for(std::chrono::seconds(15));

            // Check the file and send changes to the server
            auto connection = echo_client.get_con_from_hdl(hdl);
            if (connection) {
                auto connection_hdl = connection->get_handle();
                echo_client.get_io_service().post([send_changes_to_server, connection_hdl]() {
                    send_changes_to_server(connection_hdl);
                });
            } else {
                std::cerr << "Error getting handle from connection." << std::endl;
            }


            /*
            echo_client.get_io_service().post([hdl = echo_client.get_con_from_hdl(hdl)->get_handle()]() {
                send_changes_to_server(hdl);
            });
             */
        }
    });

    // Get a connection to the desired server
    websocketpp::lib::error_code ec;
    auto con = echo_client.get_connection("ws://localhost:9002", ec);

    if (ec) {
        std::cout << "Could not create connection because: " << ec.message() << std::endl;
        return 1;
    }

    // Run the client
    echo_client.connect(con);
    echo_client.run();

    // Start the thread to check the file
    file_check_thread.detach();

    return 0;
}


