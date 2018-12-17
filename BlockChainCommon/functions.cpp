#include <common/functions.h>
#include <fstream>
#include <opendht/base64.h>

const time_t appStartupTime = getEpochTimestamp();
time_t currentTimestamp = appStartupTime; // Represents the current epoch timestamp. When a node first starts, it will be set to appStartupTime.
const std::chrono::time_point<std::chrono::system_clock> appSystemStartupTime = std::chrono::system_clock::now();
CommandFunctionMap dataHandlers;
NodesMap masterNodes;
NodesMap verifierNodes;
NodesMap storageNodes;
std::vector<std::string> verifierKeys; // Used to select a random verifier.
std::vector<std::string> storageKeys; // Used to select a random storage node.
std::atomic<uint64_t> messageSequenceNumber; // Incremented by 1 every time we send a message over gossip so we can properly form a unique ID for the message.
std::set<std::string> processedMessages; // Used to store message IDs we have already seen in case we get a repeated message.
std::mutex processedMessagesMutex; // Used to lock the set into which already-processed IDs go.

FieldValueMap getFieldsAndValues(const std::string& inputString) {
	FieldValueMap hashMap;
	std::vector<std::string> fieldValuePairs;
	boost::split(fieldValuePairs, inputString, boost::is_any_of(FIELD_DELIMITER));
	size_t n = fieldValuePairs.size();
	std::vector<std::string> fieldAndValue;
	for (int i = 0; i < n; i++) {
		boost::split(fieldAndValue, fieldValuePairs[i], boost::is_any_of(PAIR_DELIMITER));
		// Decode the value back into its original form before we encoded it.
		boost::replace_all(fieldAndValue[1], "%3A", PAIR_DELIMITER);
		boost::replace_all(fieldAndValue[1], "%7C", FIELD_DELIMITER);
		hashMap.insert(FieldValueMap::value_type(fieldAndValue[0], fieldAndValue[1]));
		fieldAndValue.clear();
	}
	return FieldValueMap(hashMap);
}

uint32_t generateRandomNumber(uint32_t a, uint32_t b) {
	NumberDistribution distribution(a, b - 1);
	boost::random::random_device dev;
	uint32_t digit = distribution(dev);
	return digit;
}

std::string getGUID(size_t length) {
	std::vector<char> cGUID;
	for (size_t i = 0; i < length; i++)
		cGUID.push_back(static_cast<char>(generateRandomNumber('a', 'z' + 1)));
	cGUID.push_back('\0');
	return std::string(cGUID.data());
}

// Most of this code is from boost asio timeout example https://www.boost.org/doc/libs/1_52_0/doc/html/boost_asio/example/timeouts/blocking_udp_client.cpp
class client{
public:
	client(udp::socket& socket, boost::asio::io_service& io_service): socket_(socket),deadline_(io_service), io_service_(io_service) {
		// No deadline is required until the first socket operation is started. We
		// set the deadline to positive infinity so that the actor takes no action
		// until a specific deadline is set.
		deadline_.expires_at(boost::posix_time::pos_infin);

		// Start the persistent actor that checks for deadline expiry.
		check_deadline();
	}

	std::size_t receive(const boost::asio::mutable_buffer& buffer,boost::posix_time::time_duration timeout, boost::system::error_code& ec){
		// Set a deadline for the asynchronous operation.
		deadline_.expires_from_now(timeout);

		// Set up the variables that receive the result of the asynchronous
		// operation. The error code is set to would_block to signal that the
		// operation is incomplete. Asio guarantees that its asynchronous
		// operations will never fail with would_block, so any other value in
		// ec indicates completion.
		ec = boost::asio::error::would_block;
		std::size_t length = 0;

		// Start the asynchronous operation itself. The handle_receive function
		// used as a callback will update the ec and length variables.
		socket_.async_receive(boost::asio::buffer(buffer),
			boost::bind(&client::handle_receive, _1, _2, &ec, &length));

		// Block until the asynchronous operation has completed.
		do io_service_.run_one(); while (ec == boost::asio::error::would_block);

		return length;
	}

private:
	void check_deadline(){
		// Check whether the deadline has passed. We compare the deadline against
		// the current time since a new asynchronous operation may have moved the
		// deadline before this actor had a chance to run.
		if (deadline_.expires_at() <= deadline_timer::traits_type::now()){
			// The deadline has passed. The outstanding asynchronous operation needs
			// to be cancelled so that the blocked receive() function will return.
			//
			// Please note that cancel() has portability issues on some versions of
			// Microsoft Windows, and it may be necessary to use close() instead.
			// Consult the documentation for cancel() for further information.
			socket_.cancel();

			// There is no longer an active deadline. The expiry is set to positive
			// infinity so that the actor takes no action until a new deadline is set.
			deadline_.expires_at(boost::posix_time::pos_infin);
		}

		// Put the actor back to sleep.
		deadline_.async_wait(boost::bind(&client::check_deadline, this));
	}

	static void handle_receive(const boost::system::error_code& ec, std::size_t length,	boost::system::error_code* out_ec, std::size_t* out_length){
		*out_ec = ec;
		*out_length = length;
	}

private:
	udp::socket& socket_;
	deadline_timer deadline_;
	boost::asio::io_service& io_service_;
};
// Returns UTC epoch time
time_t getEpochTimestamp() {
	return std::time(0);
	// TODO: Planned to integrate with ntp server for time synchronization
	/*time_t timeRecv;
	boost::asio::io_service io_service;
	udp::resolver resolver(io_service);
	boost::array<unsigned char, 48> sendBuf = { 010,0,0,0,0,0,0,0,0 };
	boost::array<unsigned long, 1024> recvBuf;
	udp::endpoint sender_endpoint;
	while (true) {
		udp::resolver::query query(boost::asio::ip::udp::v4(), NTP_SERVER, "ntp");
		udp::endpoint receiver_endpoint = *resolver.resolve(query);
		udp::socket socket(io_service);
		socket.open(boost::asio::ip::udp::v4());
		socket.send_to(boost::asio::buffer(sendBuf), receiver_endpoint);
		client c(socket, io_service);
		boost::system::error_code ec;
		std::size_t n = c.receive(boost::asio::buffer(recvBuf),
			boost::posix_time::seconds(2), ec);		
		if (ec) {
			std::cerr << "NTP request timeout.Retrying... " << std::endl;
		}
		else {
			timeRecv = ntohl((time_t)recvBuf[4]);
			timeRecv -= 2208988800U;
			std::cout <<"Received timestamp from server "<< timeRecv << std::endl;
			break;
		}

	}

	return timeRecv;*/
}

SubscriptionAttributes* subscribeTo(dht::DhtRunner* dhtNode, const dht::InfoHash* hash) {
	// The listen method returns a token that we'll need to cancel this subscription.
	std::shared_future<size_t> tokenFuture = dhtNode->listen<dht::ImMessage>(*hash, [dhtNode](dht::ImMessage&& msg) {
		if (msg.from.toString() != dhtNode->getId().toString())
			return processIncomingData(dhtNode, msg.msg, msg.from.toString(), msg.date, false);
	});
	/*std::future_status status = tokenFuture.wait_for(std::chrono::seconds(60));
	if (status != std::future_status::ready)
		throw std::runtime_error("Could not attach to channel " + hash->toString() + ".");*/
	return new SubscriptionAttributes{ hash, tokenFuture };
}

SubscriptionAttributes* subscribeTo(dht::DhtRunner* dhtNode, const std::string& channelId) {
	return subscribeTo(dhtNode, &dht::InfoHash::get(channelId));
}

void unsubscribeFrom(dht::DhtRunner* node, SubscriptionAttributes* channelInfo) {
	node->cancelListen(*(channelInfo->channel), channelInfo->token);
	delete channelInfo;
}

void gossip(dht::DhtRunner* dhtNode, const std::string& channelId, const std::string& msg) {
	auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	std::string nonConstMsg(msg);
	// We use an ID based on the node's public key fingerprint instead of randomizing the ID parameter to imMessage's constructor to
	// prevent ID collisions.
	nonConstMsg += FIELD_DELIMITER + std::string("msgid") + PAIR_DELIMITER + dhtNode->getId().toString() + std::to_string(messageSequenceNumber++);
	dht::Value::Id id = 3;	
	dhtNode->putSigned(dht::InfoHash::get(channelId), dht::ImMessage(id, std::move(nonConstMsg), now), [msg](bool ok) {
		if (!ok)
			std::cerr << "Message \"" + msg + "\" publishing failed!" << std::endl;
	});
}

void gossip(dht::DhtRunner* dhtNode, const std::string& channelId, CommandType cmd, const FieldValueMap& kv) {
	std::string msg = std::string("c") + PAIR_DELIMITER + std::to_string(static_cast<uint8_t>(cmd));
	if (kv.size() > 0) {
		for (FieldValueMap::const_iterator it = kv.cbegin(); it != kv.cend(); it++) {
			std::string key(it->first);
			// Make sure we haven't accidentally used reserved keys to pass data to other nodes; used to catch field-naming problems.
			if (key == "msgid" || key == "k")
				throw std::runtime_error("The key " + key + " Can't be used because it is reserved.");
			std::string value(it->second);
			// encode the delimiters so we don't get parsing errors
			boost::replace_all(value, PAIR_DELIMITER, "%3A");
			boost::replace_all(value, FIELD_DELIMITER, "%7C");
			msg += FIELD_DELIMITER + std::string(it->first) + ":" + std::string(value);
		}
		// This value is inserted in processIncomingData, and is generated by OpenDHT or supplied by WebSocketServer.
		//msg += FIELD_DELIMITER + std::string("tm") + ":" + boost::lexical_cast<std::string>(getEpochTimestamp());
	}
	gossip(dhtNode, channelId, msg);
}

// Fetch data from the DHT
void dhtGet(dht::DhtRunner* dht, const std::string key, const std::function<void(std::string)>& func) {
	std::future<std::vector<std::string>> output = dht->get<std::string>(dht::InfoHash::get(key));
	// Fetching last element in the vector if not empty
	std::vector<std::string> result = output.get();
	func(std::string(!result.empty() ? result.back() : ""));
}

// Fetch data from the DHT
std::string dhtGet(dht::DhtRunner* dht, const std::string key) {
	std::future<std::vector<std::string>> output = dht->get<std::string>(dht::InfoHash::get(key));
	std::vector<std::string> result = output.get();
	return std::string(!result.empty() ? result.back() : "");
}

void dhtPut(dht::DhtRunner* dht, const std::string& key, const std::string& value) {
	dht->put(dht::InfoHash::get(key), dht::Value(value), [](bool ok) {},,true);
	
}

dht::crypto::Identity* initializeKeypair() {
	std::ifstream inFile("pk.key");
	if (!inFile.good()) {
		inFile.close();
		dht::crypto::Identity* ident = new dht::crypto::Identity(dht::crypto::generateIdentity());
		// We need to write out the private key and certificate so that we can load it on next boot of this node.
		// The key and certificate together make up the identity pair.
		std::ofstream outFile("pk.key", std::ios::out | std::ios::binary);
		dht::Blob key = ident->first->serialize();
		outFile.write((char*)&key[0], key.size());
		outFile.close();
		key.clear();
		// Next, we write the certificate.
		ident->second->pack(key);
		outFile = std::ofstream("cert.key", std::ios::out | std::ios::binary);
		outFile.write((char*)&key[0], key.size());
		outFile.close();
		return ident;
	} else {
		// We should build the Identity object manually.
		// The first field will contain the private key.
		// and the second field will contain the certificate.
		dht::Blob inKey;
		std::ifstream inFile("pk.key", std::ios::in);
		char c;
		while ((c = inFile.get()) != EOF) {
			inKey.push_back(c);
		}
		std::shared_ptr<dht::crypto::PrivateKey> privateKey(std::make_shared<dht::crypto::PrivateKey>(dht::crypto::PrivateKey(inKey)));
		inFile.close();
		inKey.clear();
		// Next, read in the certificate.
		inFile = std::ifstream("cert.key", std::ios::in);
		while ((c = inFile.get()) != EOF) {
			inKey.push_back(c);
		}
		std::shared_ptr<dht::crypto::Certificate> inCert(std::make_shared<dht::crypto::Certificate>(dht::crypto::Certificate(inKey)));
		// Next, set the private key to the first field and certificate to the second field so we have a valid identity pair.
		dht::crypto::Identity* ident = new dht::crypto::Identity(std::move(privateKey), std::move(inCert));
		return ident;
	}
}

void addNode(dht::DhtRunner* dhtNode, const std::string& key, const std::string& uri, BlockChainNode::NodeType type) {
	switch (type) {
	case BlockChainNode::NodeType::masterNode: {
		masterNodes.insert(NodesMap::value_type(key, new BlockChainNode(key, uri, type)));
		if (dhtNode) {
			gossip(dhtNode, key, CommandType::syncNodes, { {"d", std::string(buildSyncNodesInput())} });
		}
		break;
	}
	case BlockChainNode::NodeType::verifierNode: {
		verifierNodes.insert(NodesMap::value_type(key, new BlockChainNode(key, uri, type)));
		verifierKeys.push_back(std::string(key));
		break;
	}
	case BlockChainNode::NodeType::storageNode: {
		storageNodes.insert(NodesMap::value_type(key, new BlockChainNode(key, uri, type)));
		storageKeys.push_back(std::string(key));
	}
	}
}

std::string buildSyncNodesInput() {
	json mj;
	for (auto& x : masterNodes) {
		mj[x.first] = x.second->toJSONString();
	}

	json vj;
	for (auto& x : verifierNodes) {
		vj[x.first] = x.second->toJSONString();
	}

	json sj;
	for (auto& x : storageNodes) {
		sj[x.first] = x.second->toJSONString();
	}
	json j;
	j["m"] = mj.dump();
	j["v"] = vj.dump();
	j["s"] = sj.dump();

	return j.dump();
}

void registerDataHandlers(const CommandFunctionMap& handlers) {
	dataHandlers = CommandFunctionMap(handlers);
}

bool processIncomingData(dht::DhtRunner* dhtNode, const std::string& data, const std::string& key, time_t timestamp, bool fromWebSocket) {
	FieldValueMap theFields = getFieldsAndValues(data);
	theFields.insert(FieldValueMap::value_type("tm", std::to_string(timestamp)));

	if (appStartupTime > stoll(theFields.at("tm"))) {
		std::cout << "Dropping old message " << data << std::endl;
		return true;
	}
	if (!fromWebSocket) {
		std::lock_guard<std::mutex> lock(processedMessagesMutex);
		std::string& messageId = theFields.at("msgid");
		if (processedMessages.find(messageId) == processedMessages.end()) {
			processedMessages.insert(std::set<std::string>::value_type(messageId));
		} else {
			std::cerr << "Dropping duplicate message " << data << std::endl;
			return true;
		}
	}
	std::thread t([dhtNode, data, key, timestamp, fromWebSocket, theFields = std::move(theFields)]() mutable {
		try {
			// The ws field will be 1 if this payload is from a web socket, 0 otherwise.
			// This information could be useful in the data processors.
			theFields.insert(FieldValueMap::value_type("ws", (fromWebSocket) ? "1" : "0"));
			//std::cout << theFields.at("c");
			CommandType cmd = static_cast<CommandType>(atoi(theFields.at("c").c_str()));
			theFields.insert(FieldValueMap::value_type("k", key));
			if (cmd == CommandType::connectionRequest) {
				std::string nodeKey = theFields.at("k");
				std::cout << "nodeKey " << nodeKey;
				BlockChainNode::NodeType type = static_cast<BlockChainNode::NodeType>(atoi(theFields.at("t").c_str()));
				std::string uri = theFields.at("u");
				addNode(dhtNode, nodeKey, uri, type);
				return;
			}
			CommandFunctionMap::iterator it = dataHandlers.find(cmd);
			if (it != dataHandlers.end()) {
				it->second(std::move(theFields));
				return;
			}
		} catch (const std::exception e) {
			std::cerr << e.what() << std::endl;
		}
	});
	t.detach();
	return true;
}

size_t getNumberOfMasterNodes() {
	return masterNodes.size();
}

size_t getNumberOfStorageNodes() {
	return storageNodes.size();
}

std::string& getStorageNodeKey(const size_t& n) {
	return storageKeys[n];
}

std::string& getFirstStorageNodeKey() {
	return storageKeys[0];
}

std::string getStorageNodeURL(const std::string& key) {
	return storageNodes[key]->getURI();
}

size_t getNumberOfVerifiers() {
	return verifierNodes.size();
}

std::string& getVerifierNodeKey(const size_t& n) {
	return verifierKeys[n];
}

void print_usage() {
	std::cout << std::endl << std::endl;
	std::cout << "Usage: node.exe [--p port] [--ws port] [--b bootstrap_host[:port]] [--createaccount]" << std::endl << std::endl;
	std::cout << "--w: Set the port that this node's web socket server listens on. If this node has no web socket server, this option will be ignored." << std::endl;
	std::cout << "--p: Set the port that this node's OpenDHT instance listens on. This is the port that other nodes will use to connect to this node." << std::endl;
	std::cout << "--b: Set a bootstrap node. The port given here should be the port supplied with --p." << std::endl;
	std::cout << "--createaccount: Generate an account ID and associated private key. This command will show the account seed, account ID and private key. The private key and seed should be kept in a secure location and can be used to recover the account ID." << std::endl;
	exit(0);
}


void parseArgs(int argc, char **argv, command_params* params, boost::optional<opt::options_description&> additionalOptions) {
	opt::options_description optionsList("Allowed command options");
	optionsList.add_options()
		("help,h", "Help screen")
		("p", opt::value<int>(&params->port)->default_value(DHT_DEFAULT_PORT), "Port")
		("w", opt::value<int>(&params->wsPort)->default_value(WS_DEFAULT_PORT), "Port")
		("b", opt::value<std::string>()->notifier([&params](std::string bootstrap) {
		params->bootstrap = dht::splitPort(boost::any_cast<std::string>(bootstrap));
				if (not params->bootstrap.first.empty() and params->bootstrap.second.empty()) {
					params->bootstrap.second = std::to_string(DHT_DEFAULT_PORT);
				}
				}), "bootstrap node")
		("createaccount", "Creates account");
		
	if (additionalOptions.is_initialized())
		optionsList.add(additionalOptions.get());

	opt::variables_map vm;
	opt::store(opt::parse_command_line(argc, argv, optionsList), vm);
	opt::notify(vm);
	if (vm.count("help") || vm.count("h"))
		// call helper function which prints the allowed command options details
		print_usage();
	if (vm.count("createaccount"))
		createAccount();
}

std::string createEd25519Signature(std::string& message, std::string& base64publicKey, std::string& base64privateKey) {
	const size_t sigLen = 64;
	std::vector<unsigned char> messageVec;
	messageVec.assign(message.c_str(), message.c_str() + message.length());
	std::string publicKeyStr= base64_decode(base64publicKey);
	std::vector<unsigned char> publicKeyVec;
	publicKeyVec.assign(publicKeyStr.c_str(), publicKeyStr.c_str() + publicKeyStr.length());
	std::string privateKeyStr = base64_decode(base64privateKey);
	std::vector<unsigned char> privateKeyVec;
	privateKeyVec.assign(privateKeyStr.c_str(), privateKeyStr.c_str() + privateKeyStr.length());
	unsigned char* signature = new unsigned char[sigLen];
	/* create signature on the message with the keypair */
	ed25519_sign(signature, messageVec.data(), message.length(), publicKeyVec.data(), privateKeyVec.data());
	std::string base64signature = base64_encode(std::vector<unsigned char>(signature, signature + sigLen));
	return std::string(base64signature);
}

std::string createX509Signature(const std::string& message, const dht::crypto::Identity* keypair) {
	dht::Blob blobMessage;
	blobMessage.assign(message.c_str(), message.c_str() + message.length());
	dht::Blob blobSignature = keypair->first->sign(blobMessage);
	return std::string(base64_encode(blobSignature));
}

bool verifyX509Signature(const std::string& message, const std::string& base64signature, dht::crypto::PublicKey& publicKey) {
	dht::Blob data;
	data.assign(message.c_str(), message.c_str() + message.length());
	std::string signature(base64_decode(base64signature));
	dht::Blob blobSignature;
	blobSignature.assign(signature.c_str(), signature.c_str() + signature.length());
	return publicKey.checkSignature(data, blobSignature);
}

bool verifyEd25519Signature(const std::string& message, const std::string& base64key, const std::string& base64signature) {
	std::vector<unsigned char> msgVec;
	msgVec.assign(message.c_str(), message.c_str() + message.length() + 1);
	std::string decodedKey(base64_decode(base64key));
	std::vector<unsigned char> decodedKeyVec;
	decodedKeyVec.assign(decodedKey.c_str(), decodedKey.c_str() + decodedKey.length());
	std::cerr << decodedKey.size() << std::endl;
	std::string decodedSignature(base64_decode(base64signature));
	std::vector<unsigned char> decodedSignatureVec;
	decodedSignatureVec.assign(decodedSignature.c_str(), decodedSignature.c_str() + decodedSignature.length());
	std::cerr << decodedSignature.size() << std::endl;
	size_t msgLength = message.length();
	return ed25519_verify(decodedSignatureVec.data(), msgVec.data(), msgLength, decodedKeyVec.data());
}

time_t updateCurrentTimestamp() {
	std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
	std::chrono::seconds secondsSinceStartup = std::chrono::duration_cast<std::chrono::seconds>(now - appSystemStartupTime);
	return (currentTimestamp = appStartupTime + secondsSinceStartup.count());
}

time_t getCurrentTimestamp() {
	return currentTimestamp;
}

time_t getSecondsPastHour() {
	return currentTimestamp % 3600;
}

time_t getSecondsPastHour(time_t timestamp) {
	return timestamp % 3600;
}

// Fetches Public IP Address of a system
std::string getPublicIP() {
	try {

		int version = 11;

		// The io_context is required for all I/O
		net::io_context ioc;

		// These objects perform our I/O
		tcp::resolver resolver{ ioc };
		tcp::socket socket{ ioc };

		// Look up the domain name
		auto const results = resolver.resolve(std::string(URL_PUBLIC_IP), std::string(URL_PUBLIC_PORT));

		// Make the connection on the IP address we get from a lookup
		net::connect(socket, results.begin(), results.end());

		// Set up an HTTP GET request message
		http::request<http::string_body> req{ http::verb::get, std::string(URL_PUBLIC_PATH), 11 };
		req.set(http::field::host, std::string(URL_PUBLIC_IP));
		req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

		// Send the HTTP request to the remote host
		http::write(socket, req);

		// This buffer is used for reading and must be persisted
		beast::flat_buffer buffer;

		// Declare a container to hold the response
		http::response<http::string_body> res;

		// Receive the HTTP response
		http::read(socket, buffer, res);

		// Gracefully close the socket
		beast::error_code ec;
		socket.shutdown(tcp::socket::shutdown_both, ec);
		// not_connected happens sometimes
			// so don't bother reporting it.
			//
		if (ec && ec != beast::errc::not_connected)
			throw beast::system_error{ ec };

		if (res.result_int() == 200)
			// Write the message to standard out
			return std::string(res.body());
	} catch (std::exception const& e) {
		std::cerr << "Error: " << e.what() << std::endl;
	}
	return "";
}

json generateAccount() {
	json j;
	uint8_t seed[32];
	uint8_t privateKey[32];
	uint8_t publicKey [32];
	if (ed25519_create_seed(seed) > 0)
		throw std::exception("Seed generation failed!");
	ed25519_create_keypair(publicKey, privateKey, seed);
	j["PrivateKey"] = base64_encode(std::vector<uint8_t>(privateKey, privateKey + 32));
	j["PublicKey"] = base64_encode(std::vector<uint8_t>(publicKey, publicKey + 32));
	j["Seed"] = base64_encode(std::vector<uint8_t>(seed, seed + 32));
	return json(j);
}

NodesMap* getMasterNodes() {
	return &masterNodes;
}

NodesMap* getVerifierNodes() {
	return &verifierNodes;
}

NodesMap* getStorageNodes() {
	return &storageNodes;
}

std::vector<std::string> getVerifierKeys() {
	return verifierKeys;
}

std::vector<std::string> getStorageKeys() {
	return storageKeys;
}

void addVerifierKey(const std::string& key) {
	verifierKeys.push_back(key);
}

void addStorageKey(const std::string& key) {
	storageKeys.push_back(key);
}

void createAccount() {
	std::cout << "Please find your account information below. You should store your private key and seed in a secure location; anyone who gains access to this information will be able to impersonate your account." << std::endl;
	json j(generateAccount());
	std::cout << j.dump() << std::endl;
	exit(0);
}