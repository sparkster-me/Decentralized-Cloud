#include <common/webSocketServer.h>
// Inspired by example at https://raw.githubusercontent.com/zaphoyd/websocketpp/master/examples/associative_storage/associative_storage.cpp

WebSocketServer::WebSocketServer() {
	m_server.init_asio();
	m_server.set_open_handler(bind(&WebSocketServer::onOpen, this, ::_1));
	m_server.set_close_handler(bind(&WebSocketServer::onClose, this, ::_1));
	m_server.set_message_handler(bind(&WebSocketServer::onMessage, this, ::_1, ::_2));
	m_server.clear_access_channels(websocketpp::log::alevel::all);
}

void WebSocketServer::onOpen(connection_hdl hdl) {
}

void WebSocketServer::onClose(connection_hdl hdl) {
	std::string key = getKeyFromHdl(hdl);
	std::cout << "Closing connection " << key << std::endl;
	m_connections.erase(key);	
}

void WebSocketServer::onMessage(connection_hdl hdl, server::message_ptr msg) {
	try {
		std::string nodeKey;
		std::string data = msg->get_payload();
		FieldValueMap theFields = getFieldsAndValues(data);
		CommandType cmd = static_cast<CommandType>(atoi(theFields["c"].c_str()));
		if (cmd == CommandType::connectionRequest) {
			nodeKey = theFields.at("k");			
			if (!exists(nodeKey)) {
				addConnection(nodeKey, hdl);
			}
			return;
		} else {
			nodeKey = getKeyFromHdl(hdl);
			if (nodeKey.empty())
				throw std::runtime_error("Socket connection doesn't have a key. Could be due to no connection request being made before sending a different command. Client should send CommandType::connectionRequest first.");
		}
		processIncomingData(nullptr, data, nodeKey, getCurrentTimestamp(), true);
	} catch (const std::exception e) {
		std::cerr << e.what() << std::endl;
	}
}

ConnectionData& WebSocketServer::getDataFromKey(const std::string& key) {
	auto it = m_connections.find(key);
	if (it == m_connections.end()) {
		// this connection is not in the list. This really shouldn't happen
		// and probably means something else is wrong.
		throw std::invalid_argument("No data available for session...");
	}
	return it->second;
}

std::string WebSocketServer::getKeyFromHdl(const connection_hdl hdl) const {
	for (ConnectionsMap::const_iterator it = m_connections.cbegin(); it != m_connections.cend(); it++) {
		if (it->second.hdl.lock() == hdl.lock()) {
			return it->first;
		}
	}
	return std::string("");
}

bool WebSocketServer::exists(const std::string& key) const  {
	return m_connections.find(key) != m_connections.end();
}

void WebSocketServer::addConnection(const std::string& key, const connection_hdl hdl) {
	m_connections.insert(ConnectionsMap::value_type(key, {
		hdl // hdl field
																																																	}));
}

void WebSocketServer::run(uint16_t port) {
	m_server.listen(port);
	m_server.start_accept();
	m_server.run();
}

void WebSocketServer::send(const connection_hdl hdl, const std::string & msg) {
	WebSocketServer::sendThreaded(hdl, msg);
}

void WebSocketServer::sendThreaded(const connection_hdl hdl, const std::string & msg) {
	m_server.send(hdl, msg, websocketpp::frame::opcode::text);
}

void WebSocketServer::send(const std::string& key, const std::string& msg) {
	try {
		ConnectionData data = getDataFromKey(key);
		send(data.hdl, msg);
	}
	catch (std::exception e) {
		std::cerr << e.what() << std::endl; 
	}
}