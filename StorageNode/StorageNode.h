#pragma once

#include <boost/algorithm/string.hpp>
#include <boost/optional/optional.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "Block.h"
#include <queue>
#include <stack>
#include <common/webSocketServer.h>
#include <common/types.h>
#include <ipfs/client.h>
#include <opendht/base64.h>

#define FUNCTION_TREE "function"
#define DATA_TREE "data"
#define TEMPLATE_TREE "template"
#define TRANSACTION_TREE "tx"
#define CHARACTER_DELIMITER "_"
#define ROOT_IDENTIFIER "."
#define BLOCK_TREE "block"
#define MSG_FUNCTION_NOT_FOUND "No function found for the given Id"
#define MSG_TEMPLATE_NOT_FOUND "No Entity found for the given Id"
#define MSG_TRANSACTION_NOT_FOUND "No Transaction found for the given Id"
#define MSG_DOCUMENT_ROOT_NOT_FOUND "No Entity found for the given document to operate"
#define MSG_BLOCK_NOT_FOUND "No Block found for the given Id"
#define MSG_DELETE_SUCCESS "Deleted successfully"
#define MSG_INVALID_OBJECT_ID "Invalid Object Id"

using json = nlohmann::json;
typedef std::unordered_map<std::string, std::string> UnorderedEdgeMap;
typedef std::map<std::string, std::string> OrderedEdgeMap;
typedef std::unordered_map<std::string, bool> ExistingIndexesMap;
typedef unsigned int uint32;
typedef long long DoubleLong;
using namespace std;
using namespace dht;
// Values in this hash map are true given operators [a][b] if a has higher precedence than b.
// If a has equal or less precedence than b, the value in the map will be false.
std::unordered_map<std::string, std::unordered_map<std::string, bool>> precedence = {
	{ "=",{ { "&", true },{ "|", true } } },
	{ "<",{ { "&", true },{ "|", true } } },
	{ ">",{ { "&", true },{ "|", true } } },
	{ "<=",{ { "&", true },{ "|", true } } },
	{ ">=",{ { "&", true },{ "|", true } } },
	{ "<>",{ { "&", true },{ "|", true } } },
	{ "&",{ { "|", true } } },
	{ "|",{} }
};

struct TreeNode {
	std::string root;
	TreeNode* left;
	TreeNode* right;
	ExistingIndexesMap* fields;
};

struct Custom_Compare {
	Custom_Compare(std::string operand) {
		this->operand = operand;
	}
	std::string operand;

	bool operator()(json a, json b) {
		return (a[operand] < b[operand]);
	}
};

struct OrderByTimeStamp {
	constexpr bool operator()(std::pair<std::string, std::pair<std::pair<FieldValueMap, std::vector<std::string>>, DoubleLong>> const & a,
		std::pair<std::string, std::pair<std::pair<FieldValueMap, std::vector<std::string>>, DoubleLong>> const & b) const noexcept	{
		return a.second.second > b.second.second;
	}
};

typedef std::priority_queue<std::pair<std::string, std::pair<std::pair<FieldValueMap,std::vector<std::string>>, DoubleLong>>, std::vector<std::pair<std::string, std::pair<std::pair<FieldValueMap, std::vector<std::string>>, DoubleLong>> >, OrderByTimeStamp> OrderedPriorityQueue;

typedef std::priority_queue <std::pair<DoubleLong, std::string>, std::vector<std::pair<DoubleLong, std::string>>> PriorityPairQueue;

typedef std::unordered_map<std::string, OrderedPriorityQueue*> UnorderedQueueMap;

// Used to queue the blocks received from the masternodes
PriorityPairQueue blocksToCommit;
// Queues all state change transactions based on the command and tenant
UnorderedQueueMap stateChangeQueueMap;
// Used to hold websocket server connection
WebSocketServer* wsServer = nullptr;
// Used to hold dht connection
dht::DhtRunner* dhtNode = nullptr;
//Used to hold IPFS connection
ipfs::Client* ipfsClient = nullptr;

std::pair<std::string, long>* ipfsAddress;

dht::crypto::Identity* nodeIdentity = nullptr;

std::mutex stateUpdateMutex;

std::mutex blockCommitMutex;
// Returns array of documents based on the given filter expression and indexes
json evaluateExpression(ipfs::Client& client,TreeNode* filter, UnorderedEdgeMap& indexes, json& _docs);
// Fetches object from IPFS for the given hash and returns JSON array 
json fetchDocument(std::string& hash);
// Send off message to the client (can be websocket client or DHT)
void send(const std::string&& source, const std::string &channelId, CommandType cmd,const std::string& txid,const std::string& objectId,const std::string val);
// Returns Data string of object for the given hash
std::string fetchObject(const std::string &hash);
// Returns Data string of object for the given hash and ipfs client
std::string fetchObject(ipfs::Client& client, const std::string &hash);
// Adds DAG object to IPFS
std::string addObject(ipfs::Client& client,const std::string& data);
// Sends connection request command to both storagenodes and mastrnodes
void sendConnectionRequest(int& port);
// Executes state change commands
void executeStateChangeCommand(ipfs::Client& client,std::vector<std::string>&& command,std::string& source, std::string & channelId, std::string & txid, std::string & tid);
// finds the document in the given entity based on string expression
void findDocument(std::string&& source,std::string&& channelId, std::string&& txid, std::string&& tid, std::string&& entityId, std::string&& expression);
// Returns matching documents as per the expression
json searchDocument(ipfs::Client& client, std::string & tid, std::string & entityId, std::string && expression);
// Adds and object to the IPFS DAG
void addChildNode(ipfs::Client& client, std::string& source, const std::string& channelId, std::string& txid, std::string& tid, const std::string& tree, std::string& objectId, std::string&& object);
// Creates or deletes a document in the given entity
void createOrDeleteDocument(ipfs::Client& client, std::string& source, std::string& channelId, std::string& txid, std::string& tid, std::string& entityId, std::string& documentId, std::string&& documentJson, std::string op = "");
// Updates or deletes a document in the given entity
void updateOrDeleteDocument(ipfs::Client& client, std::string& source, std::string& channelId, std::string& txid, std::string& tid, std::string& entityId, std::string& documentId, std::string&& documentJson, std::string op);
// Differentiates state change commands and pushes the transaction in to the queue
void onStateChange(FieldValueMap&& theFields);
// Links object DAG Node to the root Node. If root node not available then creates and links the child
void linkToRoot(ipfs::Client& client, const std::string& tree, std::string &objectId, std::string &objectHash, const std::string& txid);
// Fecth function definition from the IPFS for the given function id or hash
void fetchFunction(const std::string&& source,std::string&& channelId, std::string&& txid, std::string&& tid, std::string&& functionId,std::string&& hash);
// Returns template definition to the client for the given templateId
void fetchTemplate(std::string&& source, std::string&& channelId, std::string&& txid, std::string&& tid, std::string&& templateId);
// process list of queued transactions
void processStateUpdates(const std::string& key);
// Updates child object DAG and re-link to the root
void updateChildNode(ipfs::Client& client,const std::string& source, const std::string& tree, const std::string& channelId, std::string& txid, std::string& tid, std::string& objectId, std::string&& object);
// Fetches all the values from new object and overrides the values in old object
std::string updateValues(std::string oldObject, std::string newObject);

// Returns new instance of IPFS Client
ipfs::Client* getIPFSClient();


