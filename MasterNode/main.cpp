#include "../MasterNodeLib/masterNodeLib.h"
std::mutex blockDetailRequestsMutex;
TxAndConnectionMap blockDetailRequests;
std::mutex txDetailRequestsMutex;
TxAndConnectionMap txDetailRequests;
int main(int argc, char* argv[]) {
	try {		
		command_params params;
		parseArgs(argc, argv, &params);
		logInfo("Starting Master Node");
		lastRoundTimestamp = getCurrentTimestamp();
		lastCleanupTime = currentTimestamp;
		initializeNextVotingTimestamp();
		nodeIdentity = initializeKeypair();
		dhtNode.run(params.port, *nodeIdentity, true, NETWORK_ID);
		if (not params.bootstrap.first.empty()) {
			dhtNode.bootstrap(params.bootstrap.first.c_str(), params.bootstrap.second.c_str());
			logInfo("Added bootstrap node " + params.bootstrap.first + ":" + params.bootstrap.second);
		}

		// Listen on the master node global channel
		subscribeTo("masternodes");
		// Listen on the master node's individual channel
		subscribeTo(dhtNode.getId().toString());		
		registerDataHandlers(
			{
			{ CommandType::codeDelivery, [](FieldValueMap&& theFields) {
			// TODO: Implement saving function to Storage Node. This command was previously used to send actual code to the Compute Node.
			/*
				std::string& tenantId = theFields["tid"];
				std::string& code = theFields["co"];
				std::string& inputs = theFields["i"];
				std::string& signature = theFields["s"];
				std::string& publicKey = theFields["k"];
				processCodePacket(tenantId, code, inputs, signature, publicKey);
				*/
			}},
			{ CommandType::exeFunction, [](FieldValueMap&& theFields) {
				std::string& tenantId = theFields["tid"];
				std::string& code = theFields["h"];
				std::string& inputs = theFields["i"];
				std::string& signature = theFields["s"];
				std::string& publicKey = theFields["k"];
				std::string& utxId = theFields["utxid"];
				std::string& htxId = theFields["txid"];
				processCodePacket(utxId, htxId, tenantId, code, inputs, signature, publicKey);
			}},
			{CommandType::verificationResult, [](FieldValueMap&& theFields) {
				// Master node will get txId and hash of output.
				std::string& txId = theFields.at("txid");
				std::string& hash = theFields.at("h");
				// If the verification result is sent by a Compute Node, the Master Node will also receive the actual output of the function.
				FieldValueMap::iterator itOutput = theFields.find("o");
				verifyTransaction(txId, hash, (itOutput != theFields.end())?itOutput->second:"");
			}},
			{CommandType::exception, [](FieldValueMap&& theFields) {
				// Master node will get txId and exception.
				std::string txDump;
				std::string txstr = (theFields.find("tx") != theFields.end())? theFields.at("tx"):"";
				if (txstr.empty()) {
					std::string& txId = theFields.at("txid");
					std::string& exp = theFields.at("e");
					std::cerr << "Code execution exception: " << exp << std::endl;
					{
						std::lock_guard<std::mutex> lock(transactionsMutex);
						// This transaction could have been erased due to already reaching verification.
						if (transactions.find(txId) == transactions.end())
							return;
						Transaction& tx = transactions.at(txId);
						tx.setOutput("");
						tx.setFunctionId("");
						tx.setException(exp);
						// send the exception back to the client.
						if (tx.isComputeNode()) {
							json j;
							j["txid"] = txId;
							j["data"] = exp;
							wsServer.send(tx.getPublicKey(), j.dump());
						}
						txDump = tx.toJson().dump();
						if (isBlockProducer()) {
							queueOut.push_back(std::move(tx));
						}
						transactions.erase(txId);
					}
					// Send this transaction to 20 other Master Nodes.
					gossip("master21", CommandType::exception, { {"tx", txDump } });
				} else { // If txstr is not empty
					json j = json::parse(txstr);
					Transaction tx;
					tx.fromJson(j);
					if (isBlockProducer()) {
						std::lock_guard<std::mutex> lock(transactionsMutex);
						queueOut.push_back(std::move(tx));
					}
				}
			}},
			{CommandType::choseRandomNumber, [](FieldValueMap&& theFields) {
				std::string& signature = theFields.at("s");
				std::string& key = theFields.at("key");
				registerVote(std::move(signature), std::move(key));
			}},
			{CommandType::txNotification, [](FieldValueMap&& theFields) {
				// This call will either create a new transaction or update an existing one.
				processIncomingTransaction(theFields.at("utxid"), theFields.at("tx"));
			}},
			{ CommandType::syncNodes, [](FieldValueMap&& theFields) {
				// Used to send known nodes to the newly connected node.
				// The newly connected node is using this node as a bootstrap.
				std::string data = theFields.at("d");
				syncNodes(data);
			} },
			{ CommandType::storageurl, [](FieldValueMap&& theFields) {
				json j;
				std::string& tenantId = theFields.at("tid");
				std::string& txid = theFields.at("txid");
				std::string& publicKey = theFields.at("k");
				j["txid"] = txid;
				std::unique_lock<std::mutex> lock(nodesSyncMutex);
				size_t n = getNumberOfStorageNodes();
				if (n == 0)
					j["data"] = "n/a";
				else {
					std::string key(getStorageNodeKey(generateRandomNumber(1, static_cast<uint32_t>(n))));
					std::string url = getStorageNodeURL(key);
					j["data"] = url;
				}
				lock.unlock();
				wsServer.send(publicKey, j.dump());
			}},
			{ CommandType::stateChangeResponse, [](FieldValueMap&& theFields) {
				std::string& txId = theFields.at("txid");
				std::string& data = theFields.at("data");
				std::string& id = theFields.at("id");
				std::unique_lock<std::mutex> lock(transactionsMutex);
				TransactionsMap::const_iterator it = transactions.find(txId);
				if (it == transactions.cend())
					return;
				std::string clientKey = it->second.getPublicKey();
				lock.unlock();
				json j;
				j["txid"] = txId;
				j["data"] = data;
				j["id"] = id;
				wsServer.send(clientKey, j.dump());
			} },
			{ CommandType::verifyBlock, [](FieldValueMap&& theFields) {
				// TODO: During token integration, we should check the whitelist based on the sender's public key.
				json b = json::parse(base64_decode(theFields.at("b")));
				std::string& timestamp = theFields.at("tm");
				std::string producerPKey = base64_decode(b["k"].get<std::string>());
				std::string s = b["si"].get<std::string>();
				std::string blocknumber = b["n"].get<std::string>();
				long bid = std::stol(b["n"].get<std::string>());
				if (blocks.find(bid) == blocks.end())
					return;
				BlockUtil::Block& block = blocks.at(bid);
				dht::Blob blobKey;
				blobKey.assign(producerPKey.c_str(), producerPKey.c_str() + producerPKey.length() + 1);
				dht::crypto::PublicKey publicKey(blobKey);
				getBlockStorageNode();
				if (blockStorageNode.empty())
					return;
				std::string si = createX509Signature(block.getHash(), nodeIdentity);
				json j;
				j["k"] = base64_encodeString(nodeIdentity->first->getPublicKey().toString());
				j["si"] = si;
				int producer = getBlockProducerFor(std::stol(timestamp));
				if (producer == -1)
					return;

				std::string data = base64_encodeString(j.dump());

				if (blockProducers.at(producer).publicKey != producerPKey) { // If the given block producer shouldn't have produced this block.
					j["s"] = "false";
					gossip(blockStorageNode, CommandType::verifyBlock, { {"b", blocknumber}, {"d", data} });
				} else if (verifyX509Signature(block.getHash(), s, publicKey)) {
					j["s"] = "true";
					gossip(blockStorageNode, CommandType::verifyBlock, { {"b", blocknumber}, {"d", data} });
				} else { // The signature doesn't check out...
					j["s"] = "false";
					gossip(blockStorageNode, CommandType::verifyBlock, { {"b", blocknumber}, {"d", data} });
				}
				blocks.erase(bid);
			} }
			
			}
		);
		dhtNode.setOnStatusChanged([](dht::NodeStatus status, dht::NodeStatus status2) {
			auto nodeStatus = std::max(status, status2);
			if (nodeStatus == dht::NodeStatus::Connected)
				logTrace("Connected");
			else if (nodeStatus == dht::NodeStatus::Disconnected)
				logTrace("Disconnected");
			else
				logTrace("Connecting");
		});
		// If a node boots up in the middle of voting, we won't let it participate since it could get incorrect results because
		// some nodes might have already cast ballots before this node joined the network.
		if (inVotingTime())
			ignoreThisVotingRound = true;
		COMM_PROTOCOL_INIT;
		// A call to this macro registers a global exit handler, but before
		// the global handler is called we should call the master node handler since it's a higher level handler.
		std::atexit([]() {
			logInfo("Shutting down Master Node loop");
			requestedMasterNodeStop = true;
			while (runningMasterNode)
				std::this_thread::sleep_for(std::chrono::nanoseconds(1));
			logInfo("Shutting down web socket server");
			wsServer.shutdown();
		});
		logInfo("Node public key fingerprint: " + dhtNode.getId().toString());
		// send connection request to bootstrap master nodes
		sendConnectionRequest(params.wsPort);
		std::future<void> result(std::async([p = params.wsPort]() { wsServer.run(p); }));		
		logInfo("Node is up and running on port %d ! Press CTRL+C to quit.", params.port);
		runLoop();
	} catch (const std::exception& e) {
		logFatal(e.what() );
		return 1;
	} catch (...) {
		logFatal("Fatal error occurred in the Master Node main thread.");
	}
	return 0;
}