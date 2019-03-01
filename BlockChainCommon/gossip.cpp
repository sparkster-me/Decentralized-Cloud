#include <gossip.h>
#include <windows.h>
extern "C" { FILE __iob_func[3] = { *stdin,*stdout,*stderr }; }

const auto hdl = LoadLibraryA("gossip.dll");
std::thread runnerThread;
bool initialized = false;
func_ptr_GoInt SetListenPortPtr = nullptr;
func_ptr_Void BeginPtr = nullptr;
func_ptr_GoInt SetHeartbeatMillisPtr = nullptr;
func_ptr_GoString SetListenIPPtr = nullptr;
func_ptr_GoString SetBootstrapNodePtr = nullptr;
func_ptr_GoInt SetMaxBroadcastBytesPtr = nullptr;
func_ptr_GoString BroadcastStringPtr = nullptr;
func_ptr_GoUint8 SetMulticastEnabledPtr = nullptr;
func_ptr_DataHandler SetDataCallbackPtr = nullptr;
func_ptr_GoString SetLogThresholdPtr = nullptr;
GoString toGoString(const std::string& str) {
	size_t n = str.length();
	char* c = new char[n + 1];
	strcpy(c, str.c_str());
	return { c, static_cast<ptrdiff_t>(str.length()) };
}
void SetListenPort(GoInt p0) {
	SetListenPortPtr(p0);
}

// Spawns a new thread to run the listener.
void Begin() {
	if (!initialized) {
		std::cerr << "The Begin function was called before initializeGossip." << std::endl;
		return;
	}
	runnerThread = std::thread([]() {BeginPtr(); });
	// We'll wait a bit so we don't start sending messages too early and can give the node time to spin up.
	std::this_thread::sleep_for(std::chrono::seconds(2));
}

void SetHeartbeatMillis(GoInt p0) {
	SetHeartbeatMillisPtr(p0);
}

void SetListenIP(GoString p0) {
	SetListenIPPtr(p0);
}

void SetBootstrapNode(GoString p0) {
	SetBootstrapNodePtr(p0);
}

void SetMaxBroadcastBytes(GoInt p0) {
	SetMaxBroadcastBytesPtr(p0);
}

void SetDataCallback(DataHandler handler) {
	SetDataCallbackPtr(handler);
}

void BroadcastString(GoString p0) {
	if (!initialized) {
		std::cerr << "The BroadcastString function was called before initializeGossip." << std::endl;
		return;
	}
	BroadcastStringPtr(p0);
}

void SetLogThreshold(const std::string& level) {
	if (level != "info" && level != "warn" && level != "all") {
		std::cerr << "Invalid log level " << level << ". It must be one of all, info or warn, with all being the most verbose and warn being the least verbose." << std::endl;
		return;
	}
	GoString g = toGoString(level);
	SetLogThresholdPtr(g);
	delete g.p;
}

void initializeGossip(DataHandler handler, GoInt listenPort, std::string bootstrapNode) {
	if (hdl) {
		SetListenPortPtr = reinterpret_cast<func_ptr_GoInt>(GetProcAddress(hdl, "SetListenPort"));
		BeginPtr = reinterpret_cast<func_ptr_Void>(GetProcAddress(hdl, "Begin"));
		SetHeartbeatMillisPtr = reinterpret_cast<func_ptr_GoInt>(GetProcAddress(hdl, "SetHeartbeatMillis"));
		SetListenIPPtr = reinterpret_cast<func_ptr_GoString>(GetProcAddress(hdl, "SetListenIP"));
		SetBootstrapNodePtr = reinterpret_cast<func_ptr_GoString>(GetProcAddress(hdl, "SetBootstrapNode"));
		SetMaxBroadcastBytesPtr = reinterpret_cast<func_ptr_GoInt>(GetProcAddress(hdl, "SetMaxBroadcastBytes"));
		SetDataCallbackPtr = reinterpret_cast<func_ptr_DataHandler>(GetProcAddress(hdl, "SetDataCallback"));
		BroadcastStringPtr = reinterpret_cast<func_ptr_GoString>(GetProcAddress(hdl, "BroadcastString"));
		SetMulticastEnabledPtr = reinterpret_cast<func_ptr_GoUint8>(GetProcAddress(hdl, "SetMulticastEnabled"));
		SetLogThresholdPtr = reinterpret_cast<func_ptr_GoString>(GetProcAddress(hdl, "SetLogThreshold"));
	} else
		throw std::runtime_error("Gossip.dll was not found.");
	GoString ip = toGoString("0.0.0.0");
	SetListenIP(ip);
	delete ip.p;
	SetListenPort(listenPort);
	SetMaxBroadcastBytes(MAX_MESSAGE_SIZE);
	SetHeartbeatMillis(250);
	if (!bootstrapNode.empty()) {
		GoString gBootstrapNode = toGoString(bootstrapNode);
		SetBootstrapNode(gBootstrapNode);
		delete gBootstrapNode.p;
	}
	SetMulticastEnabled(false);
	SetDataCallback(handler);
	initialized = true;
}

void SetMulticastEnabled(GoUint8 p0) {
	SetMulticastEnabledPtr(p0);
}