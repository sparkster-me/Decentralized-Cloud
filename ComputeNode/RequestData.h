#pragma once
#include <string>

class RequestData {
public:
	RequestData() {}
	RequestData(std::string txId, std::string timestamp) {
		this->txId = txId;
		this->timestamp = timestamp;
	};

	RequestData(std::string txId, std::string timestamp, std::string i) {
		this->txId = txId;
		this->timestamp = timestamp;
		this->input = i;
	};

	RequestData(std::string reqId, std::string txId, std::string timestamp, std::string i, std::string tid, std::string hash, std::string k, std::string nodeType, std::string sn) {
		this->reqId = reqId;
		this->txId = txId;
		this->timestamp = timestamp;
		this->input = i;
		this->tenantId = tid;
		this->hash = hash;
		this->channelId = k;
		this->nodeType = nodeType;
		this->storageNode = sn;
	};

	std::string getTimeStamp() const { return timestamp; }
	std::string getTxId() const { return txId; }
	std::string getReqId() const { return reqId; }
	std::string getInput() const { return input; };
	std::string getTenantId() const { return tenantId; };
	std::string getHash() const { return hash; };
	std::string getChannelId() const { return channelId; };
	std::string getNodeType() const { return nodeType; };
	std::string getStorageNode() const { return storageNode; };

	void setInput(std::string i) { input = i; }
	std::string setTimeStamp(std::string tm) { timestamp = tm; };
	std::string setTxId(std::string txId) { txId = txId; };
	std::string setReqId(std::string rId) { reqId = rId; };
	std::string setTenantId(std::string tid) { tenantId = tid; };
	std::string setHash(std::string h) { hash = h; };
	std::string setChannelId(std::string k) { channelId = k; };
	std::string setNodeType(std::string ntype) { nodeType = ntype; };
	std::string setStorageNode(std::string sn) { storageNode = sn; };

private:
	std::string reqId;
	std::string timestamp;
	std::string txId;
	std::string input;
	std::string tenantId;
	std::string hash;
	std::string channelId;
	std::string nodeType;
	std::string storageNode;
};


