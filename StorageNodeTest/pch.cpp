//
// pch.cpp
// Include the standard header and generate the precompiled header.
//

#include "pch.h"

std::string encode(std::string s) {
	return boost::beast::detail::base64_encode(s);
}

std::string random_generator() {
	boost::uuids::uuid uuid = boost::uuids::random_generator()();
	return boost::uuids::to_string(uuid);
}

WSClient* getWSClient() {
	wsClient = new WSClient();
	return wsClient;
}
