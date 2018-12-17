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

	BlockChainNode(std::string publicKey, std::string uri, NodeType nodeType);
	std::string getPublicKey() const;
	std::string getURI() const;
	NodeType getType() const;
	std::string toJSONString();
	BlockChainNode toBlockChainNode(const json&j, BlockChainNode& node);
private:
	std::string m_publicKey;
	std::string m_uri;
	NodeType m_type;
	
};