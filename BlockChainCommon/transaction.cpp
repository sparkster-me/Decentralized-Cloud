#include <common/transaction.h>

void to_json(json& j, const Transaction& tx) {
	j = json(tx.toJson());
}

void from_json(const json&j, Transaction& tx) {
	tx.fromJson(j);
}

Transaction::Transaction() { m_computeNode = false; }
Transaction::Transaction(std::string txId, time_t timestamp, bool computeNode) {
	m_txId = txId;
	m_timestamp = timestamp;
	m_computeNode = computeNode;
}

Transaction::Transaction(std::string txId, std::string hash, std::string output) {
	m_txId = txId;
	m_lastValidHash = hash;
	m_output = output;
}

time_t Transaction::getTimeStamp() const {
	return m_timestamp;
}

std::string Transaction::getTxId() const {
	return m_txId;
}

std::string Transaction::getTenantId() const {
	return m_tenantId;
}

std::string Transaction::getCode() const {
	return m_code;
}

std::string Transaction::getComputeNodeId() const {
	return m_computeNodeId;
}

std::string Transaction::getPublicKey() const {
	return m_publicKey;
}

std::string Transaction::getOutput() const {
	return m_output;
}

std::string Transaction::getInput() const {
	return m_input;
}

std::string Transaction::getException() const {
	return m_exception;
}

void Transaction::setOutput(const std::string& output) {
	m_output = output;
}

void Transaction::setInput(const std::string& input) {
	m_input = input;
}

void Transaction::setException(const std::string& exception) {
	m_exception = exception;
}

int Transaction::getTotalVerified() const {
	return m_totalVerified;
}

long Transaction::getBlockId() const {
	return m_blockId;
}

bool Transaction::isComputeNode() const {
	return m_computeNode;
}

void Transaction::incrementTotalVerified() {
	m_totalVerified++;
}

bool Transaction::verifyHash(const std::string& hash) {
	if (m_lastValidHash.empty()) { // If this is the first verification we are getting back.
		m_lastValidHash = hash;
		incrementTotalVerified();
		return true;
	}
	if (hash == m_lastValidHash) {
		incrementTotalVerified();
		return true;
	}
	return false;
}

void Transaction::setBlockId(const long blockid) {
	m_blockId = blockid;
}

void Transaction::setTenantId(const std::string& id) {
	m_tenantId = id;
}

void Transaction::setComputeNodeId(const std::string& id) {
	m_computeNodeId = id;
}

void Transaction::setMasterNodeId(const std::string& id) {
	m_masterNodeId = id;
}

void Transaction::setCode(const std::string& code) {
	m_code = code;
}

void Transaction::setSignature(const std::string& signature) {
	m_signature = signature;
}

void Transaction::setPublicKey(const std::string& key) {
	m_publicKey = key;
}

json Transaction::toJson() const {
	json j = {
		{ "txid", m_txId },
		{ "t", m_timestamp },
		{ "h", m_lastValidHash },
		{ "mid", m_masterNodeId },
		{ "cid", m_computeNodeId },
		{ "tid", m_tenantId },
		{ "s", m_signature },
		{ "k", m_publicKey },
		{ "co", m_code },
		{"o", m_output},
		{"i", m_input},
		{"e", m_exception}
	};
	return json(j);
}

void Transaction::fromJson(const json& j) {
	std::cout << j.dump() << std::endl;
	m_txId = j["txid"].get<std::string>();
	m_timestamp = j["t"].get<time_t>();
	m_lastValidHash = j["h"].get<std::string>();
	m_masterNodeId = j["mid"].get<std::string>();
	m_computeNodeId = j["cid"].get<std::string>();
	m_tenantId = j["tid"].get<std::string>();
	m_signature = j["s"].get<std::string>();
	m_publicKey = j["k"].get<std::string>();
	m_code = j["co"].get<std::string>();
	m_output = j["o"].get<std::string>();
	m_input = j["i"].get<std::string>();
	m_exception = j["e"].get<std::string>();
}