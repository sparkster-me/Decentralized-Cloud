// Inspired by example at https://raw.githubusercontent.com/zaphoyd/websocketpp/master/examples/associative_storage/associative_storage.cpp
#pragma once
#include <iostream>
#include <unordered_map>
#include <exception>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <websocketpp/frame.hpp>
#include <common/functions.h>

typedef websocketpp::server<websocketpp::config::asio> server;

using websocketpp::connection_hdl;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

struct ConnectionData {
	connection_hdl hdl;
};

class WebSocketServer {
public:
	typedef std::unordered_map<std::string, ConnectionData> ConnectionsMap;
	WebSocketServer();
	void onOpen(connection_hdl hdl);
	void onClose(connection_hdl hdl);
	void onMessage(connection_hdl hdl, server::message_ptr msg); // Will process an incoming command; will add a new connection to the connection pool, and send the payload to processIncomingData
	ConnectionData& getDataFromKey(const std::string& key);
	std::string getKeyFromHdl(const connection_hdl hdl) const;
	void run(uint16_t port);
	bool exists(const std::string& key) const;
	void addConnection(const std::string& key, const connection_hdl hdl);
	void send(const connection_hdl hdl, const std::string & msg);
	void send(const std::string& key, const std::string& msg);

private:
	void sendThreaded(const connection_hdl hdl, const std::string & msg);
	server m_server;
	ConnectionsMap m_connections;
};