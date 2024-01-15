#include <BLE2902.h>
#include <BLECharacteristic.h>
#include <BLEDevice.h>

namespace elm327 {

const uint16_t SERVICE_UUID = 0xFFF0;
const uint16_t RX_UUID = 0xFFF1;
const uint16_t TX_UUID = 0xFFF2;

#define DEBUG_LEVEL 1

class Device final : public BLEServerCallbacks {
public:
  ~Device() noexcept override = default;

  static Device& getInstance() noexcept {
    static Device instance;
    return instance;
  }

  void setATH1Active(bool active) {
    ath1Active = active;
  }

  bool isConnected() const noexcept {
    return server->getConnectedCount() > 0;
  }

  bool start(BLECharacteristicCallbacks* callbacks) noexcept {
    BLEDevice::init("Carista");
    BLEDevice::setPower(ESP_PWR_LVL_P9);

    server = BLEDevice::createServer();
    server->setCallbacks(this);

    service = server->createService(BLEUUID(SERVICE_UUID));

    rxCharacteristic = createCharacteristic(RX_UUID, BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_INDICATE);
    txCharacteristic = createCharacteristic(TX_UUID, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
    txCharacteristic->setCallbacks(callbacks);

    service->start();
    setupAdvertising();

    return true;
  }

  void send(const char* data) noexcept {
    if (clientConnected) {
      rxCharacteristic->setValue((uint8_t*)data, strlen(data));
      rxCharacteristic->notify();
#if DEBUG_LEVEL >= 1
      Serial.print("Sent: ");
      Serial.println(data);
#endif
    }
  }

  void onConnect(BLEServer*) override {
    clientConnected = true;
#if DEBUG_LEVEL >= 1
    Serial.println("Device connected");
#endif
    handleATSCommand = false;
    handleATHCommand = false;
#if DEBUG_LEVEL >= 1
    Serial.println("AT Commands Reset");
#endif
  }

  void onDisconnect(BLEServer*) override {
    clientConnected = false;
#if DEBUG_LEVEL >= 1
    Serial.println("Device disconnected");
#endif
    BLEDevice::startAdvertising();
  }

  bool handleATHCommand = false;
  bool handleATSCommand = false;
  bool ath1Active = false;

private:
  Device() noexcept
    : server(nullptr), service(nullptr), rxCharacteristic(nullptr),
      txCharacteristic(nullptr), clientConnected(false) {}

  BLECharacteristic* createCharacteristic(uint16_t uuid, uint32_t properties) {
    BLECharacteristic* characteristic = service->createCharacteristic(BLEUUID(uuid), properties);
    characteristic->addDescriptor(new BLE2902());
    return characteristic;
  }

  void setupAdvertising() {
    BLEAdvertising* advertising = BLEDevice::getAdvertising();
    advertising->addServiceUUID(service->getUUID());
    advertising->setScanResponse(false);
    BLEDevice::startAdvertising();
  }

  BLEServer* server;
  BLEService* service;
  BLECharacteristic* rxCharacteristic;
  BLECharacteristic* txCharacteristic;
  bool clientConnected;
};

class MyCallbacks : public BLECharacteristicCallbacks {
public:
  void onWrite(BLECharacteristic* characteristic) override {
    uint8_t* value = characteristic->getData();
    size_t len = characteristic->getLength();

    size_t start = 0;
    while (start < len && (value[start] == ' ' || value[start] == '\t' || value[start] == '\n' || value[start] == '\r')) {
      start++;
    }

    while (len > start && (value[len - 1] == ' ' || value[len - 1] == '\t' || value[len - 1] == '\n' || value[len - 1] == '\r')) {
      len--;
    }

#if DEBUG_LEVEL >= 1
    Serial.print("Received: ");
    Serial.write(value + start, len - start);
    Serial.println();
    Serial.print("Received data length: ");
    Serial.println(len - start);
#endif

    std::string command((char*)(value + start), len - start);
    std::string response = handleCommand(command);
    std::string finalResponse = sendResponse(command, response + "\r>");
    Device::getInstance().send(finalResponse.c_str());

#if DEBUG_LEVEL >= 2
    Serial.print("onWrite: Sent final response: ");
    Serial.println(finalResponse.c_str());
#endif
  }

private:
  std::string handleCommand(const std::string& command) {
    // Handle commands here using a map or switch-case for better organization
    std::string response;
    if (command == "ATI" || command == "AT@1") {
      response = "ELM327 v2.1";
    } else if (command == "ATS1" || command == "AT S1") {
      Device::getInstance().handleATSCommand = false;   // Turn Spaces On
      response = "OK";
    } else if (command == "ATS0" || command == "AT S0") {
      Device::getInstance().handleATSCommand = true;    // Turn Spaces Off
      response = "OK";
    } else if (command == "ATH1" || command == "AT H1") {
      response = "OK";
      Device::getInstance().handleATHCommand = true;
      Device::getInstance().setATH1Active(true);  // Set ATH1 active
    } else if (command == "ATH0" || command == "AT H0") {
      Device::getInstance().handleATHCommand = false;
      Device::getInstance().setATH1Active(false);  // Reset ATH1 active
      response = "OK";
    } else if (command == "ATZ") {  // Reset
      Device::getInstance().handleATHCommand = false;
      Device::getInstance().setATH1Active(false);
      Device::getInstance().handleATSCommand = false;
      response = "OK";
    } else if (command == "ATE0" || command == "ATPC" || command == "ATM0" ||
      command == "ATL0" || command == "ATST62" || command == "ATSP0" ||
      command == "ATSP0" || command == "ATAT1" || command == "ATAT2" ||
      command == "ATAT2" || command == "ATSP6" ||command == "ATSPA6") {
      response = "OK";
    } else if (command == "ATDPN") {
      response = "6"; // Protocol Number 6 CAN bus
    } else if (command == "0100") { // PID 1 - Supported PIDs [01-20]
      response = "41 00 04 1C D0 08 00";
    } else if (command == "0120") { // PID 2 - Supported PIDs [21-40]
      response = "41 20 00 00 10 00 00";
    } else if (command == "0140") { // PID 3 - Supported PIDs [41-60]
      response = "41 40 12 00 40 00 00";
    } else if (command == "0160") { // PID 4 - Supported PIDs [61-80]
      response = "41 60 02 00 00 00 00";
    } else if (command == "0180") { // PID 5 - Supported PIDs [81-A0]
      response = "41 80 NO DATA";
    } else if (command == "01A0") { // PID 6 - Supported PIDs [A1-C0]
      response = "41 A0 NO DATA";
    } else if (command == "01C0") { // PID 7 - Supported PIDs [C1-E0]
      response = "41 C0 NO DATA";
    
    // Handle PID Requests
    } else if (command == "0105") { // Engine Coolant Temperature (PID 0105)
      response = "41 05 64";  
    } else if (command == "0109") { // Long term fuel trim (PID 0109)
      response = "41 09 34"; 
    } else if (command == "010B") { // Intake Air Temperature (PID 010B)
      response = "41 0B 45";  
    } else if (command == "010C") { // RPM (PID 010C)
      response = "41 0C 64";  
    } else if (command == "010D") { // Vehicle Speed (PID 010D)
      response = "41 0D 64";  
    } else if (command == "0110") { // MAF (Mass Air Flow) Air Flow Rate (PID 0110)
      response = "41 10 FF";  
    } else if (command == "0111") { // Throttle Position (PID 0111)
      response = "41 11 64";  
    } else if (command == "0113") { // Engine RPM (PID 0113)
      response = "41 13 44";  
    } else if (command == "011C") { // Throttle Position (PID 011C)
      response = "41 1C 22";  
    } else if (command == "0133") { // Engine Coolant Temperature (PID 0133)
      response = "41 33 BC";  
    } else if (command == "0143") { // Intake Air Temperature (PID 0143)
      response = "41 43 64";  
    } else if (command == "0146") {  // Ambient Air Temperature (PID 0146)
      response = "41 46 64"; 
    } else if (command == "0151") { // Vehicle Speed (PID 0151)
      response = "41 51 55";  
    } else if (command == "0166") { // Engine Load (PID 0166)
      response = "41 66 64";  
    } else if (command == "0902") {  // VIN (PID 0902)
      response = "49 02 00 00 59 41 4D 41 48 41 45 53 50 33 32 4F 44 42"; 
    } else if (command == "0904") { // Calibration ID (PID 0904)
      response = "49 04 00 00 00 00";
    } else if (command == "090A") { // ECU Name (PID 090A)
      response = "49 0A 10 10 10 12";
      } else if (command == "01009") { // ???
      response = "41 009 NO DATA";
    } else {
#if DEBUG_LEVEL >= 1
      Serial.println(("Unknown command or length mismatch. Received: " + command).c_str());
#endif
    }
    return response;
  }

  std::string sendResponse(const std::string& command, const std::string& response) {
    Device& device = Device::getInstance();
    std::string finalResponse = response;

    if (device.ath1Active && command.rfind("01", 0) == 0) {
      finalResponse = "7E8 06 " + finalResponse;
      debugPrint("ATH1 Active: Headers appended");
    }

    if (device.handleATSCommand) {
      finalResponse.erase(std::remove(finalResponse.begin(), finalResponse.end(), ' '), finalResponse.end());
      debugPrint("ATS1 Active: Spaces removed");
    }

    debugPrint("Final Response: " + finalResponse);
    return finalResponse;
  }

  void debugPrint(const std::string& message) {
#if DEBUG_LEVEL == 2
    Serial.println(message.c_str());
#endif
  }
};

}  // namespace elm327

void setup() {
  Serial.begin(115200);
  Serial.println("Yamaha ESP32 S3 BLE elm327 Emulator");
  delay(1000); // Delay for 1000ms 
  elm327::MyCallbacks* myCallbacks = new elm327::MyCallbacks();
  bool success = elm327::Device::getInstance().start(myCallbacks);
  if (!success) {
    Serial.println("Failed to start BLE");
  }
}

void loop() {
}
