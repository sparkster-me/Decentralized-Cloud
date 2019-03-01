#pragma once
#include <string>
#include<iostream>
#include < functional>
#include <nlohmann/json.hpp>
#include <common/commonDefines.h> // For MAJORITY_THRESHOLD
using json = nlohmann::json;
class Transaction {
public:
	Transaction();
	// Called by a Master Node that receives CommandType::verificationResult and hasn't received txNotification yet.
	Transaction(const std::string& txId, const std::string& hash, const time_t& timestamp);
	// Called by the originating Master Node. Sets computeNode to true since this is the originating Master Node.
	Transaction(const std::string& txId, const time_t& timestamp);
	time_t getTimeStamp() const;
	std::string getTxId() const;
	std::string getTenantId() const;
	std::string getFunctionId() const;
	std::string getComputeNodeId() const;
	int getTotalVerified() const;
	size_t getBlockId() const;
	std::string getPublicKey() const;
	std::string getOutput() const;
	std::string getInput() const;
	// True if this transaction contains the function output, false if the transaction is still waiting for the output.
	bool receivedOutput() const;
	// True if this transaction is ready to send to the client, false toherwise.
	// For this function to return true, this transaction must be verified, contain the client's key, and must contain the function output.
	bool canSend() const;
	std::string getException() const;
	bool isComputeNode() const;
	void incrementTotalVerified();
	bool verifyHash(const std::string& hash);
	void setTenantId(const std::string& id);
	void setComputeNodeId(const std::string& id);
	// Returns true if this transaction has already been sent to the client, false otherwise.
	void setSentToClient();
	void setMasterNodeId(const std::string& id);
	void setFunctionId(const std::string& id);
	void setSignature(const std::string& signature);
	void setPublicKey(const std::string& key);
	void setOutput(const std::string& output);
	void setInput(const std::string& input);
	void setException(const std::string& exception);
	void setBlockId(const size_t& blockId);
	// Returns true if this transaction has been filled in with the txNotification command, false otherwise.
	bool receivedFullTransaction() const;
	json toJson() const;
	void fromJson(const json& j);
	// Returns true if this transaction is a complete transaction and has been verified, false otherwise.
	bool isVerified() const;
	// Returns true if this transaction was already added to a block, false otherwise.
	bool inBlock() const;
	void setInBlock();

private:
	bool m_sentToClient = false;
	bool m_receivedOutput = false;
	bool m_receivedFullTransaction = false;
	int m_totalVerified = 0;
	std::string m_lastValidHash;
	bool m_inBlock = false;
	time_t m_timestamp = 0;
	std::string m_txId;
	size_t m_blockId = 0;
	bool m_computeNode = false;
	std::string m_tenantId;
	std::string m_masterNodeId;
	std::string m_computeNodeId;
	std::string m_code;
	std::string m_signature;
	std::string m_publicKey;
	std::string m_output;
	std::string m_input;
	std::string m_exception;
};

// Used to convert a Transaction to JSON implicitly.
void to_json(json& j, const Transaction& tx);
// Used to convert a json object into a Transaction implicitly.
void from_json(const json&j, Transaction& tx);