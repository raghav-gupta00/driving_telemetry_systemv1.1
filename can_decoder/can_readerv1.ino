#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

#define ON_Board_LED 2 // Onboard LED for indicators

const char* ssid = "-----"; //give ssid
const char* password = "------"; //give password

const char* host = "script.google.com";
const int httpsPort = 443;

WiFiClientSecure client; 
// give Google App Script Id where javascript is deployed
//String GAS_ID = "AKfycbzfsQBHs5LiKE0QIXNKTzgGNmUaV9PAsp4aynZhXvqsGWXmmhXXOgACbrQ94Ov5ORwfvQ";

// Structure to hold decoded packet
struct CANPacket {
    int address; // Address in integer
    int data;    // Data in integer
};

// Array to store packets (max 10 buffer for simplicity)
#define MAX_PACKETS 10
CANPacket decodedPackets[MAX_PACKETS];
int packetCount = 0; // Number of packets stored

void setup() {
    Serial.begin(115200);
    delay(500);
    pinMode(13,OUTPUT);
    WiFi.begin(ssid, password);
    Serial.println("");
    pinMode(ON_Board_LED, OUTPUT);
    digitalWrite(ON_Board_LED, HIGH);

    Serial.print("Connecting");
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        digitalWrite(ON_Board_LED, LOW);
        delay(250);
        digitalWrite(ON_Board_LED, HIGH);
        delay(250);
    }

    digitalWrite(ON_Board_LED, HIGH);
    Serial.println("");
    Serial.print("Successfully connected to: ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.println();

    client.setInsecure();

    Serial.println("Starting CAN packet decoder...");
    Serial.println("Enter binary CAN packets separated by spaces or newlines.");
    Serial.println("Each packet must be exactly 28 bits: 12-bit Address and 16-bit Data.");
}

void loop() {
    if (Serial.available() > 0) {
        String inputStream = Serial.readStringUntil('\n');
        inputStream.trim();

        if (inputStream.length() > 0) {
            Serial.println("Received CAN packets:");
            Serial.println(inputStream);

            processMultiplePackets(inputStream);

            // After processing all packets, send them
            sendAllData();
        } else {
            Serial.println("No valid data entered. Please try again.");
        }
    }
}

void processMultiplePackets(String stream) {
    Serial.println("Processing CAN packets...");
    packetCount = 0; // Reset packet count

    // Split the input stream into separate packets
    int start = 0;
    int end = stream.indexOf(' ');
    while (end != -1) {
        String packet = stream.substring(start, end);
        packet.trim();
        processSinglePacket(packet);

        start = end + 1;
        end = stream.indexOf(' ', start);
    }

    // Process the last packet
    String lastPacket = stream.substring(start);
    lastPacket.trim();
    processSinglePacket(lastPacket);
}

void processSinglePacket(String packet) {
    packet.trim();
    if (packet.length() != 28) {
        Serial.print("Error: Packet must be exactly 28 bits long: ");
        Serial.println(packet);
        return;
    }

    for (int i = 0; i < packet.length(); i++) {
        if (packet[i] != '0' && packet[i] != '1') {
            Serial.print("Error: Invalid character in packet: ");
            Serial.println(packet);
            return;
        }
    }

    if (packetCount >= MAX_PACKETS) {
        Serial.println("Error: Maximum packet storage limit reached!");
        return;
    }

    decodeCANPacket(packet);
}

void decodeCANPacket(String binaryData) {
    char address[13] = {0};
    char data[17] = {0};

    binaryData.substring(0, 12).toCharArray(address, 13);
    binaryData.substring(12, 28).toCharArray(data, 17);

    int addressInt = strtol(address, nullptr, 2);
    int dataInt = strtol(data, nullptr, 2);

    // Store the decoded packet in the array
    decodedPackets[packetCount].address = addressInt;
    decodedPackets[packetCount].data = dataInt;
    packetCount++;

    // Print the decoded packet for debugging
    Serial.println("Decoded CAN Packet:");
    Serial.print("  Address: 0x");
    Serial.println(addressInt, HEX);
    Serial.print("  Data: 0x");
    Serial.println(dataInt, HEX);
}

void sendAllData() {
    Serial.println("Sending all decoded packets...");
    for (int i = 0; i < packetCount; i++) {

        if (decodedPackets[i].address == 1 && decodedPackets[i].data > 150) {
            sendData(decodedPackets[i].address, decodedPackets[i].data);
        }

        if (decodedPackets[i].address == 2 && decodedPackets[i].data == 0) {
            sendData(decodedPackets[i].address, decodedPackets[i].data);
            digitalWrite(13, LOW);
            delay(500);
            digitalWrite(13,HIGH);
            
        }

        if (decodedPackets[i].address == 3 && decodedPackets[i].data > 4000) {
            sendData(decodedPackets[i].address, decodedPackets[i].data);
        }

        if (decodedPackets[i].address == 4 && decodedPackets[i].data > 80) {
            sendData(decodedPackets[i].address, decodedPackets[i].data);
        }

        if (decodedPackets[i].address == 5 && decodedPackets[i].data > 80) {
            sendData(decodedPackets[i].address, decodedPackets[i].data);
        }

        if (decodedPackets[i].address == 6 && decodedPackets[i].data < 5) {
            sendData(decodedPackets[i].address, decodedPackets[i].data);
        }
    }
    packetCount = 0; // Clear packets after sending
}


void sendData(int address, int data) {
    Serial.println("==========");
    Serial.print("Connecting to ");
    Serial.println(host);

    if (!client.connect(host, httpsPort)) {
        Serial.println("Connection failed");
        return;
    }

    String string_address = String(address, DEC);
    String string_data = String(data, DEC);
    String url = "/macros/s/" + GAS_ID + "/exec?address=" + string_address + "&data=" + string_data;

    Serial.print("Requesting URL: ");
    Serial.println(url);

    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" +
                 "User-Agent: ESP8266CANReader\r\n" +
                 "Connection: close\r\n\r\n");

    while (client.connected()) {
        String line = client.readStringUntil('\n');
        if (line == "\r") {
            Serial.println("Headers received");
            break;
        }
    }

    String line = client.readStringUntil('\n');
    if (line.startsWith("{\"state\":\"success\"")) {
        Serial.println("Data sent successfully!");
    } else {
        Serial.println("Failed to send data");
    }

    Serial.println("==========");
}
