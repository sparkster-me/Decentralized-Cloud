#define  WEBSCOKET_CLIENT_H
#include <boost/beast/core.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <cstdlib>
#include <iostream>
#include <string>
#include <chrono> 
#include <thread>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

class WSClient {
public:
	// Please change the IP Address to the Storage Node ip address
	std::string const host = "localhost";
	std::string const port = "1033";
	std::string const CONNECTION_REQUEST = "c:1|txid:1234567890|k:0123456789|tid:9876543210|tm:1944631469190";
	json send(std::string text) {
		try
		{
			// The io_context is required for all I/O
			net::io_context ioc;

			// These objects perform our I/O
			tcp::resolver resolver{ ioc };
			websocket::stream<tcp::socket> ws{ ioc };

			// Look up the domain name
			auto const results = resolver.resolve(boost::asio::ip::tcp::resolver::query{ host, port });

			// Make the connection on the IP address we get from a lookup
			net::connect(ws.next_layer(), results.begin(), results.end());

			// Perform the websocket handshake
			ws.handshake(host, "/");

			// Send the message
			ws.write(net::buffer(CONNECTION_REQUEST));

			//wait
			std::this_thread::sleep_for(std::chrono::milliseconds(100));

			// Send the message
			ws.write(boost::asio::buffer(text));

			// This buffer will hold the incoming message
			beast::multi_buffer buffer;

			// Read a message into our buffer
			ws.read(buffer);

			// If we get here then the connection is closed gracefully
			std::string res = boost::beast::buffers_to_string(buffer.data());

			std::cout << "Acknowledgement : " + res << std::endl;

			buffer.consume(buffer.size());
			
			//wait
			std::this_thread::sleep_for(std::chrono::milliseconds(200));

			// Read a message into our buffer
			ws.read(buffer);

			// Close the WebSocket connection
			ws.close(websocket::close_code::normal);

			// If we get here then the connection is closed gracefully
			res = boost::beast::buffers_to_string(buffer.data());

			// The buffers() function helps print a ConstBufferSequence
			std::cout << "Response : " + res << std::endl;
			json j;
			if (res.empty()) {
				return j;
			}
			else {
				j = j.parse( res);
				return j;
			}
		}
		catch (std::exception const& e)
		{
			std::cerr << "Error: " << e.what() << std::endl;
			return EXIT_FAILURE;
		}
		return EXIT_SUCCESS;
	};

	json get(std::string text) {
		try
		{
			// The io_context is required for all I/O
			net::io_context ioc;

			// These objects perform our I/O
			tcp::resolver resolver{ ioc };
			websocket::stream<tcp::socket> ws{ ioc };

			// Look up the domain name
			auto const results = resolver.resolve(boost::asio::ip::tcp::resolver::query{ host, port });

			// Make the connection on the IP address we get from a lookup
			net::connect(ws.next_layer(), results.begin(), results.end());

			// Perform the websocket handshake
			ws.handshake(host, "/");

			// Send the message
			ws.write(net::buffer(CONNECTION_REQUEST));

			//wait
			std::this_thread::sleep_for(std::chrono::milliseconds(100));

			// Send the message
			ws.write(boost::asio::buffer(text));

			// This buffer will hold the incoming message
			beast::multi_buffer buffer;

			// Read a message into our buffer
			ws.read(buffer);

			// Close the WebSocket connection
			ws.close(websocket::close_code::normal);

			// If we get here then the connection is closed gracefully
			std::string res = boost::beast::buffers_to_string(buffer.data());

			// The buffers() function helps print a ConstBufferSequence
			std::cout << "Response : " + res << std::endl;
			json j;
			if (res.empty()) {
				return j;
			}
			else {
				j = j.parse(res);
				return j;
			}
		}
		catch (std::exception const& e)
		{
			std::cerr << "Error: " << e.what() << std::endl;
			return EXIT_FAILURE;
		}
		return EXIT_SUCCESS;
	};
};
