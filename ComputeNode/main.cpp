#include "ComputeNode.h"

/*
	Method to process input
*/
void processRequest(FieldValueMap& theFields) {
	std::string& tenantId = theFields["tid"];
	std::string& txid = theFields["txid"];
	std::string& timeStamp = theFields["tm"];
	std::string& co = theFields["co"];
	std::string& k = theFields["k"];
	std::string& nodeType = theFields["com"];
	std::string& storageNode = theFields["s"];
	std::string& input = theFields["i"];

	// store data into local memory
	RequestData rq(txid, timeStamp, input, tenantId, co, k, nodeType, storageNode);
	rds.insert(RequestDataMap::value_type(txid, std::move(rq)));

	// retrieve function definition from storageNode
	gossip(dhtNode, storageNode, CommandType::fetchFunction, { {"txid", txid }, {"h", co }, {"tid", tenantId } });
}

/*
	This method will execute js code
*/
void executeJsCode(FieldValueMap& theFields) {
	std::string& txid = theFields["txid"];
	std::string& code = theFields["data"];
	RequestDataMap::iterator it = rds.find(txid);

	// check if transaction data exists in local memory
	if (it == rds.end()) {
		return;
	}

	RequestData rq = it->second;
	std::string tenantId = rq.getTenantId();
	std::string timeStamp = rq.getTimeStamp();
	std::string h = rq.getHash();
	std::string k = rq.getChannelId();
	std::string nodeType = rq.getNodeType();
	std::string storageNode = rq.getStorageNode();
	std::string i = rq.getInput();

	try {
		JsProcessor jsProcessor;
		std::string result = jsProcessor.executeScript(code, i);
		std::string hash(hashCode(result));
		NodeType type = static_cast<NodeType>(std::stoi(nodeType));
		switch (type) {
		case NodeType::Compute:
		{
			//store transaction result and state change commands in memory
			TransactionResult transactionResult(txid, timeStamp);
			transactionResult.set_deterministicCode(result);
			trs.insert(TransactionResultMap::value_type(txid, std::move(transactionResult)));

			// Send deterministic code to 21 Master Nodes.
			gossip(dhtNode, "master21", CommandType::deterministicCodeDelivery, { {"txid", txid }, {"o", result }, {"h", hash } });
			break;
		}
		case NodeType::Verifier:
		{
			// Send this result hash to 21 Master Nodes.
			gossip(dhtNode, "master21", CommandType::verificationResult, { {"txid", txid },{"h", hash } });
			break;
		}
		}
		// erase data from local memory after processing the input
		rds.erase(txid);
	} catch (std::exception e) {
		std::cerr << e.what() << std::endl;
		// Send this exception to Master Node.
		std::string exp(e.what());
		gossip(dhtNode, k, CommandType::exception, { {"txid", txid },{"e", exp } });
	}
}

/*
	Process state change commands
*/
void processStateChangeCmds(FieldValueMap theFields) {
	try {
		std::string& tenantId = theFields["tid"];
		std::string& transactionId = theFields["txid"];
		std::string& storageNodeChannelId = theFields["sid"];

		std::cout << "Executing compute node command" << endl;

		std::unordered_map<std::string, TransactionResult>::iterator itr;
		itr = trs.find(transactionId);

		// For given transacton if state change command exists then update storage node
		if (itr != trs.end() && itr->second.get_stateChangeCmds().size() > 0) {
			json j = itr->second.get_stateChangeCmds();
			gossip(dhtNode, storageNodeChannelId, CommandType::stateChange, { {"txid", transactionId }, {"tid", tenantId }, {"data", j.dump() } });
		}
	} catch (std::exception e) {
		std::cerr << e.what() << std::endl;
	}
}

/*
	generate hashCode
*/
std::string hashCode(std::string& data) {
	dht::InfoHash infoHash;
	return infoHash.get(data).toString();
}

/*
	share node information to masternodes
*/
void sendConnectionRequest(int port) {
	FieldValueMap theFields;
	char cmdstr[5];
	sprintf(cmdstr, "%d", static_cast<uint8_t>(BlockChainNode::NodeType::verifierNode));
	theFields.insert(FieldValueMap::value_type(std::string("t"), std::string(cmdstr)));

	std::string ip(getPublicIP());
	boost::trim(ip);

	// insert verifierNode uri
	theFields.insert(FieldValueMap::value_type(std::string("u"), std::string("ws://" + ip + std::string(":") + std::to_string(port))));

	// share verifier information to all master nodes
	gossip(dhtNode, "masternodes", CommandType::connectionRequest, theFields);
}

int main(int argc, char* argv[]) {

	//js engine initialization
	v8::V8::InitializeICUDefaultLocation(argv[0]);
	v8::V8::InitializeExternalStartupData(argv[0]);
	std::unique_ptr<v8::Platform> platform = v8::platform::NewDefaultPlatform();
	v8::V8::InitializePlatform(platform.get());
	v8::V8::Initialize();

	try {

		//Initialize DHT Node and RUN
		dhtNode = new dht::DhtRunner();
		nodeIdentity = initializeKeypair();
		command_params params;
		parseArgs(argc, argv, &params);
		dhtNode->run(params.port, *nodeIdentity, true);
		if (not params.bootstrap.first.empty()) {
			dhtNode->bootstrap(params.bootstrap.first, params.bootstrap.second);
			std::cout << "Added bootstrap nodes" << std::endl;
		}

		// Listen to channel for any new requests
		subscribeTo(dhtNode, dhtNode->getId().toString());
		std::cout << "Starting Compute Node" << std::endl;
		std::cout << "id " << dhtNode->getId() << std::endl;
		std::cout << "node id " << dhtNode->getNodeId() << std::endl;

		registerDataHandlers(
			{
			{ CommandType::codeDelivery, [](FieldValueMap&& theFields) {
				processRequest(theFields);
			}},
			{ CommandType::canCommit, [](FieldValueMap&& theFields) {
				processStateChangeCmds(theFields);
			}},
			{ CommandType::fetchFunction, [](FieldValueMap&& theFields) {
				executeJsCode(theFields);
			}}
			}
		);

		// send connection request to masternodes
		sendConnectionRequest(params.wsPort);
		std::cout << "Node is up and running..." << endl;

		while (true) { std::this_thread::sleep_for(std::chrono::seconds(60)); }
	} catch (const std::exception e) {
		std::cerr << e.what() << std::endl;
		return 1;
	}
	return 0;
}