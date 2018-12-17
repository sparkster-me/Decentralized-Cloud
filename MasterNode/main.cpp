#include "../MasterNodeLib/masterNodeLib.h"
int main(int argc, char* argv[]) {
	try {
		opt::options_description optionsList("MasterNode command options");
		optionsList.add_options()
		("s", opt::value<time_t>(&openVotingStartMinute)->default_value(openVotingStartMinute), "start");
		command_params params;
		parseArgs(argc, argv,&params,optionsList);
		std::cout << "Starting Master Node" << std::endl;
		lastRoundTimestamp = getCurrentTimestamp();
		updateNextVotingTimestamp();
		dhtNode = new dht::DhtRunner();
		std::cout << "Initializing keypair..." << std::endl;
		nodeIdentity = initializeKeypair();		
		dhtNode->run(params.port, *nodeIdentity, true);
		if (not params.bootstrap.first.empty()) {
			dhtNode->bootstrap(params.bootstrap.first, params.bootstrap.second);
			std::cout << "Added bootstrap nodes" << std::endl;
		}
		// Listen on the master node global channel
		subscribeTo(dhtNode, "masternodes");
		// Listen on the master node's individual channel
		subscribeTo(dhtNode, dhtNode->getId().toString());
		std::cout << "id: " << dhtNode->getId() << std::endl;
		registerDataHandlers(
			{
			{ CommandType::codeDelivery, [](FieldValueMap&& theFields) {
				std::string& tenantId = theFields["tid"];
				std::string& code = theFields["co"];
				std::string& inputs = theFields["i"];
				std::string& signature = theFields["s"];
				std::string& publicKey = theFields["k"];
				processCodePacket(tenantId, code, inputs, signature, publicKey);
			}},
			{ CommandType::exeFunction, [](FieldValueMap&& theFields) {
				std::string& tenantId = theFields["tid"];
				std::string& code = theFields["h"];
				std::string& inputs = theFields["i"];
				std::string& signature = theFields["s"];
				std::string& publicKey = theFields["k"];
				processCodePacket(tenantId, code, inputs, signature, publicKey);
			}},
			{CommandType::verificationResult, [](FieldValueMap&& theFields) {
				// Master node will get txId and hash of output.
				std::string& txId = theFields.at("txid");
				std::string& hash = theFields.at("h");
				std::lock_guard<std::mutex> lock(transactionsMutex);
				// This transaction could have been erased due to already reaching verification.
				if (transactions.find(txId) == transactions.end())
					return;
				Transaction& tx = transactions.at(txId);
				tx.verifyHash(hash); // Will update totalVerified if passes
				std::cout << "\n verification Result : " << hash << std::endl;
				if (tx.getTotalVerified() >= MAJORITY_THRESHOLD) {
					std::cout << "\n verification Result MAJORITY_THRESHOLD Success : " << hash << std::endl;
					// If this Master Node is the one that spawned the Compute Node for this transaction, it will also be
					// the one to tell the Compute Node to commit the transaction.
					// It will also be the one to send the result of the code's execution back to the client.
					if (tx.isComputeNode()) {
						std::string key(getStorageNodeKey(generateRandomNumber(0, static_cast<uint32_t>(getNumberOfStorageNodes()))));

						gossip(dhtNode, tx.getComputeNodeId(), CommandType::canCommit, { { "txid", tx.getTxId() }, { "sid", key }, { "tid", "" } });
						json j;
						j["txid"] = tx.getTxId();
						j["data"] = tx.getOutput();
						std::cout << "\n verification Result User Response : " << j.dump() << std::endl;
						wsServer->send(tx.getPublicKey(), j.dump());
					}
					if (isBlockProducer()) {
						queueOut.push_back(std::move(tx));
					}
					transactions.erase(txId);
				}
			}},
			{CommandType::exception, [](FieldValueMap&& theFields) {
				// Master node will get txId and exception.
				std::string& txstr = theFields["tx"];
				//check whether request form compute node or mater node
				if (txstr.empty()) {
					std::string& txId = theFields["txid"];
					std::string& exp = theFields["e"];
					// This transaction could have been erased due to already reaching verification.
					if (transactions.find(txId) == transactions.end())
						return;

					Transaction& tx = transactions.at(txId);
					{
						std::lock_guard<std::mutex> lock(transactionsMutex);
						tx.setOutput(NULL);
						tx.setCode(NULL);
						tx.setException(exp);

						// send the exception back to the client.
						if (tx.isComputeNode()) {
							json j;
							j["txid"] = txId;
							j["data"] = exp;

							wsServer->send(tx.getPublicKey(), j.dump());
						}
						if (isBlockProducer()) {
							queueOut.push_back(std::move(tx));
						}
						transactions.erase(txId);
					}

					// Send this transaction to 20 other Master Nodes.
					gossip(dhtNode, "master21", CommandType::exception, { {"tx", tx.toJson().dump() } });
				} else {
					json j = json::parse(txstr);
					Transaction tx;
					tx.fromJson(j);

					if (isBlockProducer()) {
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
				processIncomingTransaction(theFields.at("tx"));
			}},
			{ CommandType::syncNodes, [](FieldValueMap&& theFields) {
				// to synch the data between nodes
				std::string data = theFields.at("d");
				syncNodes(data);
			} },
			{CommandType::deterministicCodeDelivery, [](FieldValueMap&& theFields) {
				std::string& txId = theFields.at("txid");
				std::string& hash = theFields.at("h");
				std::string& output = theFields.at("o");
				std::lock_guard<std::mutex> lock(transactionsMutex);
				TransactionsMap::iterator it = transactions.find(txId);
				// If a Master Node receives deterministicCodeResult before the TXNotification command,
				// this transaction will not exist yet. So, we should build it with what we have and then when
				// txNotification is received, the rest of the transaction will get filled in.
				if (it != transactions.end()) {
					it->second.verifyHash(hash);
					it->second.setOutput(output);
					std::cout << "Tx Data : " << it->second.toJson().dump() << std::endl;
				} else {
					Transaction tx(txId, hash, output);
					tx.verifyHash(hash); // Verify 1.
					transactions.insert(TransactionsMap::value_type(txId, std::move(tx)));
				}
			}},
			{ CommandType::storageurl, [](FieldValueMap&& theFields) {
				std::string& tenantId = theFields.at("tid");
				std::string& txid = theFields.at("txid");
				std::string& publicKey = theFields.at("k");
				std::string key(getStorageNodeKey(generateRandomNumber(0, static_cast<uint32_t>(getNumberOfStorageNodes()))));
				std::string url = getStorageNodeURL(key);
				json j;
				j["txid"] = txid;
				j["data"] = url;

				wsServer->send(publicKey, j.dump());
			}},
			{ CommandType::verifyBlock, [](FieldValueMap&& theFields) {
				// TODO: During token integration, we should check the whitelist based on the sender's public key.
				
				json b = json::parse(theFields.at("b"));
				std::string& timestamp = theFields.at("tm");
				std::string producerPKey = b["k"].get<std::string>();
				std::string s = b["si"].get<std::string>();
				std::string blocknumber = b["n"].get<std::string>();
				long bid = std::stol(b["n"].get<std::string>());
				if (blocks.find(bid) == blocks.end())
					return;
				Block& block = blocks.at(bid);
				dht::Blob blobKey;
				blobKey.assign(producerPKey.c_str(), producerPKey.c_str() + producerPKey.length() + 1);
				dht::crypto::PublicKey publicKey(blobKey);
				std::string key(getStorageNodeKey(generateRandomNumber(0, static_cast<uint32_t>(getNumberOfStorageNodes()))));
				std::string si = createX509Signature(block.getHash(), nodeIdentity);
				json j;
				j["k"] = nodeIdentity->first->getPublicKey().toString();
				j["si"] = si;
				int producer = getBlockProducerFor(std::stol(timestamp));
				if (producer == -1)
					return;
								
				std::string data = j.dump();
				std::vector<uint8_t> vec(data.begin(), data.end());
				dht::Blob blob(vec);
				if (blockProducers.at(producer).publicKey != producerPKey) { // If the given block producer shouldn't have produced this block.
					j["s"] = "false";
					gossip(dhtNode, key, CommandType::verifyBlock, { {"b", blocknumber}, {"d", base64_encode(blob)} });
				} else if (verifyX509Signature(block.getHash(), s, publicKey)) {
					j["s"] = "true";
					gossip(dhtNode, key, CommandType::verifyBlock, { {"b", blocknumber}, {"d", base64_encode(blob)} });
				} else { // The signature doesn't check out...
					j["s"] = "false";
					gossip(dhtNode, key, CommandType::verifyBlock, { {"b", blocknumber}, {"d", base64_encode(blob)} });
				}
				blocks.erase(bid);
			} }
			}
		);
		// send connection request to bootstrap master nodes
		sendConnectionRequest(params.port);
		// If a node boots up in the middle of voting, we won't let it participate since it could get incorrect results because
		// some nodes might have already cast ballots before this node joined the network.
		if (inVotingTime())
			ignoreThisVotingRound = true;
		std::thread t(runLoop);
		wsServer = new WebSocketServer();
		std::cout << "Node up! Press CTRL+C to quit." << std::endl;
		wsServer->run(params.wsPort);
	} catch (const std::exception e) {
		std::cerr << e.what() << std::endl;
		return 1;
	}
	return 0;
}