#include "ComputeNode.h"

/*
	Method to process input
*/
void processRequest(FieldValueMap& theFields) {
	try {
		std::string& tenantId = theFields["tid"];
		std::string& txid = theFields["txid"];
		std::string& timeStamp = theFields["tm"];
		std::string& co = theFields["co"];
		std::string& k = theFields["k"];
		std::string& nodeType = theFields["com"];
		std::string& storageNode = theFields["s"];
		std::string& input = theFields["i"];
		std::string reqId = JsProcessor::generateUUID();

		// store data into local memory
		RequestData rq(reqId, txid, timeStamp, input, tenantId, co, k, nodeType, storageNode);
		{
			std::lock_guard<std::mutex> lock(rdsMutex);
			logDebug("Added new transaction " + reqId + " with nonce" + theFields["nonce"]);
			rds.insert(RequestDataMap::value_type(reqId, std::move(rq)));
		}

		// retrieve function definition from storageNode
		gossip(storageNode, CommandType::fetchFunction, { {"txid", reqId }, {"h", co }, {"tid", tenantId } });
	}
	catch (const std::exception e) {
		logError(e.what());
	}
}

/*
	This method will execute js code
*/
void executeJsCode(FieldValueMap& theFields) {
	std::string& txid = theFields["txid"];
	std::string& code = theFields["data"];
	logDebug("Received code from Storage node for req ID " + txid + " with code: " + code);
	code = json::parse(code)["code"].get<std::string>();

	std::unique_lock<std::mutex> lock(rdsMutex);
	RequestDataMap::iterator it = rds.find(txid);
	// check if transaction data exists in local memory
	if (it == rds.end()) {
		logError("transaction not found " + txid);
		return;
	}
	RequestData& rq = it->second;
	lock.unlock();
	std::string tenantId = rq.getTenantId();
	std::string timeStamp = rq.getTimeStamp();
	std::string h = rq.getHash();
	std::string k = rq.getChannelId();
	std::string storageNode = rq.getStorageNode();
	std::string reqId = txid;
	txid = rq.getTxId();

	try {
		JsProcessor jsProcessor;
		json j = jsProcessor.executeScript(code, rq.getInput(), tenantId);
		std::string result = j["result"].get<std::string>();
		std::string hash(hashCode(result));
		NodeType type = static_cast<NodeType>(std::stoi(rq.getNodeType()));
		switch (type) {
		case NodeType::Compute:
		{
			//store transaction result and state change commands in memory
			TransactionResult transactionResult(txid, timeStamp);
			transactionResult.set_stateChangeCmds(j["statechangecmds"].get<std::string>());
			{
				std::lock_guard<std::mutex> lock(txMutex);
				trs.insert(TransactionResultMap::value_type(txid, std::move(transactionResult)));
			}
			// In this case, we'll send not only the hash but also the function's output to fill in the transaction.
			logDebug("Sending [compute node] result with reqId " + reqId + " and master node txId " + txid + " to master21");
			gossip("master21", CommandType::verificationResult, { {"txid", txid }, {"o", result }, {"h", hash } });
			break;
		}
		case NodeType::Verifier:
		{
			// Send this result hash to 21 Master Nodes.
			logDebug("Sending [verifier node] result with reqId " + reqId + " and master node txId " + txid + " to master21");
			gossip("master21", CommandType::verificationResult, { {"txid", txid },{"h", hash } });
			break;
		}
		}
		// erase data from local memory after processing the input
		std::lock_guard<std::mutex> lock(rdsMutex);
		rds.erase(reqId);
	}
	catch (std::exception e) {
		logError(std::string("In execute JS code: ") + e.what());
		// Send this exception to Master Node.
		std::string exp(e.what());
		gossip(k, CommandType::exception, { {"txid", txid },{"e", exp } });
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

		logDebug("*Executing compute node command");

		std::unordered_map<std::string, TransactionResult>::iterator itr;
		{
			std::lock_guard<std::mutex> lock(txMutex);
			itr = trs.find(transactionId);
			if (itr == trs.end()) {
				return;
			}
		}
		// For given transacton if state change command exists then update storage node
		if (itr->second.get_stateChangeCmds().size() > 0) {
			std::string data = "txid:" + transactionId + "|" + itr->second.get_stateChangeCmds();
			gossip(storageNodeChannelId, data);
		}
	}
	catch (std::exception e) {
		logError(e.what());
	}
}

/*
	This method will update document data into documentmap
*/
void updateDocumentMap(FieldValueMap& theFields) {
	try {
		std::string& txid = theFields["txid"];
		std::string& data = theFields["data"];
		logDebug("Received data from Storage node: " + data);
		// update document data in documentmap
		dds[txid] = std::move(data);

		cv.notify_one();
	}
	catch (std::exception e) {
		logError(e.what());
	}
}

void JsProcessor::fetchDocument(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	try {
		logDebug("Logged:", "i called from js");
		if (args.Length() < 0) {
			return;
		}
		Isolate* isolate = args.GetIsolate();
		HandleScope scope(isolate);
		Local<Value> arg = args[0];
		String::Utf8Value value(isolate, arg);
		std::string input(*value);
		logDebug("Logged: js input : ", input);
		std::string txid = JsProcessor::generateUUID();
		std::string data = "txid:" + txid + "|" + input;

		// retrieve document from storageNode
		gossip(rds.begin()->second.getStorageNode(), data);

		// Wait until receive notification
		std::unique_lock<std::mutex> lk(m);
		cv.wait(lk, [txid]() {return (dds.find(txid) != dds.end()); });
		std::string result = dds[txid];
		Local<String> retval = String::NewFromUtf8(isolate, result.c_str());
		args.GetReturnValue().Set(retval);
		// erase data from local memory after processing the input
		dds.erase(txid);

		lk.unlock();
	}
	catch (std::exception e) {
		logError(e.what());
	}
}

/*
	generate hashCode
*/
std::string hashCode(std::string& data) {
	try {
		dht::InfoHash infoHash;
		return infoHash.get(data).toString();
	}
	catch (const std::exception e) {
		logError(e.what());
	}
}

/*
	share node information to masternodes
*/
void sendConnectionRequest(int port) {
	try {
		FieldValueMap theFields;
		char cmdstr[5];
		sprintf(cmdstr, "%d", static_cast<uint8_t>(BlockChainNode::NodeType::verifierNode));
		theFields.insert(FieldValueMap::value_type(std::string("t"), std::string(cmdstr)));

		std::string ip(getPublicIP());
		boost::trim(ip);

		// insert verifierNode uri
		theFields.insert(FieldValueMap::value_type(std::string("u"), std::string("ws://" + ip + std::string(":") + std::to_string(port))));

		// share verifier information to all master nodes
		gossip("masternodes", CommandType::connectionRequest, theFields);
	}
	catch (const std::exception e) {
		logError(e.what());
	}
}

int main(int argc, char* argv[]) {
	logInfo("Starting Compute Node");
	//js engine initialization
	v8::V8::InitializeICUDefaultLocation(argv[0]);
	v8::V8::InitializeExternalStartupData(argv[0]);
	std::unique_ptr<v8::Platform> platform = v8::platform::NewDefaultPlatform();
	v8::V8::InitializePlatform(platform.get());
	v8::V8::Initialize();
	JsProcessor::loadCmdBuilderScript();
	try {

		//Initialize DHT Node and RUN
		nodeIdentity = initializeKeypair();
		command_params params;
		parseArgs(argc, argv, &params);
		dhtNode.run(params.port, *nodeIdentity, true, NETWORK_ID);
		if (not params.bootstrap.first.empty()) {
			dhtNode.bootstrap(params.bootstrap.first.c_str(), params.bootstrap.second.c_str());
			logInfo("Added bootstrap node " + params.bootstrap.first + ":" + params.bootstrap.second);
		}

		// Listen to channel for any new requests
		subscribeTo(dhtNode.getId().toString());
		logInfo("Node public key fingerprint " + dhtNode.getId().toString());

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
			}},
			{ CommandType::fetchDocument, [](FieldValueMap&& theFields) {
				updateDocumentMap(theFields);
			}},
			{ CommandType::stateChange, [](FieldValueMap&& theFields) {
				gossip("master21", CommandType::stateChangeResponse, { {"id", theFields["id"]}, {"txid", theFields["txid"]}, {"data", theFields["data"]} });
			}}
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
		COMM_PROTOCOL_INIT;
		// send connection request to masternodes
		sendConnectionRequest(params.wsPort);
		logInfo("Node is up and running on port %d ! Press CTRL+C to quit.", params.port);
		while (true) { std::this_thread::sleep_for(std::chrono::seconds(60)); }
	}
	catch (const std::exception e) {
		logError(e.what());
		return 1;
	}
	return 0;
}