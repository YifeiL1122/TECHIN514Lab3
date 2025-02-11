#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// 定义 BLE UUID
static BLEUUID serviceUUID("48848b39-e4c9-4cfc-8569-ea3d56eec20b");
static BLEUUID charUUID("dd0a5dda-47bd-428f-ab5d-ed8cc9a69d03");

// BLE 连接状态变量
static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;
static BLEAdvertisedDevice* myDevice;

// 存储最大 & 最小距离
float maxDistance = 0;
float minDistance = 9999;
int dataCount = 0; // 记录接收到的数据次数

// 更新最大最小值
void updateMinMax(float currentDistance) {
    if (currentDistance > maxDistance) maxDistance = currentDistance;
    if (currentDistance < minDistance) minDistance = currentDistance;
}

// 回调函数: 接收 BLE 数据
static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    char dataStr[20];
    size_t copyLength = (length < sizeof(dataStr) - 1) ? length : (sizeof(dataStr) - 1);
    memcpy(dataStr, pData, copyLength);
    dataStr[copyLength] = '\0';

    float currentDistance = atof(dataStr);
    updateMinMax(currentDistance);
    dataCount++; // 记录数据条数

    // 串口打印当前数据、最大 & 最小值
    Serial.printf("[DATA %d] Received Distance: %.2f cm | Max: %.2f cm | Min: %.2f cm\n", dataCount, currentDistance, maxDistance, minDistance);
    
    if (dataCount >= 10) {
        Serial.println("\nReceived at least 10 readings! Take screenshot now.\n");
    }
}

// 处理 BLE 连接回调
class MyClientCallback : public BLEClientCallbacks {
    void onConnect(BLEClient* pclient) {}
    void onDisconnect(BLEClient* pclient) {
        connected = false;
        Serial.println("Disconnected from BLE server");
    }
};

// 连接到 BLE 服务器
bool connectToServer() {
    Serial.printf("Connecting to: %s\n", myDevice->getAddress().toString().c_str());
    
    BLEClient* pClient = BLEDevice::createClient();
    pClient->setClientCallbacks(new MyClientCallback());
    
    if (!pClient->connect(myDevice)) {
        Serial.println("Failed to connect to device!");
        return false;
    }
    Serial.println("Connected to device!");

    pClient->setMTU(517);
    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (!pRemoteService) {
        Serial.println("Failed to find service!");
        pClient->disconnect();
        return false;
    }
    Serial.println("Service found!");

    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
    if (!pRemoteCharacteristic) {
        Serial.println("Failed to find characteristic!");
        pClient->disconnect();
        return false;
    }
    Serial.println("Characteristic found!");

    if (pRemoteCharacteristic->canNotify()) {
        Serial.println("Subscribing to notifications...");
        pRemoteCharacteristic->registerForNotify(notifyCallback);
    }
    
    connected = true;
    return true;
}

// 扫描 BLE 设备
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        Serial.printf("Found device: %s\n", advertisedDevice.getAddress().toString().c_str());
        
        if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {
            Serial.println("Found target BLE device! Stopping scan...");
            BLEDevice::getScan()->stop();
            myDevice = new BLEAdvertisedDevice(advertisedDevice);
            doConnect = true;
            doScan = true;
        }
    }
};

void setup() {
    Serial.begin(115200);
    delay(2000);  // 确保串口初始化完成
    Serial.println("Starting BLE scan...");
    
    BLEDevice::init("");
    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setInterval(1349);
    pBLEScan->setWindow(449);
    pBLEScan->setActiveScan(true);
    pBLEScan->start(10, false);
    Serial.println("BLE Scan Started!");
}

void loop() {
    if (doConnect) {
        Serial.println("Attempting to connect...");
        if (connectToServer()) {
            Serial.println("Successfully connected!");
        } else {
            Serial.println("Failed to connect!");
        }
        doConnect = false;
    }

    if (connected) {
        Serial.println("Connected, waiting for notifications...");
        delay(1000);
    } else if (doScan) {
        Serial.println("Scanning for BLE devices...");
        BLEDevice::getScan()->start(0);
    }
}
