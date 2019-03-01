#include <common/transaction.h>

void to_json(json& j, const Transaction& tx) {
	j = json(tx.toJson());
}

void from_json(const json&j, Transaction& tx) {
	tx.fromJson(j);
}

Transaction::Transaction() { m_computeNode = false; }
Transaction::Transaction(const std::string& txId, const time_t& timestamp) {
	m_txId = txId;
	m_timestamp = timestamp;
	m_receivedFullTransaction  = m_computeNode = true;
}

Transaction::Transaction(const std::string& txId, const std::string& hash, const time_t& timestamp) {
	m_txId = txId;
	m_lastValidHash = hash;
	m_timestamp = timestamp;
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

std::string Transaction::getFunctionId() const {
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
	// One of the conditions for verification is now satisfied.
	m_receivedOutput = true;
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

size_t Transaction::getBlockId() const {
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

void Transaction::setBlockId(const size_t& blockId) {
	m_blockId = blockId;
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

void Transaction::setFunctionId(const std::string& id) {
	m_code = id;
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
	m_receivedFullTransaction = true;
}

bool Transaction::receivedOutput() const {
	return m_receivedOutput;
}

bool Transaction::canSend() const {
	return !m_sentToClient && m_computeNode;
}

void Transaction::setSentToClient() {
	m_sentToClient = true;
}

bool Transaction::receivedFullTransaction() const {
	return m_receivedFullTransaction;
}

bool Transaction::isVerified() const {
	return m_totalVerified >= MAJORITY_THRESHOLD && m_receivedFullTransaction && m_receivedOutput;
}

bool Transaction::inBlock() const {
	return m_inBlock;
}

void Transaction::setInBlock() {
	m_inBlock = true;
}