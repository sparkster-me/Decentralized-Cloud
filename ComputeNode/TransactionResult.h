#pragma once
#include <string>

class TransactionResult {
public:
	TransactionResult(std::string txId, std::string timestamp) {
		this->txId = txId;
		this->timestamp = timestamp;
	};

	TransactionResult(std::string txId, std::string timestamp, std::string res) {
		this->txId = txId;
		this->timestamp = timestamp;
		this->result = res;
	};

	std::string getTimeStamp() const { return timestamp; }
	std::string getTxId() const { return txId; }
	std::string getResult() const { return result; };
	void setResult(std::string rt) { result = rt; }
	void set_deterministicCode(std::string dc) { deterministicCode = dc; }
	void set_stateChangeCmds(std::string scc) { stateChangeCmds = scc; }

	std::string get_deterministicCode() { return deterministicCode; }
	std::string get_stateChangeCmds() { return stateChangeCmds; }

private:
	std::string timestamp;
	std::string txId;
	std::string result;
	std::string deterministicCode;
	std::string stateChangeCmds;
};


