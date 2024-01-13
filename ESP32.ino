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
    }

    void onDisconnect(BLEServer*) override {
        clientConnected = false;
        Serial.println("Device disconnected");
        BLEDevice::startAdvertising();
    }

    bool headersOn = false;  // State variable for header

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
        handleCommand(command);
    }

private:
void handleCommand(const std::string& command) {
    std::string response;

    if (command == "ATI") {
        response = "ELM327 v2.1";
        Device::getInstance().headersOn = false;
    } else if (command == "ATZ" || command == "AT@1") {
        response = "ELM327 v2.1";
    } else if (command == "ATE0" || command == "ATPC" || command == "ATM0" ||
               command == "ATL0" || command == "ATST62" || command == "ATS0" ||
               command == "ATSP0" || command == "ATAT1" || command == "ATAT2" ||
               command == "ATSPA5") {
        response = "OK";
    } else if (command == "ATH1" || command == "AT H1") {
        handleATHCommand(true);  // Turn headers off
        response = "OK";
    } else if (command == "ATH0" || command == "AT H0") {
        handleATHCommand(false);  // Turn headers on
        response = "OK";
    } else if (command == "ATDPN") {
        response = "0";
    } else if (command == "0100") {
        response = "41 00 05 0D A4";
    } else if (command == "0120") {
        response = "41 20 NO DATA";
    } else if (command == "010D") {
        response = "41 0D 64";
    } else if (command == "0111") {
        response = "41 00";
    } else {
        response = "Unknown command or length mismatch. Received: " + command;
    }

    // Send a carriage return to indicate readiness for the next command
    sendResponse(response + "\r"); 
      
    return;

}



    void sendResponse(const std::string& response) {
        // Create a new string by appending ">" to the response after '\r'
        std::string responseWithModifiedCR = response;
        size_t pos = responseWithModifiedCR.find_last_of('\r');
        if (pos != std::string::npos) {
            responseWithModifiedCR.insert(pos + 1, ">");
        }

        // Send the modified response
        elm327::Device::getInstance().send(responseWithModifiedCR.c_str());
    }

    void handleATHCommand(bool state) {
        elm327::Device::getInstance().headersOn = state;
        sendResponse("OK");
    }
};

}  // namespace elm327

void setup() {
    Serial.begin(115200);
    elm327::MyCallbacks* myCallbacks = new elm327::MyCallbacks();
    if (elm327::Device::getInstance().start(myCallbacks)) {
        Serial.println("BLE startup successful.");
    } else {
        Serial.println("ERROR: BLE startup failed!");
        esp_restart();
    }
}

void loop() {
    
}
