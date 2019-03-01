// Number of master nodes who are eligible to be block producers in a round.
#include <stdarg.h>
#define TOTAL_MASTER_NODES 3
#define MAJORITY_THRESHOLD TOTAL_MASTER_NODES*(2.0/3.0)
#define PROTOCOL_SMUDGE 0
#define PROTOCOL_OPENDHT 1
#define DEFAULT_SMUDGE_PORT 51100
#define MAX_MESSAGE_SIZE 999999999
#define COMM_PROTOCOL PROTOCOL_OPENDHT
#if COMM_PROTOCOL == PROTOCOL_SMUDGE
#define SUB_TYPE std::string
#define SUB_NONE ""
#define COMM_PROTOCOL_INIT logDebug("Set communication protocol to Smudge."); if(!params.peer.first.empty()) startSmudge(smudgeDataHandler, params.peerPort, params.peer.first + ":" + params.peer.second); else startSmudge(smudgeDataHandler, params.peerPort);registerGlobalShutdownHandler();registerSigint();
#elif COMM_PROTOCOL == PROTOCOL_OPENDHT
#define SUB_TYPE SubscriptionAttributes
#define SUB_NONE nullptr
#define COMM_PROTOCOL_INIT setLoggingLevel(params.logLevel, params.logFile,params.enableLogging);\
logDebug("Set communication protocol to OpenDHT.");\
registerGlobalShutdownHandler(); \
registerSigint(); \
dhtNode.registerType(dht::ValueType(6, "Short Duration", std::chrono::seconds(60)));\
dhtNode.registerType(dht::ValueType(8, "Short Duration", std::chrono::hours(1000)));
#endif
/*dhtNode.setLoggers([](char const *m, va_list args) {\
std::array<char, 8192> buffer;\
int ret = vsnprintf(buffer.data(), buffer.size(), m, args);\
if (ret < 0)\
	return ;\
std::string str(buffer.data());\
logError(std::string(buffer.data())); }, [](char const *m, va_list args) {\
	std::array<char, 8192> buffer;\
	int ret = vsnprintf(buffer.data(), buffer.size(), m, args);\
	if (ret < 0)\
		return ;\
	std::string str(buffer.data());\
	logWarning(std::string(buffer.data())); }, [](char const *m, va_list args) {\
		std::array<char, 8192> buffer;\
		int ret = vsnprintf(buffer.data(), buffer.size(), m, args);\
		if (ret < 0)\
			return ;\
		std::string str(buffer.data());\
		logDebug(std::string(buffer.data())); });\
*/
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
#define NETWORK_ID 1111