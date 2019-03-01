#pragma once
#pragma warning (disable: 4996)
#include <future>
#include <chrono>
#include <boost/algorithm/string.hpp>
#include <boost/log/expressions.hpp> // For filter expressions so we can filter by log severity.
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp> // For boost_trivial macros
#include <boost/log/utility/setup/file.hpp> // So we get add_file helper function
#include <boost/log/utility/setup/console.hpp> // So we get add_console_log helper function
#include <boost/log/utility/setup/common_attributes.hpp> // So we get commonAttributes helper function
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
#include "ed25519/src/ed25519.h"
#include <opendht/base64.h>
#include <nlohmann/json.hpp>
#include <gossip.h>
#include <csignal> // For signal handling
#include <common/commonDefines.h>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <websocketpp/frame.hpp>

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
const extern time_t appSystemStartupTime;
extern dht::DhtRunner dhtNode;
extern std::mutex nodesSyncMutex;
static std::mt19937_64 rd{ dht::crypto::random_device{}() };
static std::uniform_int_distribution<dht::Value::Id> rand_id;
struct command_params {
	bool help{ false };
	int port{ 0 };
	int wsPort{ 0 };
	int peerPort{ 0 };
	std::pair<std::string, std::string> bootstrap{};
	std::pair<std::string, std::string> peer{};
	std::string logLevel = "trace";
	std::string logFile = "log.log";
	bool enableLogging{ true };
};
/* Represents a subscription to a DHT channel.
Channel is the InfoHash struct used to attach to a channel, and token is the ID returned when subscribing.
*/
struct SubscriptionAttributes {
	dht::InfoHash channel;
	std::shared_future<size_t> token;
	SubscriptionAttributes& operator = (const SubscriptionAttributes& other) {
		if (this != &other) {
			channel = other.channel;
			token = other.token;
		}
		return *this;
	}
};
typedef websocketpp::server<websocketpp::config::asio> server;

using websocketpp::connection_hdl;
using websocketpp::lib::bind;

struct ConnectionData {
	connection_hdl hdl;
};

class WebSocketServer {
public:
	typedef std::unordered_map<std::string, ConnectionData> ConnectionsMap;
	WebSocketServer();
	void onOpen(connection_hdl hdl);
	void onClose(connection_hdl hdl);
	void onMessage(connection_hdl hdl, server::message_ptr msg); // Will process an incoming command; will add a new connection to the connection pool, and send the payload to processIncomingData
	ConnectionData& getDataFromKey(const std::string& key);
	std::string getKeyFromHdl(const connection_hdl hdl) const;
	void run(uint16_t port);
	bool exists(const std::string& key) const;
	void addConnection(const std::string& key, const connection_hdl hdl);
	void send(const connection_hdl hdl, const std::string & msg);
	void send(const std::string& key, const std::string& msg);
	void shutdown();

private:
	void sendThreaded(const connection_hdl hdl, const std::string & msg);
	server m_server;
	ConnectionsMap m_connections;
	std::atomic<bool> running; // Used to control whether or not to send actual messages over the socket. In the case of tests, we spawn a socket server but we don't intend to use it.
};

// given a string f1:v1|f2:v2|...|fn:vn, returns a hash map where f1...fn are the keys and v1...vn are the associated values.
FieldValueMap getFieldsAndValues(const std::string& inputString);
// Builds a list of nodes
std::string buildSyncNodesInput();
// Updates the epoch timestamp according to how long the node has been running, and returns the new timestamp. For example, if the node has been running for 5 seconds, the currentTimestamp will be the epoch at which this node started + 5. Returns the new timestamp.
// Guarantee: the timestamp will ever only be updated by calling this function. Other time functions listed in this header will not update the timestamp, and will use the last value returned by this function.
time_t getCurrentTimestamp();
// Gets the number of seconds past the last hour, based on currentTimestamp. Useful for actions that rely on a fixed point in every hour.
// This function will NOT update the timestamp and assumes that the current timestamp has been updated by a previous call to getCurrentTimestamp(). This is so we can optimize the number of calls to update the timestamp.
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
// Helper function to send the specified message over the specified channel using the specified dhtRunner object. Retries the message on failure. Tries is the number of attempts made to send the message so far. Zero-based.
// This function should never be used by code except for the gossip function. Gossip calls this function to send messages to dht.
void sendMessageWithRetry(const dht::Value::Id& id, const std::string& channelId, std::string&& msg, const time_t& timestamp, const int& tries);
// Sends the specified message over the specified channel using the specified DhtRunner object.
// We use a copy operation for msg so we can freely move it around inside this function to fine-tune access speed.
// If isMayday is true, this message is sent "as-is".
void gossip(std::string channelId, std::string msg, bool isMayday = false);
/* Overload of previous function.
This function inserts cmd into the c field of the message, and builds a string from the rest of the
keys and values in the given map. The receivers will check the c field to extract the command.
The values are encoded before transmission to prevent clashing with the colon and vertical bar symbols used to delimit different parts of the message. */
void gossip(const std::string& channelId, CommandType cmd, const FieldValueMap& kv);
/* Listens on a specified channel. Will wait some time for the subscription. If the DhtRunner cannot
subscribe after the time has elapsed, this function will throw an exception.
This is a blocking call.
Returns a pointer to a SubscriptionAttributes struct that the caller can use to unsubscribe from a channel.
If OPENDHT is not the communication protocol, the dhtNode parameter is ignored. */
SUB_TYPE subscribeTo(dht::InfoHash&& hash);
/* Same as above, except that this is a convenient function that expects a string and will build an InfoHash struct from it.
If OPENDHT is not the communication protocol, the InfoHash struct is never built. See the function definition for details. */
SUB_TYPE subscribeTo(const std::string& channelId);
/* Detaches from the channel described by the given SubscriptionAttributes struct. This should be the same SubscriptionAttributes struct that was returned by one of the subscribeTo functions. */
void unsubscribeFrom(const SUB_TYPE& channelInfo);
// Processes a piece of data.
// Replies to mayday calls are also processed in this function. If this node receives a Mayday call, it will respond by sending the original packet over gossip in the hopes that it will reach whoever needs it.
void dataHandler(std::string msg, const std::string originator="");
// The Smudge library will make a callback to this function when it receives data.
#if COMM_PROTOCOL == PROTOCOL_SMUDGE
void smudgeDataHandler(const char* payload);
#endif
/* Retrieves a value with the specified key from the DHT. */
std::string dhtGet(const std::string& key);
/* Retrieves a value from the DHT given the specified key. Calls the specified callback function with the retrieved value. */
void dhtGet(const std::string& key, const std::function<void(std::string)>& func);
/* Post a value to dht with the given key*/
void dhtPut(const std::string& key, const std::string& value, bool permanent=false);
void addNode(const std::string& key, const std::string& uri, BlockChainNode::NodeType type);
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
// Because the invokation of this function is critical to how fast the message is processed, we use
// move parameters here to maximize speed in data access.
bool processIncomingData(std::string&& data, std::string&& key, time_t&& timestamp, bool&& fromWebSocket,WebSocketServer* wsServer=nullptr);
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
NodesMap& getMasterNodes();
NodesMap& getVerifierNodes();
NodesMap& getStorageNodes();
std::vector<std::string> getVerifierKeys();
std::vector<std::string> getStorageKeys();
// These functions are used to add data to the list of verifier keys and storage keys.
void addVerifierKey(const std::string& key);
void addStorageKey(const std::string& key);
// Creates a client account and displays it to stdout. Warning: this function displays the private key and seed used to generate the account.
void createAccount();
// Sets up Smudge using the specified data callback, port and bootstrap node.
// This function also starts the Mayday thread. This thread will monitor packets and send out a Mayday call for any segments it determines failed to arrive.
// The mayday calls are sent using backoff until the packet is eventually removed from memory.
// Mayday calls are constructed in the form mayUID|missingSegment1,missingSegment2, ...
#if COMM_PROTOCOL == PROTOCOL_SMUDGE
void startSmudge(DataHandler handler, int port, std::string bootstrap = "");
#endif
// The following two functions were adapted from https://stackoverflow.com/questions/27529570/simple-zlib-c-string-compression-and-decompression
// We've modified them to include a header that contains the positions of the null terminators from the compressed string in contrast with the original implementation that base-64 encoded the string.
// This approach keeps the size down versus base-64 encoding the string, which ultimately leads to losing the benefits of the reduced packet size due to the large data size of base-64.
// Here, we use a copy parameter because we call removeNullTerminators internally which modifies the original string, so we would have had to make a copy if we didn't already have a stack-allocated string.
std::string compressString(std::string data);
std::string decompressString(const std::string &data);
// Removes all null terminators that are embedded in the string.
void removeNullTerminators(std::string& str);
// Breaks up a packet into pieces suitable for sending over UDP. The nonce uniquely identifies this packet.
// Returns std::vector<string>
std::vector<std::string> fragmentPacket(const size_t& nonce, const std::string& packet);
std::string base64_encodeString(std::string message);
// Executes the common shutdown handler. Here, we clean up detached threads and shut down OpenDHT.
// The handler is registered using COMM_INIT
void registerGlobalShutdownHandler();
// Registers a handler for SIGINT. This handler will call exit(SIGINT), activating the exit handlers.
// The SIGINT handler is register once with no re-registration. This way, if the user wishes to
// force-terminate the program during exit handler execution, they can do so.
// This is not recommended though because we do some clean up in the exit handlers and prematurely terminating the handlers could cause resources to not be released.
// Registered using COMM_INIT
void registerSigint();
// These functions are for logging. They are ordered in descending priority. For example, trace will includes trace and all other log levels, while fatal only includes fatal log level.
void logTrace(const std::string message,...);
void logDebug(const std::string message,...);
void logInfo(const std::string message,...);
void logWarning(const std::string message,...);
void logError(const std::string message,...);
void logFatal(const std::string message,...);
// Sets the logging level. if not called or called with an invalid logging level, the log level is set to trace and info respectively.
// If not called, the log file is set to log.log.
// This function also sets up console logging. All info-severity messages will always be printed to the console as well as output to the log file (if appropriate in the latter case)
void setLoggingLevel(const std::string& level, const std::string& logFile, const bool& enableLog);