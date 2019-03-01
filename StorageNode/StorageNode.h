#pragma once

#include <boost/algorithm/string.hpp>
#include <boost/optional/optional.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <stack>
#include <ipfs/client.h>
#include <opendht/base64.h>
#include "Block.h"
#include "Command.h"
#include <common/functions.h>

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
#define MSG_DOCUMENT_NOT_FOUND "No document found for the given id"
#define MSG_BLOCK_NOT_FOUND "No Block found for the given Id"
#define MSG_DELETE_SUCCESS "Deleted successfully"
#define MSG_DOCUMENT_UPDATE "Documents are updated successfully"
#define MSG_DOCUMENT_DELETE "Documents are deleted successfully"
#define MSG_INVALID_OBJECT_ID "Invalid Object Id"
#define MSG_INVALID_DOCUMENT_STRUCTURE "Issue while parsing document body. Please ensure to pass the document in proper json format"
using json = nlohmann::json;
typedef std::unordered_map<std::string, std::string> UnorderedEdgeMap;
typedef std::map<std::string, std::string> OrderedEdgeMap;
typedef std::unordered_map<std::string, bool> ExistingIndexesMap;
typedef unsigned int uint32;
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



//typedef std::priority_queue<std::pair<std::string, std::pair<std::pair<FieldValueMap, std::vector<std::string>>, DoubleLong>>, std::vector<std::pair<std::string, std::pair<std::pair<FieldValueMap, std::vector<std::string>>, DoubleLong>> >, OrderByTimeStamp> OrderedPriorityQueue;
struct OrderByBlockNumber {
	bool operator()(std::pair<DoubleLong, std::string> a, std::pair<DoubleLong, std::string>  b) {
		return a.first > b.first;
	}
};
typedef std::priority_queue <std::pair<DoubleLong, std::string>, std::vector<std::pair<DoubleLong, std::string>>, OrderByBlockNumber> PriorityPairQueue;

// Used to queue the blocks received from the masternodes
PriorityPairQueue blocksToCommit;
// Queues all state change transactions based on the command and tenant
UnorderedQueueMap stateChangeQueueMap;

WebSocketServer wsServer;

//Used to hold IPFS connection
ipfs::Client* ipfsClient = nullptr;

std::pair<std::string, long>* ipfsAddress;

dht::crypto::Identity* nodeIdentity = nullptr;

FILE* ipfsService=nullptr;

std::mutex stateUpdateMutex;

std::mutex blockCommitMutex;
// Returns array of documents based on the given filter expression and indices
json evaluateExpression(ipfs::Client& client, TreeNode* filter, UnorderedEdgeMap& indexes, json& _docs);
// Fetches object from IPFS for the given hash and returns JSON array 
json fetchDocument(ipfs::Client& client, std::string& hash);
// Send off message to the client (can be websocket client or DHT)
void send(const std::string&& source, const std::string &channelId, CommandType cmd, const std::string& txid, const std::string& objectId, const std::string val);
// Returns Data string of object for the given hash and ipfs client
std::string fetchObject(ipfs::Client& client, const std::string &hash);
// Adds DAG object to IPFS
std::string addObject(ipfs::Client& client, const std::string& data);
// Sends connection request command to both storagenodes and mastrnodes
//void sendConnectionRequest(int& port);
void sendConnectionRequest(int& port, std::string& ip);
// Executes state change commands
void executeStateChangeCommand(ipfs::Client& client, std::vector<std::string>&& command, std::string& source, std::string & channelId, std::string & txid, std::string & tid);
// finds the document in the given entity based on string expression
void findDocument(std::string&& source, std::string&& channelId, std::string&& txid, std::string&& tid, std::string&& entityId, std::string&& expression);
// Returns matching documents as per the expression
json searchDocument(ipfs::Client& client, std::string & tid, std::string & entityId, std::string && expression);
// Adds and object to the IPFS DAG
void addChildNode(ipfs::Client& client, std::string& source, const std::string& channelId, std::string& txid, std::string& tid, const std::string& tree, std::string&& object);
// Creates or deletes a document in the given entity
void createOrDeleteDocument(ipfs::Client& client, std::string& source, std::string& channelId, std::string& txid, std::string& tid, std::string& entityId, std::string& documentId, std::string&& documentJson, std::string op = "");
// Updates or deletes a document in the given entity
void updateOrDeleteDocument(ipfs::Client& client, std::string& source, std::string& channelId, std::string& txid, std::string& tid, std::string& entityId, std::string& documentId, std::string&& documentJson, std::string op);
// Differentiates state change commands and pushes the transaction in to the queue
void onStateChange(FieldValueMap&& theFields);
// Links object DAG Node to the root Node. If root node not available then creates and links the child
void linkToRoot(ipfs::Client& client, const std::string& tree, std::string &objectId, std::string &objectHash, const std::string& txid);
// Fecth function definition from the IPFS for the given function id or hash
void fetchFunction(const std::string&& source, std::string&& channelId, std::string&& txid, std::string&& tid, std::string&& functionId, std::string&& hash);
// Returns template definition to the client for the given templateId
void fetchTemplate(std::string&& source, std::string&& channelId, std::string&& txid, std::string&& tid, std::string&& templateId);
// process list of queued transactions. This function is invoked in a new thread.
void processStateUpdates(const std::string key);
// Updates child object DAG and re-link to the root
void updateChildNode(ipfs::Client& client, const std::string& source, const std::string& tree, const std::string& channelId, std::string& txid, std::string& tid, std::string& objectId, std::string&& object);
// Fetches all the values from new object and overrides the values in old object
std::string updateValues(std::string oldObject, std::string newObject);
// Returns new instance of IPFS Client
ipfs::Client* getIPFSClient();
// start ipfs service
void runIpfsDaemon();
//Register shutdown handler
void registerShutdownHandler();

