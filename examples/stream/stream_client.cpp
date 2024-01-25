#include <iostream>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <fstream>
#include <streambuf>
#include <filesystem>
#include <thread>

namespace ws = websocketpp;

// Create a client endpoint
typedef ws::client<ws::config::asio_client> client;

// Set up a client
client echo_client;

// Variable to store the previous content of the file
static std::string previous_file_content;

// Mutex for synchronizing access to common resources
std::mutex file_mutex;

// Define a function to send changes to the server
void send_changes_to_server(ws::connection_hdl hdl) {
    std::cout << "2 LOOP: " << std::endl;
    std::ifstream myfile("myfile2.txt");
    if (myfile.is_open()) {
        std::cout << "3 LOOP: " << std::endl;
        // Read the content of the file
        std::string current_file_content((std::istreambuf_iterator<char>(myfile)), std::istreambuf_iterator<char>());
        myfile.close();
        std::cout << "4 LOOP: " << std::endl;

        // Find differences relative to the previous content
        std::size_t pos = previous_file_content.size();
        std::string changes = current_file_content.substr(pos);
        std::cout << "5 LOOP: " << std::endl;

        // Use a mutex to synchronize access to common resources
        std::lock_guard<std::mutex> lock(file_mutex);

        // Send changes to the server
        if (!changes.empty()) {
            std::cout << "6 LOOP: " << std::endl;
            try {
                echo_client.send(hdl, changes, ws::frame::opcode::text);
                std::cout << "Sent changes to server." << std::endl;
            } catch (const websocketpp::exception& e) {
                std::cerr << "WebSocket++ Exception: " << e.what() << std::endl;
            }

        } else {
            std::cout << "No changes to send." << std::endl;
        }

        // Update the previous content
        std::cout << "7 LOOP: " << std::endl;
        previous_file_content = current_file_content;
    } else {
        std::cerr << "Error opening myfile2.txt" << std::endl;
    }
}

int main() {
    using ws::lib::placeholders::_1;
    using ws::lib::placeholders::_2;
    using ws::lib::bind;

    // Initialize the client
    echo_client.init_asio();
    std::cout << "START CLIENT" << std::endl;

    // Register our message handler
    echo_client.set_message_handler(bind([&](ws::connection_hdl hdl, client::message_ptr msg) {
        std::cout << "Received Message: " << msg->get_payload() << std::endl;
    }, _1, _2));

    // Send a message to the server after successful connection
    echo_client.set_open_handler([&](ws::connection_hdl hdl) {
        std::cout << "Connected to the server" << std::endl;

        // Start a thread to check for changes in the file and send them to the server
        std::thread file_check_thread([&]() {
            while (true) {
                // Wait for 3 seconds
                std::this_thread::sleep_for(std::chrono::seconds(3));
                std::cout << "1 LOOP: " << std::endl;

                // Check the file and send changes to the server
                echo_client.get_io_service().post([hdl]() {
                    send_changes_to_server(hdl);
                });
            }
        });

        // Join the thread after the client.run() completes
        echo_client.set_close_handler([&](ws::connection_hdl hdl) {
            file_check_thread.join();
        });

        // Start the thread to check the file
        file_check_thread.detach();
    });

    // Get a connection to the desired server
    ws::lib::error_code ec;
    auto con = echo_client.get_connection("ws://localhost:9002", ec);

    if (ec) {
        std::cout << "Could not create connection because: " << ec.message() << std::endl;
        return 1;
    }

    // Run the client
    echo_client.connect(con);
    echo_client.run();

    return 0;
}
