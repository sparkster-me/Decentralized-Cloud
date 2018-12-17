#include <common/blockChainNode.h>
BlockChainNode::BlockChainNode(std::string publicKey, std::string uri, NodeType nodeType) {
	m_publicKey = publicKey;
	m_uri = uri;
	m_type = nodeType;
}

std::string BlockChainNode::getPublicKey() const {
	return m_publicKey;
}

std::string BlockChainNode::getURI() const {
	return m_uri;
}

BlockChainNode::NodeType BlockChainNode::getType() const {
	return m_type;
}

std::string BlockChainNode::toJSONString() {
	json _json;
	_json["k"] = this->m_publicKey;
	_json["u"] = this->m_uri;
	_json["t"] = static_cast<int>(this->m_type);
	
	return _json.dump();
}

BlockChainNode BlockChainNode::toBlockChainNode(const json&j, BlockChainNode& node) {
	node.m_publicKey = j["k"].get<std::string>();
	node.m_uri= j["u"].get<std::string>();
	node.m_type= j["t"].get<NodeType>();
	return node;
}