#pragma once
#include <string>
#include <nlohmann/json.hpp>
using json = nlohmann::json;
class BlockChainNode {
public:
	enum class NodeType : char {
		masterNode,
		verifierNode,
		storageNode
	};

	BlockChainNode();
	BlockChainNode(const std::string& publicKey, const std::string& uri, const NodeType& nodeType);
	std::string getPublicKey() const;
	std::string getURI() const;
	NodeType getType() const;
	std::string toJSONString() const;
	static void toBlockChainNode(const json&j, BlockChainNode& node);
private:
	std::string m_publicKey;
	std::string m_uri;
	NodeType m_type;
};