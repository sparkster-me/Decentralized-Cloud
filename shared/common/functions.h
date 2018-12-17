#pragma once
#pragma warning (disable: 4996)



#include <future>
#include <chrono>
#include <boost/algorithm/string.hpp>
#include <boost/random.hpp>
#include <boost/random/random_device.hpp>
#include <boost/program_options.hpp>
#include <boost/asio.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <ctime>
#include <vector>
#include <opendht.h>
#include "types.h"
#define FIELD_DELIMITER "|"
#define PAIR_DELIMITER ":"
#define DHT_DEFAULT_PORT 4222
#define IPFS_DEFAULT_IP "localhost"
#define IPFS_DEFAULT_PORT 5001
#define NTP_SERVER "pool.ntp.org"
#define WS_DEFAULT_PORT 1033
#define NTP_SERVER "pool.ntp.org"
#define URL_PUBLIC_IP "myexternalip.com"
#define URL_PUBLIC_PORT  "80"
#define URL_PUBLIC_PATH  "/raw"
#include "ed25519/src/ed25519.h"
#include <opendht/base64.h>
#include <nlohmann/json.hpp>
using json = nlohmann::json;
using boost::asio::deadline_timer;
using boost::asio::ip::udp;

namespace opt = boost::program_options;
namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;
typedef boost::uniform_int<> NumberDistribution;
typedef boost::mt19937 RandomNumberGenerator;
typedef boost::variate_generator<RandomNumberGenerator&, NumberDistribution> Generator;
const extern time_t appStartupTime;
extern time_t currentTimestamp; // Represents the current epoch timestamp. When a node first starts, it will be set to appStartupTime.
const extern std::chrono::time_point<std::chrono::system_clock> appSystemStartupTime;
static std::mt19937_64 rd{ dht::crypto::random_device{}() };
static std::uniform_int_distribution<dht::Value::Id> rand_id;
struct command_params {
	bool help{ false };
	int port{ 0 };
	int wsPort{ 0 };
	std::pair<std::string, std::string> bootstrap{};
};

// given a string f1:v1|f2:v2|...|fn:vn, returns a hash map where f1...fn are the keys and v1...vn are the associated values.
FieldValueMap getFieldsAndValues(const std::string& inputString);
// Updates the epoch timestamp according to how long the node has been running. For example, if the node has been running for 5 seconds, the currentTimestamp will be the epoch at which this node started + 5. Returns the new timestamp.
time_t updateCurrentTimestamp();
// Builds a list of nodes
std::string buildSyncNodesInput();
// Fetches the current timestamp.
time_t getCurrentTimestamp();
// Gets the number of seconds past the last hour, based on currentTimestamp. Useful for actions that rely on a fixed point in every hour.
time_t getSecondsPastHour();
// Gets the number of seconds past the last hour in a given timestamp.
time_t getSecondsPastHour(time_t timestamp);
/* Generates a random number over the interval [a, b)
Taken from http://www.radmangames.com/programming/generating-random-numbers-in-c
*/
uint32_t generateRandomNumber(uint32_t a, uint32_t b);
/* Generates a string filled with n random letters from 'a' to 'z'. */
std::string getGUID(size_t length);
/* Returns the current epoch timestamp. */
time_t getEpochTimestamp();
/* Sends the specified message over the specified channel using the specified DhtRunner object. */
void gossip(dht::DhtRunner* dhtNode, const std::string& channelId, const std::string& msg);
/* Overload of previous function.
This function inserts cmd into the c field of the message, and builds a string from the rest of the
keys and values in the given map. The receivers will check the c field to extract the command.
The values are encoded before transmission to prevent clashing with the colon and vertical bar symbols used to delimit different parts of the message. */
void gossip(dht::DhtRunner* dhtNode, const std::string& channelId, CommandType cmd, const FieldValueMap& kv);
/* Listens on a specified channel. Will wait some time for the subscription. If the DhtRunner cannot
subscribe after the time has elapsed, this function will throw an exception.
This is a blocking call.
Returns a pointer to a SubscriptionAttributes struct that the caller can use to unsubscribe from a channel. */
SubscriptionAttributes* subscribeTo(dht::DhtRunner* dhtNode, const dht::InfoHash* hash);
/* Same as above, except that this is a convenient function that expects a string and will build an InfoHash struct from it. */
SubscriptionAttributes* subscribeTo(dht::DhtRunner* dhtNode, const std::string& channelId);
/* Detaches from the channel described by the given SubscriptionAttributes struct. This should be the same SubscriptionAttributes struct that was returned by one of the subscribeTo functions. */
void unsubscribeFrom(dht::DhtRunner* node, SubscriptionAttributes* channelInfo);
/* Retrieves a value with the specified key from the DHT. */
std::string dhtGet(dht::DhtRunner* node, const std::string key);
/* Retrieves a value from the DHT given the specified key. Calls the specified callback function with the retrieved value. */
void dhtGet(dht::DhtRunner* node, const std::string key, const std::function<void(std::string)>& func);
/* Post a value to dht with the given key*/
void dhtPut(dht::DhtRunner* dht, const std::string& key, const std::string& value);
void addNode(dht::DhtRunner* dhtNode, const std::string& key, const std::string& uri, BlockChainNode::NodeType type);
size_t getNumberOfMasterNodes();
size_t getNumberOfStorageNodes();
std::string& getStorageNodeKey(const size_t& n);
size_t getNumberOfVerifiers();
std::string& getVerifierNodeKey(const size_t& n);
// Creates a new identity and loads it when the node next boots.
dht::crypto::Identity* initializeKeypair();
// Used for command..function mapping. IE: given the specified command, the specified function will execute.
void registerDataHandlers(const CommandFunctionMap& handlers);
// Processes a string of data sent from either the web socket or OpenDHT. This function spawns a new thread so it doesn't block the event loop. The supplied timestamp value ends up in the fields mapping as tm,
// and is obtained from msg.date in the case of OpenDHT, or supplied by WebSocketServer using getCurrentTimestamp().
bool processIncomingData(dht::DhtRunner* dhtNode, const std::string& data, const std::string& key, time_t timestamp, bool fromWebSocket);
// Standard command parser.
void parseArgs(int argc, char **argv, command_params* params, boost::optional<opt::options_description&> additionalOptions = boost::none);
std::string getStorageNodeURL(const std::string& key);
// Generates a public and private keypair used to represent a client account. All values returned by this function are base-64 encoded.
json generateAccount();
// Generates a signature in base64 given the message, and the base64 public and private keys.
std::string createEd25519Signature(std::string& message, std::string& base64publicKey, std::string& base64privateKey);
// Returns true if the given message was signed using the given base64 public key and base64 signature.
bool verifyEd25519Signature(const std::string& message, const std::string& base64key, const std::string& base64signature);
// Signs a message given the node's public and private keypair. Returns a base-64 encoded signature.
std::string createX509Signature(const std::string& message, const dht::crypto::Identity* keypair);
// Verifies a signature that was generated using a node's public key
bool verifyX509Signature(const std::string& message, const std::string& base64signature, dht::crypto::PublicKey& publicKey);
std::string buildSyncNodesInput();
// Get Public IP of a system
std::string getPublicIP();
//sync nodes data
void syncNodes(std::string& data);
NodesMap* getMasterNodes();
NodesMap* getVerifierNodes();
NodesMap* getStorageNodes();
std::vector<std::string> getVerifierKeys();
std::vector<std::string> getStorageKeys();
// These functions are used to add data to the list of verifier keys and storage keys.
void addVerifierKey(const std::string& key);
void addStorageKey(const std::string& key);
// Creates a client account and displays it to stdout. Warning: this function displays the private key and seed used to generate the account.
void createAccount();