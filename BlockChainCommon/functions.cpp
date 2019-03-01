#include <common/functions.h>
#include <fstream>
#include <opendht/base64.h>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/algorithm/hex.hpp>
const time_t appStartupTime = getEpochTimestamp();
time_t currentTimestamp = appStartupTime; // Represents the current epoch timestamp. When a node first starts, it will be set to appStartupTime.
const time_t appSystemStartupTime = time(0);
CommandFunctionMap dataHandlers;
NodesMap masterNodes;
NodesMap verifierNodes;
NodesMap storageNodes;
std::vector<std::string> verifierKeys; // Used to select a random verifier.
std::vector<std::string> storageKeys; // Used to select a random storage node.
std::set<std::string> processedMessages; // Used to store message IDs we have already seen in case we get a repeated message.
std::mutex processedMessagesMutex; // Used to lock the set into which already-processed IDs go.
std::mutex nodesSyncMutex;
std::set<std::string> subscribedChannels; // When channel-subscription is handled by us instead of the communication API, we'll use this set to store the channel names to which the node is subscribed.
dht::DhtRunner dhtNode;

FieldValueMap getFieldsAndValues(const std::string& inputString) {
	FieldValueMap hashMap;
	std::vector<std::string> fieldValuePairs;
	boost::split(fieldValuePairs, inputString, boost::is_any_of(FIELD_DELIMITER));
	size_t n = fieldValuePairs.size();
	std::vector<std::string> fieldAndValue;
	for (int i = 0; i < n; i++) {
		boost::split(fieldAndValue, fieldValuePairs[i], boost::is_any_of(PAIR_DELIMITER));
		if (fieldAndValue.size() < 2)
			throw std::out_of_range("The data is corrupt. No field-value pair could be extracted.");
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
	std::string cGUID;
	for (size_t i = 0; i < length; i++)
		cGUID.push_back(static_cast<char>(generateRandomNumber('a', 'z' + 1)));
	return std::string(cGUID);
}

// Most of this code is from boost asio timeout example https://www.boost.org/doc/libs/1_52_0/doc/html/boost_asio/example/timeouts/blocking_udp_client.cpp
class client {
public:
	client(udp::socket& socket, boost::asio::io_service& io_service) : socket_(socket), deadline_(io_service), io_service_(io_service) {
		// No deadline is required until the first socket operation is started. We
		// set the deadline to positive infinity so that the actor takes no action
		// until a specific deadline is set.
		deadline_.expires_at(boost::posix_time::pos_infin);

		// Start the persistent actor that checks for deadline expiry.
		check_deadline();
	}

	std::size_t receive(const boost::asio::mutable_buffer& buffer, boost::posix_time::time_duration timeout, boost::system::error_code& ec) {
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
	void check_deadline() {
		// Check whether the deadline has passed. We compare the deadline against
		// the current time since a new asynchronous operation may have moved the
		// deadline before this actor had a chance to run.
		if (deadline_.expires_at() <= deadline_timer::traits_type::now()) {
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

	static void handle_receive(const boost::system::error_code& ec, std::size_t length, boost::system::error_code* out_ec, std::size_t* out_length) {
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

SUB_TYPE subscribeTo(dht::InfoHash&& hash) {
#if COMM_PROTOCOL == PROTOCOL_SMUDGE
	return subscribeTo(hash.toString());
#elif COMM_PROTOCOL == PROTOCOL_OPENDHT
	// The listen method returns a token that we'll need to cancel this subscription.
	std::shared_future<size_t> tokenFuture = dhtNode.listen<dht::ImMessage>(hash, [](dht::ImMessage&& msg) {
		// We use the timestamp and key already embedded inside the message by the original sender.
		dataHandler(msg.msg, msg.from.toString());
		return true;
	});
	/*std::future_status status = tokenFuture.wait_for(std::chrono::seconds(60));
	if (status != std::future_status::ready)
		throw std::runtime_error("Could not attach to channel " + hash->toString() + ".");*/
	return SubscriptionAttributes{ std::move(hash), std::move(tokenFuture) };
#endif
}

SUB_TYPE subscribeTo(const std::string& channelId) {
#if COMM_PROTOCOL == PROTOCOL_SMUDGE
	subscribedChannels.insert(channelId);
	return channelId;
#elif COMM_PROTOCOL == PROTOCOL_OPENDHT
	return subscribeTo(dht::InfoHash::get(channelId));
#endif
}

void unsubscribeFrom(const SUB_TYPE& channelInfo) {
#if COMM_PROTOCOL == PROTOCOL_SMUDGE
	subscribedChannels.erase(channelInfo);
#elif COMM_PROTOCOL == PROTOCOL_OPENDHT
	dhtNode.cancelListen(channelInfo.channel, channelInfo.token);
#endif
}

void dataHandler(std::string msg, const std::string originator) {
	try {
		if (msg.empty())
			return;
		// Check if this is a notification packet. This will happen if we send a large packet and don't feel like breaking it up into many smaller pieces.
		// In this case, the sender has put the packet in the DHT and we will receive the nonce of the packet to fetch.
		if (msg.compare(0, 1, "g") == 0) {
			std::string nonce = msg.substr(1, std::string::npos);
			logDebug("Fetching message from DHT using the key " + nonce);
			dhtGet(nonce, [nonce](std::string data) {
				if (data.empty()) {
					logError("The message retrieved with the key " + nonce + "was empty.");
					return;
				}
				data += "|nonce:" + nonce;
				logDebug("Successfully fetched data from DHT using the key " + nonce);
				processIncomingData(std::move(data), "", 0, false);
			});
			return;
		}
	} catch (const std::exception& e) {
		logError("There was an error while processing a message. error: %s", msg.c_str());
	} catch (...) {
		logError("An unknown error occurred in function dataHandler.");
	}
}

#if COMM_PROTOCOL == PROTOCOL_SMUDGE
void smudgeDataHandler(const char* payload) {
	std::string msg(payload);
	removeNullTerminators(msg);
	dataHandler(msg);
}
#endif

void gossip(std::string channelId, std::string msg, bool isMayday) {
	time_t now = getCurrentTimestamp();
	// Id is the packet ID. Other nodes will use it to request a Mayday.
	dht::Value::Id id = rand_id(rd);
	// If this is a mayday request / response, we shouldn't modify this packet at all because it's already been constructed.
	if (!isMayday) {
		msg += FIELD_DELIMITER + std::string("msgid") + PAIR_DELIMITER + getGUID(64);
#if COMM_PROTOCOL == PROTOCOL_SMUDGE
		// We need to manually add the channel ID for this message.
		// OpenDHT adds this automatically.
		msg += FIELD_DELIMITER + std::string("chanid") + PAIR_DELIMITER + channelId;
#endif
		// Here, although the timestamp is provided by OpenDHT, we use the timestamp embedded in the message in case we need to request a Mayday to get the rest of the message, in which case the timestamp will be the time when the last fragment arrived, which is incorrect.
		// This is also the case with the ID of the sending node: if we rely on OpenDHT, the ID will be the node that responded to the mayday request.
		msg += FIELD_DELIMITER + std::string("k") + PAIR_DELIMITER + dhtNode.getId().toString();
		msg += FIELD_DELIMITER + std::string("tm") + PAIR_DELIMITER + std::to_string(now);
	}
	removeNullTerminators(msg);
	std::string nonce = std::to_string(id);
	std::string packet = "g" + nonce;
	dhtPut(nonce, msg, false);
	logDebug("Sending messaged nonce " + nonce + " to the node " + channelId);
#if COMM_PROTOCOL == PROTOCOL_SMUDGE
	GoString p = toGoString(packet);
	BroadcastString(p);
	delete p.p;
#elif COMM_PROTOCOL == PROTOCOL_OPENDHT
	sendMessageWithRetry(id, channelId, std::move(packet), now, 0);
#endif
}

void sendMessageWithRetry(const dht::Value::Id& id, const std::string& channelId, std::string&& msg, const time_t& timestamp, const int& tries) {
	// We don't know if msg will be alive by the time we copy it into the lambda or if it'll be moved
	// into the function, because of unspecified order of evaluation of function parameters.
	// So we'll create a copy to be safe.
	std::string msgCopy(msg);
	dhtNode.putSigned(dht::InfoHash::get(channelId), dht::ImMessage(id, std::move(msg), timestamp), [msgCopy, timestamp, id, tries, channelId](bool ok) mutable {
		if (!ok) {
			if (tries < 2) {
				logWarning("Retrying message, retry number " + std::to_string(tries + 1));
				std::this_thread::sleep_for(std::chrono::milliseconds(500 * (tries + 1)));
				sendMessageWithRetry(id, channelId, std::move(msgCopy), timestamp, tries + 1);
			} else
				logError("A message failed to publish.");
		}
	});
}

void gossip(const std::string& channelId, CommandType cmd, const FieldValueMap& kv) {
	std::string msg = std::string("c") + PAIR_DELIMITER + std::to_string(static_cast<uint8_t>(cmd));
	if (kv.size() > 0) {
		for (FieldValueMap::const_iterator it = kv.cbegin(); it != kv.cend(); it++) {
			std::string key(it->first);
			// Make sure we haven't accidentally used reserved keys to pass data to other nodes; used to catch field-naming problems.
			if (key == "msgid" || key == "k" || key == "c" || key == "chanid")
				throw std::runtime_error("The key " + key + " Can't be used because it is reserved.");
			std::string value(it->second);
			// encode the delimiters so we don't get parsing errors
			boost::replace_all(value, PAIR_DELIMITER, "%3A");
			boost::replace_all(value, FIELD_DELIMITER, "%7C");
			msg += FIELD_DELIMITER + std::string(it->first) + ":" + value;
		}
		// This value is inserted in processIncomingData, and is generated by OpenDHT or supplied by WebSocketServer.
		//msg += FIELD_DELIMITER + std::string("tm") + ":" + boost::lexical_cast<std::string>(getEpochTimestamp());
	}
	gossip(channelId, msg);
}

// Fetch data from the DHT
// Fetch data from the DHT
void dhtGet(const std::string& key, const std::function<void(std::string)>& func) {
	dhtNode.get<std::string>(dht::InfoHash::get(key), [func](std::vector<std::string> result) {
		func(std::string(!result.empty() ? result.back() : ""));
		return false;
	});
}

// Fetch data from the DHT
std::string dhtGet(const std::string& key) {
	std::future<std::vector<std::string>> output = dhtNode.get<std::string>(dht::InfoHash::get(key));
	std::vector<std::string> result = output.get();
	return std::string(!result.empty() ? result.back() : "");
}

void dhtPut(const std::string& key, const std::string& value, bool permanent) {
	dht::Value dht_value(value);
	dht_value.id = rand_id(rd);
	dht_value.type = permanent?8:6;	
	dhtNode.put(dht::InfoHash::get(key), std::move(dht_value), [key](bool success) {
		/*if(!success)
		logError("DHT put failed for the key: %s",key)*/
	}, dht::time_point::max(), permanent);
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

void addNode(const std::string& key, const std::string& uri, BlockChainNode::NodeType type) {
	{
		std::lock_guard<std::mutex> lock(nodesSyncMutex);
		switch (type) {
		case BlockChainNode::NodeType::masterNode: {
			masterNodes.insert(NodesMap::value_type(key, BlockChainNode(key, uri, type)));
			break;
		}
		case BlockChainNode::NodeType::verifierNode: {
			verifierNodes.insert(NodesMap::value_type(key, BlockChainNode(key, uri, type)));
			verifierKeys.push_back(std::string(key));
			break;
		}
		case BlockChainNode::NodeType::storageNode: {
			storageNodes.insert(NodesMap::value_type(key, BlockChainNode(key, uri, type)));
			storageKeys.push_back(std::string(key));
		}
		}
	}
	// We must do this here or we'll end up double-locking the syncNodesMutex.
	if (type == BlockChainNode::NodeType::masterNode)
		gossip(key, CommandType::syncNodes, { {"d", std::string(buildSyncNodesInput())} });
}

std::string buildSyncNodesInput() {
	json mj;
	json vj;
	json sj;
	for (auto& x : masterNodes) {
		mj[x.first] = x.second.toJSONString();
	}

	for (auto& x : verifierNodes) {
		vj[x.first] = x.second.toJSONString();
	}

	for (auto& x : storageNodes) {
		sj[x.first] = x.second.toJSONString();
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

bool processIncomingData(std::string&& data, std::string&& key, time_t&& timestamp, bool&& fromWebSocket,WebSocketServer* wsServer) {
	std::thread t([data = std::move(data), key = std::move(key), timestamp = std::move(timestamp), fromWebSocket = std::move(fromWebSocket),wsServer]() mutable {
		FieldValueMap theFields;
		try {
			theFields = getFieldsAndValues(data);
			// Depending on the protocol used, the "k" field in the map might not be populated.
			// However, we provide a guarantee that if it's not populated, the node's key is sent in the key parameter.
			// We only need to check for the node's own message in the case of gossip. Messages over Web Socket are fine.
			if (!fromWebSocket && theFields.at("k") == dhtNode.getId().toString())
				return;
			// If we're not using OpenDHT, we need to manually set channel restrictions.
			if (!fromWebSocket) {
				// OpenDHT handles channel restrictions itself, but with other protocols the responsibility falls on us.
				// So, with other protocols we embed a chanid field that denotes the channel this message is for.
#if COMM_PROTOCOL != PROTOCOL_OPENDHT
				std::string& channelId = theFields.at("chanid");
				if (subscribedChannels.find(channelId) == subscribedChannels.end())
					return; // Ignore the message if it's not on a channel we're subscribed to.
#endif
			}
			if (theFields.find("tm") != theFields.end())
				timestamp = std::stoll(theFields["tm"]);
			else
				theFields.insert(FieldValueMap::value_type("tm", std::to_string(timestamp)));
			if (appStartupTime > timestamp) {
				logError("Dropping old message: %s", data);
				return;
			}
			if (!fromWebSocket) {
				std::lock_guard<std::mutex> lock(processedMessagesMutex);
				std::string& messageId = theFields.at("msgid");
				if (processedMessages.find(messageId) == processedMessages.end()) {
					processedMessages.insert(messageId);
				} else {
					logWarning("Dropping duplicate message " + data);
					return;
				}
			}
			else {
				std::string refId = dht::InfoHash::get(key+getGUID(36)+ theFields.at("tm")).toString();
				theFields["utxid"] = theFields.at("txid");
				theFields["txid"] = refId;				
				json j;
				j["type"] = "ack";
				j["id"] = refId;
				j["txid"] = theFields.at("utxid");				
				wsServer->send(key, j.dump());
			}
			// The ws field will be 1 if this payload is from a web socket, 0 otherwise.
			// This information could be useful in the data processors.
			theFields.insert(FieldValueMap::value_type("ws", (fromWebSocket) ? "1" : "0"));
			CommandType cmd = static_cast<CommandType>(atoi(theFields.at("c").c_str()));
			if (theFields.find("k") == theFields.end()) // Could already be inserted by non web socket libraries
				theFields.insert(FieldValueMap::value_type("k", key));
			if (cmd == CommandType::connectionRequest) {
				// If this connection was forwarded onto us by another node, they will put the connecting node's key in the key field.
				// This is because k is populated automatically by the sender's key already.
				std::string nodeKey = theFields.at("k");
				logInfo("New connection from peer " + nodeKey);
				BlockChainNode::NodeType type = static_cast<BlockChainNode::NodeType>(atoi(theFields.at("t").c_str()));
				std::string uri = theFields.at("u");
				addNode(nodeKey, uri, type);
				return;
			}
			CommandFunctionMap::iterator it = dataHandlers.find(cmd);
			if (it != dataHandlers.end()) {
				it->second(std::move(theFields));
				return;
			}
		} catch (const std::out_of_range& e) {
			logError("In processIncomingData, a key was not found. See the exception message for details: %s", e.what());
		} catch (const std::exception& e) {
			logError("ProcessIncomingData failed with message: %s", e.what());
		} catch (...) {
			logError("ProcessIncomingData failed with an unknown error. The message received was %s", data);
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
	if (storageKeys.size() >= (n + 1)) {
		return storageKeys[n];
	}
	return std::string("");
}

std::string& getFirstStorageNodeKey() {
	return storageKeys[0];
}

std::string getStorageNodeURL(const std::string& key) {
	return storageNodes[key].getURI();
}

size_t getNumberOfVerifiers() {
	return verifierNodes.size();
}

std::string& getVerifierNodeKey(const size_t& n) {
	return verifierKeys[n];
}

void print_usage() {
	std::cout << std::endl << std::endl;
	std::cout << "Usage: node.exe [--p port] [--ws port] [--b bootstrap_host[:port]] [--createaccount] [--l level] [--lf filename]" << std::endl << std::endl;
	std::cout << "--w: Set the port that this node's web socket server listens on. If this node has no web socket server, this option will be ignored." << std::endl;
	std::cout << "--p: Set the port that this node's OpenDHT instance listens on. This is the port that other nodes will use to connect to this node." << std::endl;
	std::cout << "--b: Set a bootstrap node. The port given here should be the port supplied with --p." << std::endl;
	std::cout << "--createaccount: Generate an account ID and associated private key. This command will show the account seed, account ID and private key. The private key and seed should be kept in a secure location and can be used to recover the account ID." << std::endl;
	std::cout << "--l: Set log level. Options are trace, debug, info, warning, error and fatal and increase in priority. Setting a logging level will log all messages of the specified priority and higher, so the least verbose option here is fatal, and the most verbose option here is trace. Defaults to info if not provided, and falls back to trace logging if an invalid level is provided." << std::endl;
	std::cout << "--lf: Specifies the file where logs are stored. Defaults to log.log. The user under which this node is running should have write access to the specified location." << std::endl;
	exit(0);
}

void parseArgs(int argc, char **argv, command_params* params, boost::optional<opt::options_description&> additionalOptions) {
	opt::options_description optionsList("Allowed command options");
	optionsList.add_options()
		("help,h", "Help screen")
		("p", opt::value<int>(&params->port)->default_value(DHT_DEFAULT_PORT), "Port")
		// Peerport is ignored if only DHT is handling comms.
		("peerport", opt::value<int>(&params->peerPort)->default_value(DEFAULT_SMUDGE_PORT), "Port")
		("w", opt::value<int>(&params->wsPort)->default_value(WS_DEFAULT_PORT), "Port")
		("l", opt::value<std::string>(&params->logLevel)->default_value(params->logLevel), "Logging Level")
		("lf", opt::value<std::string>(&params->logFile)->default_value(params->logFile), "Log File")
		("le", opt::value<bool>(&params->enableLogging)->default_value(params->enableLogging), "Log Enable")
		("b", opt::value<std::string>()->notifier([&params](std::string bootstrap) {
		params->bootstrap = dht::splitPort(boost::any_cast<std::string>(bootstrap));
		if (not params->bootstrap.first.empty() and params->bootstrap.second.empty()) {
			params->bootstrap.second = std::to_string(DHT_DEFAULT_PORT);
		}
	}), "bootstrap node")
		// Peer is ignored if only DHT is handling bootstrapping.
		("peer", opt::value<std::string>()->notifier([&params](std::string peer) {
		params->peer = dht::splitPort(boost::any_cast<std::string>(peer));
		if (not params->peer.first.empty() and params->peer.second.empty()) {
			params->peer.second = std::to_string(DEFAULT_SMUDGE_PORT);
		}
	}), "peer node")
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
	std::string publicKeyStr = base64_decode(base64publicKey);
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
	msgVec.assign(message.c_str(), message.c_str() + message.length());
	std::string decodedKey(base64_decode(base64key));
	std::vector<unsigned char> decodedKeyVec;
	decodedKeyVec.assign(decodedKey.c_str(), decodedKey.c_str() + decodedKey.length());
	std::string decodedSignature(base64_decode(base64signature));
	std::vector<unsigned char> decodedSignatureVec;
	decodedSignatureVec.assign(decodedSignature.c_str(), decodedSignature.c_str() + decodedSignature.length());
	size_t msgLength = message.length();
	return ed25519_verify(decodedSignatureVec.data(), msgVec.data(), msgLength, decodedKeyVec.data());
}

time_t getCurrentTimestamp() {
	return (currentTimestamp = appStartupTime + (time(0) - appSystemStartupTime));
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
		logError("Error: %s",e.what());
	}
	return "";
}

json generateAccount() {
	json j;
	uint8_t seed[32];
	uint8_t privateKey[64];
	uint8_t publicKey[32];
	if (ed25519_create_seed(seed) > 0)
		throw std::exception("Seed generation failed!");
	ed25519_create_keypair(publicKey, privateKey, seed);
	j["PrivateKey"] = base64_encode(std::vector<uint8_t>(privateKey, privateKey + 64));
	j["PublicKey"] = base64_encode(std::vector<uint8_t>(publicKey, publicKey + 32));
	j["Seed"] = base64_encode(std::vector<uint8_t>(seed, seed + 32));
	return json(j);
}

NodesMap& getMasterNodes() {
	return masterNodes;
}

NodesMap& getVerifierNodes() {
	return verifierNodes;
}

NodesMap& getStorageNodes() {
	return storageNodes;
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

#if COMM_PROTOCOL == PROTOCOL_SMUDGE
void startSmudge(DataHandler callback, int port, std::string bootstrap) {
	initializeGossip(callback, port, bootstrap);
	Begin();
	startMessageMonitor();
}
#endif

std::string compressString(std::string data) {
	removeNullTerminators(data);
	std::stringstream compressed;
	std::stringstream original;
	original << data;
	boost::iostreams::filtering_streambuf<boost::iostreams::input> out;
	out.push(boost::iostreams::zlib_compressor(9));
	out.push(original);
	boost::iostreams::copy(out, compressed);
	std::string strOfStream(compressed.str());
	std::string headers; // contains a comma-separated list of null terminator positions
	std::string::iterator it = strOfStream.begin();
	int position = 0;
	// Next, we'll remove all null terminators and add them to the front of the string as headers.
	while (it != strOfStream.end()) {
		if (*it == '\0') {
			if (!headers.empty())
				headers += ",";
			headers += std::to_string(position);
			it = strOfStream.erase(it);
		} else {
			it++;
		}
		position++;
	}
	return std::string(headers + "_" + strOfStream);
}

std::string decompressString(const std::string &data) {
	std::vector<std::string> packetParts;
	boost::split(packetParts, data, boost::is_any_of("_"));
	// There should only be two elements in the vector but in case there are more underscore-separated parts, we need to re-concatenate them
	std::string compressedString(packetParts[1]);
	if (packetParts.size() > 2) { // There are more underscores besides the one separating the header from the compressed data
		for (std::vector<std::string>::iterator it = packetParts.begin() + 2; it != packetParts.end(); it++)
			compressedString += "_" + *it;
	}
	// Next, we need to add the null terminators we removed from the compressed string.
	std::string headers(packetParts[0]);
	packetParts.clear();
	boost::split(packetParts, headers, boost::is_any_of(","));
	// PacketParts contains the 0-based indices of the null terminators.
	for (std::vector<std::string>::iterator it = packetParts.begin(); it != packetParts.end(); it++) {
		compressedString.insert(compressedString.begin() + std::stoi(*it), '\0');
	}
	std::stringstream compressed;
	std::stringstream decompressed;
	compressed << compressedString;
	boost::iostreams::filtering_streambuf<boost::iostreams::input> in;
	in.push(boost::iostreams::zlib_decompressor());
	in.push(compressed);
	boost::iostreams::copy(in, decompressed);
	return std::string(decompressed.str());
}

void removeNullTerminators(std::string& str) {
	size_t start = str.find('\0');
	if (start == std::string::npos)
		return;
	std::string::iterator it = str.begin() + start;
	while (it != str.end()) {
		if (*it == '\0')
			it = str.erase(it);
		else
			it++;
	}
}

std::string base64_encodeString(std::string message) {
	std::vector<uint8_t> vec(message.begin(), message.end());
	dht::Blob blob(vec);
	return  std::string(base64_encode(blob));
}

std::vector<std::string> fragmentPacket(const size_t& nonce, const std::string& packet) {
	const int packetSize = 50;
	int numPackets = packet.length() / packetSize;
	if ((packet.length() % packetSize) != 0)
		numPackets++;
	if (numPackets > 1) {
		dhtPut(std::to_string(nonce), packet);
		return std::vector<std::string>();
	}
	std::vector<std::string> packets(numPackets);
	int currentPacket = 0;
	int lastPacketIndex = numPackets - 1;
	int nextPacketStartPos = 0;
	std::string lastPacketIndexStr(std::to_string(lastPacketIndex));
	if (lastPacketIndexStr.length() == 1) {
		lastPacketIndexStr = std::string("0").append(lastPacketIndexStr);
	}
	std::string idStr = std::to_string(nonce);
	while (currentPacket < numPackets) {
		std::string currentPacketStr(std::to_string(currentPacket));
		if (currentPacketStr.length() == 1) {
			currentPacketStr = std::string("0").append(currentPacketStr);
		}
		std::string thePacket(idStr);
		thePacket += currentPacketStr;
		thePacket += lastPacketIndexStr;
		thePacket += ":" + packet.substr(nextPacketStartPos, packetSize);
		packets[currentPacket++] = thePacket;
		nextPacketStartPos += packetSize;
	}
	return std::vector<std::string>(packets);
}

void registerGlobalShutdownHandler() {
	std::atexit([]() {
		logInfo("Exporting DHT data....");
		std::ofstream myfile("dhtDataExport.bin", std::ios::binary);
		msgpack::pack(myfile, dhtNode.exportValues());		
		logInfo("Successfully exported dht info");
		logInfo("Stopping OpenDHT");
		// Following code gotten from opendht/tools/dhtnode.cpp
		std::condition_variable cv;
		std::mutex m;
		std::atomic_bool done{ false };
		dhtNode.shutdown([&]()
		{
			std::lock_guard<std::mutex> lk(m);
			done = true;
			cv.notify_all();
		});
		// wait for shutdown
		std::unique_lock<std::mutex> lk(m);
		cv.wait(lk, [&]() { return done.load(); });
		dhtNode.join();
	});
}

void registerSigint() {
	signal(SIGINT, [](int s) {
		signal(SIGINT, SIG_DFL); // Reset the SIGINT handler to default so user can press CTRL+C again to force-shutdown.
		std::cout << "Shutting down..." << std::endl;
		std::cout << "Press CTRL+C again to force a hsutdown." << std::endl;
		exit(s);
	});
}

std::string formatString(const std::string& message, va_list args) {
	int ret = vsnprintf(nullptr, 0, message.c_str(), args);
	std::vector<char> buffer(ret + 1, '\0');
	ret = vsnprintf(buffer.data(), ret + 1, message.c_str(), args);
	if (ret < 0)
		return "";
	return std::string(buffer.data());
}

void logTrace(const std::string message, ...) {
	va_list args;
	va_start(args, message);
	BOOST_LOG_TRIVIAL(trace) << formatString(message, args);
	va_end(args);
}

void logDebug(const std::string message, ...) {
	va_list args;
	va_start(args, message);
	BOOST_LOG_TRIVIAL(debug) << formatString(message, args);
	va_end(args);
}

void logInfo(const std::string message, ...) {
	va_list args;
	va_start(args, message);
	BOOST_LOG_TRIVIAL(info) << formatString(message, args);
	va_end(args);
}

void logWarning(const std::string message, ...) {
	va_list args;
	va_start(args, message);
	BOOST_LOG_TRIVIAL(warning) << formatString(message, args);
	va_end(args);
}

void logError(const std::string message, ...) {
	va_list args;
	va_start(args, message);
	BOOST_LOG_TRIVIAL(error) << formatString(message, args);
	va_end(args);
}

void logFatal(const std::string message, ...) {
	va_list args;
	va_start(args, message);
	BOOST_LOG_TRIVIAL(fatal) << formatString(message, args);
	va_end(args);
}

void setLoggingLevel(const std::string& level, const std::string& logFile, const bool& enableLog) {
	boost::log::core::get()->set_logging_enabled(enableLog);
	boost::log::register_simple_formatter_factory<boost::log::trivial::severity_level, char>("Severity");
	// We'll set up both console and file logging.
	// Info logs will be printed to the stdout (unconditionally) as well as to a file (if severity level is set appropriately.)
	boost::shared_ptr< boost::log::sinks::synchronous_sink< boost::log::sinks::basic_text_ostream_backend< char > >>  consoleSink = boost::log::add_console_log(
		std::cout, // Attach to stdout stream
		boost::log::keywords::format = "%Message%"
	);
	consoleSink->set_filter(boost::log::trivial::severity == boost::log::trivial::info);
	boost::shared_ptr< boost::log::sinks::synchronous_sink< boost::log::sinks::text_file_backend > > fileSink = boost::log::add_file_log(
		boost::log::keywords::file_name = logFile,
		boost::log::keywords::auto_flush = true,
		boost::log::keywords::format = "%Severity%: %Message% received on %TimeStamp%"
	);
	boost::log::add_common_attributes();
	if (level == "trace") {
		fileSink->set_filter(boost::log::trivial::severity >= boost::log::trivial::trace);
		logInfo("Log level set to TRACE");
	} else if (level == "debug") {
		fileSink->set_filter(boost::log::trivial::severity >= boost::log::trivial::debug);
		logInfo("Log level set to DEBUG");
	} else if (level == "info") {
		fileSink->set_filter(boost::log::trivial::severity >= boost::log::trivial::info);
		logInfo("Log level set to INFO");
	} else if (level == "warning") {
		fileSink->set_filter(boost::log::trivial::severity >= boost::log::trivial::warning);
		logInfo("Log level set to WARNING");
	} else if (level == "error") {
		fileSink->set_filter(boost::log::trivial::severity >= boost::log::trivial::error);
		logInfo("Log level set to ERROR");
	} else if (level == "fatal") {
		fileSink->set_filter(boost::log::trivial::severity >= boost::log::trivial::fatal);
		logInfo("Log level set to FATAL");
	} else {
		fileSink->set_filter(boost::log::trivial::severity >= boost::log::trivial::info);
		logInfo("Invalid log level supplied. Setting to info logging");
	}
	logInfo("Log file set to " + logFile);
}