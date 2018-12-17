#pragma once
#include <string>
#include<iostream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;
class Transaction {
public:
	Transaction();
	Transaction(std::string txId, std::string hash, std::string output);
	Transaction(std::string txId, time_t timestamp, bool computeNode);
	time_t getTimeStamp() const;
	std::string getTxId() const;
	std::string getTenantId() const;
	std::string getCode() const;
	std::string getComputeNodeId() const;
	int getTotalVerified() const;
	long getBlockId() const;
	std::string getPublicKey() const;
	std::string getOutput() const;
	std::string getInput() const;
	std::string getException() const;
	bool isComputeNode() const;
	void incrementTotalVerified();
	bool verifyHash(const std::string& hash);
	void setTenantId(const std::string& id);
	void setComputeNodeId(const std::string& id);
	void setMasterNodeId(const std::string& id);
	void setCode(const std::string& code);
	void setSignature(const std::string& signature);
	void setPublicKey(const std::string& key);
	void setOutput(const std::string& output);
	void setInput(const std::string& input);
	void setException(const std::string& exception);
	void setBlockId(const long blockid);
	json toJson() const;
	void fromJson(const json& j);

private:
	time_t m_timestamp;
	std::string m_txId;
	int m_totalVerified;
	long m_blockId;
	std::string m_lastValidHash;
	bool m_computeNode;
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