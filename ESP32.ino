#include <BLE2902.h>
#include <BLECharacteristic.h>
#include <BLEDevice.h>

namespace elm327 {

// Define UUIDs as constants
const uint16_t SERVICE_UUID = 0xFFF0;
const uint16_t RX_UUID = 0xFFF1;
const uint16_t TX_UUID = 0xFFF2;

class Device final : public BLEServerCallbacks {
public:
    ~Device() noexcept override = default;

    static Device& getInstance() noexcept {
        static Device instance;
        return instance;
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

        // Create RX characteristic
        rxCharacteristic = createCharacteristic(RX_UUID,
            BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_INDICATE);

        // Create TX characteristic
        txCharacteristic = createCharacteristic(TX_UUID,
            BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
        txCharacteristic->setCallbacks(callbacks);

        service->start();

        // Set up advertising
        setupAdvertising();

        return true;
    }

    void send(const char* data) noexcept {
        if (clientConnected) {
            rxCharacteristic->setValue((uint8_t*)data, strlen(data));
            rxCharacteristic->notify();
            Serial.print("Sent: ");
            Serial.println(data);
        }
    }

    void onConnect(BLEServer*) override {
    clientConnected = true;
    Serial.println("Device connected");

    // Set handleATSCommand and handleATHCommand to false when connected
    Device::getInstance().handleATSCommand = false;
    Device::getInstance().handleATHCommand = false;
    Serial.println("AT Commands Reset");
}

    void onDisconnect(BLEServer*) override {
        clientConnected = false;
        Serial.println("Device disconnected");
        BLEDevice::startAdvertising();
    }

    bool handleATHCommand = false;
    bool handleATSCommand = false;

private:
    Device() noexcept
        : server(nullptr), service(nullptr), rxCharacteristic(nullptr),
        txCharacteristic(nullptr), clientConnected(false) {}

    // Helper function to create characteristics
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

        // Trim all trailing and leading whitespace characters
        size_t start = 0;
        while (start < len &&
            (value[start] == ' ' || value[start] == '\t' ||
                value[start] == '\n' || value[start] == '\r')) {
            start++;
        }

        while (len > start &&
            (value[len - 1] == ' ' || value[len - 1] == '\t' ||
                value[len - 1] == '\n' || value[len - 1] == '\r')) {
            len--;
        }

        Serial.print("Received: ");
        Serial.write(value + start, len - start);
        Serial.println();

        // Add debugging output to print the length of the received data
        Serial.print("Received data length: ");
        Serial.println(len - start);

        std::string command((char*)(value + start), len - start);

        // Handle commands here using a map or switch-case for better organization
        std::string response = handleCommand(command);

        // Send a carriage return to indicate readiness for the next command
        sendResponse(response + "\r>");
    }

private:
    std::string handleCommand(const std::string& command) {
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
            Device::getInstance().handleATHCommand = true;    // Turn Headers On
        } else if (command == "ATH0" || command == "AT H0") {
            Device::getInstance().handleATHCommand = false;   // Turn Headers Off
            response = "OK";
        } else if (command == "ATZ") {                        // Simulate a ELM327 Reset
            Device::getInstance().handleATHCommand = false;
            Device::getInstance().handleATSCommand = false;
            response = "OK";
        } else if (command == "ATE0" || command == "ATPC" || command == "ATM0" ||
            command == "ATL0" || command == "ATST62" || command == "ATSP0" ||
            command == "ATSP0" || command == "ATAT1" || command == "ATAT2" ||
            command == "ATAT2" || command == "ATSPA5") {
            response = "OK";
        } else if (command == "ATDPN") {
            response = "5"; // Protocol Number
        } else if (command == "0100") {
            response = "41 00 05 0D A4"; // Fake PID Response
        } else if (command == "0120") {
            response = "41 20 NO DATA";
        } else if (command == "010D") {
            response = "41 0D 64";       // Speed
        } else if (command == "0111") {
            response = "41 00";          // Shouldn't be called
        } else {
            Serial.println(("Unknown command or length mismatch. Received: " + command).c_str());
        }
        return response;
    }

    // Need to tidy up SendResponse
    void sendResponse(const std::string& response) {
        std::string finalResponse = response;

        if (Device::getInstance().handleATSCommand && Device::getInstance().handleATHCommand) {
            finalResponse.insert(0, "7E8 04 "); // Simulated Header output is wrong for Protocol 5, TO FIX
            finalResponse.erase(std::remove(finalResponse.begin(), finalResponse.end(), ' '), finalResponse.end());
        } else if (Device::getInstance().handleATSCommand) {
            finalResponse.erase(std::remove(finalResponse.begin(), finalResponse.end(), ' '), finalResponse.end());
        } else if (Device::getInstance().handleATHCommand) {
            finalResponse.insert(0, "7E8 04 "); // Simulated Header output is wrong for Protocol 5, TO FIX
        }

        // Send the final response
        elm327::Device::getInstance().send(finalResponse.c_str());
    }
};

}  // namespace elm327

void setup() {
    Serial.begin(115200);
    elm327::MyCallbacks* myCallbacks = new elm327::MyCallbacks();
    bool success = elm327::Device::getInstance().start(myCallbacks);
    if (!success) {
        Serial.println("Failed to start BLE");
    }
}

void loop() {

}
