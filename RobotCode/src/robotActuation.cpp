#include "robotActuation.h"
#include <iostream>

/**
 * @brief Constructor
 * @param port The port the adafruit feather is expected on
 * @param baud_rate Baud_rate for the communication (should be 115200 per specification)
 */
RobotActuation::RobotActuation(std::string port, unsigned int baud_rate) : io(), serial(io) {
    try {
        serial.open(port);
        serial.set_option(asio::serial_port_base::baud_rate(baud_rate));
    } catch (asio::system_error &e) {
        std::cout << "Failed to open serial port: " << e.what() << std::endl;
    }
}

bool RobotActuation::readNextMessage(SerialPacket *packet) {
    if (!serial.is_open()) {
        return false;
    }

    while (recvData.size() >= SERIAL_MES_LEN) {
        uint8_t syncByte1 = recvData.front();
        recvData.pop();
        uint8_t syncByte2 = recvData.front();
        recvData.pop();
        if (syncByte1 == 0xBE && syncByte2 == 0xEF) {
            packet->portions.messageType = recvData.front();
            recvData.pop();

            for (int i = 0; i < SERIAL_MES_DATA_LEN; i++) {
                packet->portions.data[i] = recvData.front();
                recvData.pop();
            }

            // remove checksums
            recvData.pop();
            recvData.pop();
            return true;
        } else {
            std::cout << "Incorrect sync bytes: " << std::to_string((int)syncByte1) << " and "
                      << std::to_string((int)syncByte2) << std::endl;
        }
    }

    return false;
}

void RobotActuation::sendDriveMotors(int8_t frontLeftMotor, int8_t frontRightMotor, int8_t backLeftMotor,
                                     int8_t backRightMotor) {
    SerialPacket packet = {0xBE, 0xEF};

    packet.portions.messageType = 0x81;

    packet.portions.data[0] = frontLeftMotor;
    packet.portions.data[1] = frontRightMotor;
    packet.portions.data[2] = backLeftMotor;
    packet.portions.data[3] = backRightMotor;

    enqueueMessage(&packet);
}

void RobotActuation::sendIntakeMotor(int8_t intakeMotorSpeed) {
    SerialPacket packet = {0xBE, 0xEF};

    packet.portions.messageType = 0x83;

    packet.portions.data[0] = intakeMotorSpeed;

    enqueueMessage(&packet);
}

void RobotActuation::sendDumpMotor(int8_t dumpMotorSpeed) {
    SerialPacket packet = {0xBE, 0xEF};

    packet.portions.messageType = 0x84;

    packet.portions.data[0] = dumpMotorSpeed;

    enqueueMessage(&packet);
}

void RobotActuation::sendDeployControl(int8_t angle) {
    SerialPacket packet = {0xBE, 0xEF};

    packet.portions.messageType = 0x82;

    packet.portions.data[0] = angle;

    enqueueMessage(&packet);
}

void RobotActuation::sendHeartbeat() {
    SerialPacket packet = {0xBE, 0xEF};

    packet.portions.messageType = 0x01;

    enqueueMessage(&packet);
}

int RobotActuation::sendCurrentQueue() {
    if (byteQueueFull && !serialTransmit) {
        serialTransmit = true;
        serial.async_write_some(asio::buffer(outgoingBytes, SERIAL_MES_LEN),
                                [this](const asio::error_code &error, std::size_t bytes_transferred) {
                                    sendBytesHandler(error, bytes_transferred);
                                });
    }
    if (!byteQueueFull && !serialTransmit && !outgoingQueue.empty()) {
        SerialPacket *packet = &outgoingQueue.front();
        addChecksum(packet);
        std::copy(std::begin(packet->packet), std::end(packet->packet), outgoingBytes);

        outgoingQueue.pop();
        byteQueueFull = true;
    }
    return 0;
}

void RobotActuation::sendBytesHandler(const asio::error_code &error, std::size_t bytes_transferred) {
    // std::cout << "Sent " << unsigned(bytes_transferred) << " bytes" << std::endl;
    // std::cout << "Error code: " << error.value() << std::endl;
    positonOfNextOutgoingByte += bytes_transferred;
    if (positonOfNextOutgoingByte >= SERIAL_MES_LEN) {
        byteQueueFull = false;
        serialTransmit = false;
        positonOfNextOutgoingByte = 0;
    }
}

void RobotActuation::onRead(const asio::error_code &ec, size_t len) {
    if (ec.value() == 0) {
        for (size_t i = 0; i < len; i++) {
            this->recvData.push(rxBuf[i]);
        }

        memset(rxBuf, 0, SERIAL_RX_BUF_SIZE);
    } else {
        std::cout << "Failed to read from serial port: " << ec.message() << std::endl;
    }
    this->reading = false;
}

void RobotActuation::startReading() {
    serial.async_read_some(asio::buffer(this->rxBuf, SERIAL_RX_BUF_SIZE),
                           [this](const asio::error_code &ec, size_t len) { this->onRead(ec, len); });
    this->reading = true;
}

void RobotActuation::run() {
    if (io.stopped()) {
        io.reset();
    }
    sendCurrentQueue();
    io.poll();

    if (serial.is_open() && !reading) {
        startReading();
    }
}

bool RobotActuation::isConnected() { return serial.is_open(); }

void RobotActuation::enqueueMessage(SerialPacket *mess) { outgoingQueue.push(*mess); }

void RobotActuation::addChecksum(SerialPacket *packet) {
    uint8_t *ptr = packet->packet;
    uint16_t checksum = fletcher16(ptr, SERIAL_MES_LEN - 2);
    packet->portions.checksumHigh = (checksum >> 8) & 0xff;
    packet->portions.checksumLow = checksum & 0xff;
}

uint16_t RobotActuation::fletcher16(const uint8_t *data, size_t len) {
    uint32_t c0, c1;

    /*  Found by solving for c1 overflow: */
    /* n > 0 and n * (n+1) / 2 * (2^8-1) < (2^32-1). */
    for (c0 = c1 = 0; len > 0;) {
        size_t blocklen = len;
        if (blocklen > 5802) {
            blocklen = 5802;
        }
        len -= blocklen;
        do {
            c0 = c0 + *data++;
            c1 = c1 + c0;
        } while (--blocklen);
        c0 = c0 % 255;
        c1 = c1 % 255;
    }
    return (c1 << 8 | c0);
}

int SerialPacket::GetIntakePos() {
    if(this->portions.messageType != SERIAL_PACKET_INTAKE_POS) {
        throw new std::runtime_error("packet has incorrect type: must be intake pos");
    }

    int pos = portions.data[0];
    pos |= (portions.data[1] << 8);

    return pos;
}

std::string SerialPacket::GetLogMessage() {
    std::string msg;

    for(size_t i = 0; i < SERIAL_MES_DATA_LEN; i++) {
        uint8_t c = portions.data[i];

        if(c != 0) {
            msg.push_back((char) c);
        } else {
            break;
        }
    }

    return msg;
}


SerialPacketType SerialPacket::GetType() { return (SerialPacketType)portions.messageType; }