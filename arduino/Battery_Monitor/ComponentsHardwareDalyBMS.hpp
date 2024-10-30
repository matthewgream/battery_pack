
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include "HardwareSerial.h"

#include <cstdint>
#include <array>
#include <map>
#include <bitset>

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class RequestResponseFrame_Receiver;

class RequestResponseFrame {
public:
    using Receiver = RequestResponseFrame_Receiver;

    struct Constants {
        static constexpr size_t SIZE_FRAME = 13;
        static constexpr size_t SIZE_HEADER = 4;
        static constexpr size_t SIZE_DATA = (SIZE_FRAME - SIZE_HEADER - 1);

        static constexpr size_t OFFSET_BYTE_START = 0;
        static constexpr size_t OFFSET_ADDRESS = 1;
        static constexpr size_t OFFSET_COMMAND = 2;
        static constexpr size_t OFFSET_SIZE = 3;
        static constexpr size_t OFFSET_CHECKSUM = (SIZE_FRAME - 1);

        static constexpr uint8_t VALUE_BYTE_START = 0xA5;
        static constexpr uint8_t VALUE_ADDRESS_HOST = 0x40;
        static constexpr uint8_t VALUE_ADDRESS_SLAVE = 0x01;
    };

    RequestResponseFrame()
        : _data{} {
        setStartByte(Constants::VALUE_BYTE_START);
        setFrameSize(Constants::SIZE_DATA);
    }

    void setAddress(const uint8_t value) {
        _data[Constants::OFFSET_ADDRESS] = value;
    }
    uint8_t getCommand() const {
        return _data[Constants::OFFSET_COMMAND];
    }
    void setCommand(const uint8_t value) {
        _data[Constants::OFFSET_COMMAND] = value;
    }

    const RequestResponseFrame& finalize() {
        _data[Constants::OFFSET_CHECKSUM] = calculateChecksum();
        return *this;
    }

    bool valid() const {
        return _data[Constants::OFFSET_BYTE_START] == Constants::VALUE_BYTE_START && _data[Constants::OFFSET_ADDRESS] != Constants::VALUE_ADDRESS_SLAVE && _data[Constants::OFFSET_SIZE] == Constants::SIZE_DATA && _data[Constants::OFFSET_CHECKSUM] == calculateChecksum();
    }

    //

    inline uint8_t getUInt8(const size_t offset) const {
        validateDataOffset(offset);
        return _data[Constants::SIZE_HEADER + offset];
    }
    inline RequestResponseFrame& setUInt8(const size_t offset, const uint8_t value) {
        validateDataOffset(offset);
        _data[Constants::SIZE_HEADER + offset] = value;
        return *this;
    }
    inline uint16_t getUInt16(const size_t offset) const {
        validateDataOffset(offset);
        return ((uint16_t)_data[Constants::SIZE_HEADER + offset] << 8) | (uint16_t)_data[Constants::SIZE_HEADER + offset + 1];
    }
    inline uint32_t getUInt32(const size_t offset) const {
        validateDataOffset(offset);
        return ((uint32_t)_data[Constants::SIZE_HEADER + offset] << 24) | ((uint32_t)_data[Constants::SIZE_HEADER + offset + 1] << 16) | ((uint32_t)_data[Constants::SIZE_HEADER + offset + 2] << 8) | (uint32_t)_data[Constants::SIZE_HEADER + offset + 3];
    }
    inline bool getBit(const size_t offset, const uint8_t position) const {
        validateDataOffset(offset);
        return (_data[Constants::SIZE_HEADER + offset] >> position) & 0x01;
    }
    inline bool getBit(const size_t index) const {
        validateDataOffset(index >> 3);
        return (_data[Constants::SIZE_HEADER + (index >> 3)] >> (index & 0x07)) & 0x01;
    }

    const uint8_t* data() const {
        return _data.data();
    }
    static constexpr size_t size() {
        return Constants::SIZE_FRAME;
    }

protected:
    friend Receiver;
    uint8_t& operator[](const size_t index) {
        assert(index < Constants::SIZE_FRAME);
        return _data[index];
    }
    const uint8_t& operator[](const size_t index) const {
        assert(index < Constants::SIZE_FRAME);
        return _data[index];
    }

private:
    void setStartByte(const uint8_t value) {
        _data[Constants::OFFSET_BYTE_START] = value;
    }
    void setFrameSize(const uint8_t value) {
        _data[Constants::OFFSET_SIZE] = value;
    }

    inline void validateDataOffset(const size_t offset) const {
        assert(offset < Constants::SIZE_DATA);
    }

    uint8_t calculateChecksum() const {
        uint8_t sum = 0;
        for (int i = 0; i < Constants::OFFSET_CHECKSUM; i++)
            sum += _data[i];
        return sum;
    }

    std::array<uint8_t, Constants::SIZE_FRAME> _data;
};

// -----------------------------------------------------------------------------------------------

class RequestResponseFrame_Receiver {

protected:
    virtual bool getByte(uint8_t* byte) = 0;
    virtual bool sendBytes(const uint8_t* data, const size_t size) = 0;

public:
    using Handler = std::function<void(const RequestResponseFrame&)>;

    enum class ReadState {
        WaitingForStart,
        ReadingHeader,
        ReadingData
    };

    RequestResponseFrame_Receiver(const Handler& handler)
        : _handler(handler) {}

    void process() {
        uint8_t byte;
        while (getByte(&byte)) {
            switch (_readState) {
                case ReadState::WaitingForStart:
                    if (byte == RequestResponseFrame::Constants::VALUE_BYTE_START) {
                        _readFrame[RequestResponseFrame::Constants::OFFSET_BYTE_START] = byte;
                        _readOffset = RequestResponseFrame::Constants::OFFSET_ADDRESS;
                        _readState = ReadState::ReadingHeader;
                    }
                    break;

                case ReadState::ReadingHeader:
                    _readFrame[_readOffset++] = byte;
                    if (_readOffset == RequestResponseFrame::Constants::SIZE_HEADER) {
                        if (_readFrame[RequestResponseFrame::Constants::OFFSET_ADDRESS] >= RequestResponseFrame::Constants::VALUE_ADDRESS_SLAVE) {    // SLEEPING
                            _readState = ReadState::WaitingForStart;
                            _readOffset = RequestResponseFrame::Constants::OFFSET_BYTE_START;
                        } else {
                            _readState = ReadState::ReadingData;
                        }
                    }
                    break;

                case ReadState::ReadingData:
                    _readFrame[_readOffset++] = byte;
                    if (_readOffset == RequestResponseFrame::Constants::SIZE_FRAME) {
                        if (_readFrame.valid())
                            _handler(_readFrame);
                        _readState = ReadState::WaitingForStart;
                        _readOffset = RequestResponseFrame::Constants::OFFSET_BYTE_START;
                    }
                    break;
            }
        }
    }

    void write(const RequestResponseFrame& frame) {
        sendBytes(frame.data(), frame.size());
        process();
    }

private:
    const Handler _handler;
    ReadState _readState = ReadState::WaitingForStart;
    size_t _readOffset = RequestResponseFrame::Constants::OFFSET_BYTE_START;
    RequestResponseFrame _readFrame;
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class RequestResponse_Builder {
public:
    RequestResponse_Builder() {
        _request.setAddress(RequestResponseFrame::Constants::VALUE_ADDRESS_HOST);
    }
    RequestResponse_Builder& setCommand(const uint8_t cmd) {
        _request.setCommand(cmd);
        return *this;
    }
    RequestResponse_Builder& setResponseCount(const size_t count) {
        _responseCount = count;
        return *this;
    }
    const RequestResponseFrame& getRequest() {
        return _request.finalize();
    }
    size_t getResponseCount() const {
        return _responseCount;
    }
private:
    RequestResponseFrame _request;
    size_t _responseCount = 1;
};

// -----------------------------------------------------------------------------------------------

class RequestResponse {
public:
    using Builder = RequestResponse_Builder;

    RequestResponse(RequestResponse_Builder& builder)
        : _request(builder.getRequest()), _responsesExpected(builder.getResponseCount()), _responsesReceived(0) {}
    virtual ~RequestResponse() = default;
    uint8_t getCommand() const {
        return _request.getCommand();
    }
    bool isValid() const {
        return _valid;
    }
    virtual bool isRequestable() const {
        return true;
    }
    virtual RequestResponseFrame prepareRequest() {
        _responsesReceived = 0;
        return _request;
    }
    bool processResponse(const RequestResponseFrame& frame) {
        if (_responsesReceived++ < _responsesExpected && frame.getUInt8(0) == _responsesReceived)
            return processFrame(frame, _responsesReceived);
        else return false;
    }
protected:
    void setValid(const bool v = true) {
        _valid = v;
    }
    virtual bool processFrame(const RequestResponseFrame& frame, const size_t number) = 0;
    void setResponseCount(const size_t count) {
        _responsesExpected = count;
    }
private:
    bool _valid = false;
    RequestResponseFrame _request;
    size_t _responsesExpected, _responsesReceived;
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

/*
  offical 0x90-0x98
  https://robu.in/wp-content/uploads/2021/10/Daly-CAN-Communications-Protocol-V1.0-1.pdf

  unofficial
  https://diysolarforum.com/threads/decoding-the-daly-smartbms-protocol.21898/
  https://diysolarforum.com/threads/daly-bms-communication-protocol.65439/
*/

template<uint8_t COMMAND>
class RequestResponseCommand : public RequestResponse {
public:
    RequestResponseCommand()
        : RequestResponse(RequestResponse::Builder().setCommand(COMMAND)) {}
protected:
    using RequestResponse::setValid;
    using RequestResponse::setResponseCount;
};

template<uint8_t COMMAND>
bool operator==(const RequestResponseCommand<COMMAND>& lhs, const uint8_t rhs) {
    return rhs == COMMAND;
}

template<uint8_t COMMAND, int LENGTH = 1>
class RequestResponse_TYPE_STRING : public RequestResponseCommand<COMMAND> {
public:
    String string;
    RequestResponse_TYPE_STRING()
        : RequestResponseCommand<COMMAND>() {
        setResponseCount(LENGTH);
    }
protected:
    using RequestResponseCommand<COMMAND>::setValid;
    using RequestResponseCommand<COMMAND>::setResponseCount;
    bool processFrame(const RequestResponseFrame& frame, const size_t frameNum) override {
        if (frameNum == 1) string = "";
        for (size_t i = 0; i < RequestResponseFrame::Constants::SIZE_DATA - 1; i++)
            string += static_cast<char>(frame.getUInt8(1 + i));
        if (frameNum == LENGTH) {
            string.trim();
            setValid();
        }
        return true;
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class RequestResponse_BATTERY_RATINGS : public RequestResponseCommand<0x50> {
public:
    float packCapacityAh = 0.0f;
    float nominalCellVoltageV = 0.0f;
protected:
    bool processFrame(const RequestResponseFrame& frame, const size_t) override {
        packCapacityAh = static_cast<float>(frame.getUInt32(0)) / 1000.0f;
        nominalCellVoltageV = static_cast<float>(frame.getUInt32(4)) / 1000.0f;
        setValid();
        return true;
    }
};

class RequestResponse_BMS_HARDWARE_CONFIG : public RequestResponseCommand<0x51> {
public:
    uint8_t boardCount = 0;
    std::array<uint8_t, 3> cellCounts{};
    std::array<uint8_t, 3> sensorCounts{};
protected:
    bool processFrame(const RequestResponseFrame& frame, const size_t) override {
        boardCount = frame.getUInt8(0);
        for (size_t i = 0; i < cellCounts.size(); i++)
            cellCounts[i] = frame.getUInt8(1 + i);
        for (size_t i = 0; i < cellCounts.size(); i++)
            sensorCounts[i] = frame.getUInt8(4 + i);
        setValid();
        return true;
    }
};

class RequestResponse_BATTERY_STAT : public RequestResponseCommand<0x52> {
public:
    float cumulativeChargeAh;
    float cumulativeDischargeAh;
protected:
    bool processFrame(const RequestResponseFrame& frame, const size_t) override {
        cumulativeChargeAh = static_cast<float>(frame.getUInt32(0));       // XXX TBC
        cumulativeDischargeAh = static_cast<float>(frame.getUInt32(4));    // XXX TBC
        setValid();
        return true;
    }
};

class RequestResponse_BATTERY_INFO : public RequestResponseCommand<0x53> {    // XXX TBC
public:
    uint8_t batteryOperationMode = 0;
    uint8_t batteryType = 0;
    struct {
        uint8_t year;    // 0-99 + 2000
        uint8_t month;
        uint8_t day;
        String toString() const {
            char buffer[9];
            snprintf(buffer, sizeof(buffer), "20%02d%02d%02d", year, month, day);
            return String(buffer);
        }
    } productionDate;
    uint16_t automaticSleepSec = 0;
    uint8_t unknown1 = 0, unknown2 = 0;
protected:
    bool processFrame(const RequestResponseFrame& frame, const size_t) override {
        batteryOperationMode = frame.getUInt8(0);
        batteryType = frame.getUInt8(1);
        productionDate = { .year = frame.getUInt8(2), .month = frame.getUInt8(3), .day = frame.getUInt8(4) };
        automaticSleepSec = static_cast<uint16_t>(frame.getUInt8(5)) * 60;
        unknown1 = frame.getUInt8(6);
        unknown2 = frame.getUInt8(7);
        setValid();
        return true;
    }
};

using RequestResponse_BMS_FIRMWARE_INDEX = RequestResponse_TYPE_STRING<0x54, 1>;

using RequestResponse_BATTERY_CODE = RequestResponse_TYPE_STRING<0x57, 5>;

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

template<uint8_t COMMAND, typename T, auto CONVERTER>
class RequestResponse_THRESHOLD_MINMAX_TYPE : public RequestResponseCommand<COMMAND> {
public:
    T thresholdMax1 = T(0);
    T thresholdMax2 = T(0);
    T thresholdMin1 = T(0);
    T thresholdMin2 = T(0);
protected:
    using RequestResponseCommand<COMMAND>::setValid;
    bool processFrame(const RequestResponseFrame& frame, const size_t) override {
        thresholdMax1 = CONVERTER(frame.getUInt16(0));
        thresholdMax2 = CONVERTER(frame.getUInt16(2));
        thresholdMin1 = CONVERTER(frame.getUInt16(4));
        thresholdMin2 = CONVERTER(frame.getUInt16(6));
        setValid();
        return true;
    }
};

using RequestResponse_CELL_VOLTAGES_THRESHOLDS = RequestResponse_THRESHOLD_MINMAX_TYPE<0x59, float,
                                                                                       [](uint16_t v) -> float {
                                                                                           return static_cast<float>(v) / 1000.0f;
                                                                                       }>;

using RequestResponse_PACK_VOLTAGES_THRESHOLDS = RequestResponse_THRESHOLD_MINMAX_TYPE<0x5A, float,
                                                                                       [](uint16_t v) -> float {
                                                                                           return static_cast<float>(v) / 10.0f;
                                                                                       }>;

using RequestResponse_PACK_CURRENTS_THRESHOLDS = RequestResponse_THRESHOLD_MINMAX_TYPE<0x5B, float,
                                                                                       [](uint16_t v) -> float {
                                                                                           return (static_cast<float>(v) - 30000) / 10.0f;
                                                                                       }>;

class RequestResponse_PACK_TEMPERATURE_THRESHOLDS : public RequestResponseCommand<0x5C> {
public:
    int8_t chargeTemperatureMax1C = 0;
    int8_t chargeTemperatureMax2C = 0;
    int8_t chargeTemperatureMin1C = 0;
    int8_t chargeTemperatureMin2C = 0;
    int8_t dischargeTemperatureMax1C = 0;
    int8_t dischargeTemperatureMax2C = 0;
    int8_t dischargeTemperatureMin1C = 0;
    int8_t dischargeTemperatureMin2C = 0;
protected:
    bool processFrame(const RequestResponseFrame& frame, const size_t) override {
        chargeTemperatureMax1C = frame.getUInt8(0) - 40;
        chargeTemperatureMax2C = frame.getUInt8(1) - 40;
        chargeTemperatureMin1C = frame.getUInt8(2) - 40;
        chargeTemperatureMin2C = frame.getUInt8(3) - 40;
        dischargeTemperatureMax1C = frame.getUInt8(4) - 40;
        dischargeTemperatureMax2C = frame.getUInt8(5) - 40;
        dischargeTemperatureMin1C = frame.getUInt8(6) - 40;
        dischargeTemperatureMin2C = frame.getUInt8(7) - 40;
        setValid();
        return true;
    }
};

using RequestResponse_PACK_SOC_THRESHOLDS = RequestResponse_THRESHOLD_MINMAX_TYPE<0x5D, float,
                                                                                  [](uint16_t v) -> float {
                                                                                      return static_cast<float>(v) / 10.0f;
                                                                                  }>;

class RequestResponse_CELL_SENSORS_THRESHOLDS : public RequestResponseCommand<0x5E> {
public:
    float cellVoltageDiff1V = 0.0f;
    float cellVoltageDiff2V = 0.0f;
    int8_t cellTemperatureDiff1C = 0;
    int8_t cellTemperatureDiff2C = 0;
protected:
    bool processFrame(const RequestResponseFrame& frame, const size_t) override {
        cellVoltageDiff1V = static_cast<float>(frame.getUInt16(0)) / 1000.0f;
        cellVoltageDiff2V = static_cast<float>(frame.getUInt16(2)) / 1000.0f;
        cellTemperatureDiff1C = frame.getUInt8(4) - 40;
        cellTemperatureDiff2C = frame.getUInt8(5) - 40;
        setValid();
        return true;
    }
};

class RequestResponse_CELL_BALANCES_THRESHOLDS : public RequestResponseCommand<0x5F> {
public:
    float cellVoltageEnableThreshold = 0.0f;
    float cellVoltageAcceptableDifferential = 0.0f;
protected:
    bool processFrame(const RequestResponseFrame& frame, const size_t) override {
        cellVoltageEnableThreshold = static_cast<float>(frame.getUInt16(0)) / 1000.0f;
        cellVoltageAcceptableDifferential = static_cast<float>(frame.getUInt16(2)) / 1000.0f;
        setValid();
        return true;
    }
};

class RequestResponse_PACK_SHORTCIRCUIT_THRESHOLDS : public RequestResponseCommand<0x60> {
public:
    float shortCircuitShutdownA = 0.0f;
    float shortCircuitSamplingR = 0.0f;
protected:
    bool processFrame(const RequestResponseFrame& frame, const size_t) override {
        shortCircuitShutdownA = static_cast<float>(frame.getUInt16(0));
        shortCircuitSamplingR = static_cast<float>(frame.getUInt16(2)) / 1000.0f;
        setValid();
        return true;
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class RequestResponse_BMS_RTC : public RequestResponseCommand<0x61> {    // XXX TBC
public:
    uint32_t dateTime1 = 0;
    uint32_t dateTime2 = 0;
protected:
    bool processFrame(const RequestResponseFrame& frame, const size_t) override {
        dateTime1 = frame.getUInt32(0);    // XXX ??
        dateTime2 = frame.getUInt32(4);    // XXX ??
        setValid();
        return true;
    }
};

using RequestResponse_BMS_SOFTWARE_VERSION = RequestResponse_TYPE_STRING<0x62, 2>;

using RequestResponse_BMS_HARDWARE_VERSION = RequestResponse_TYPE_STRING<0x63, 2>;

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class RequestResponse_PACK_STATUS : public RequestResponseCommand<0x90> {
public:
    float packVoltage = 0.0f;
    float packCurrent = 0.0f;
    float packSOC = 0.0f;
protected:
    bool processFrame(const RequestResponseFrame& frame, const size_t) override {
        packVoltage = static_cast<float>(frame.getUInt16(0)) / 10.0f;
        packCurrent = (static_cast<float>(frame.getUInt16(4)) - 30000) / 10.0f;
        packSOC = static_cast<float>(frame.getUInt16(6)) / 10.0f;
        setValid();
        return true;
    }
};

template<uint8_t COMMAND, typename T, int S, auto CONVERTER>
class RequestResponse_TYPE_VALUE_MINMAX : public RequestResponseCommand<COMMAND> {
public:
    T valueMax = T(0);
    uint8_t numberMax = 0;
    T valueMin = T(0);
    uint8_t numberMin = 0;
protected:
    using RequestResponseCommand<COMMAND>::setValid;
    bool processFrame(const RequestResponseFrame& frame, const size_t) override {
        valueMax = CONVERTER(frame, 0);
        numberMax = frame.getUInt8(S);
        valueMin = CONVERTER(frame, S + 1);
        numberMin = frame.getUInt8(S + 1 + S);
        setValid();
        return true;
    }
};

using RequestResponse_CELL_VOLTAGES_MINMAX = RequestResponse_TYPE_VALUE_MINMAX<0x91, float, 2,
                                                                               [](const RequestResponseFrame& frame, size_t offset) -> float {
                                                                                   return static_cast<float>(frame.getUInt16(offset)) / 1000.0f;
                                                                               }>;

using RequestResponse_CELL_TEMPERATURES_MINMAX = RequestResponse_TYPE_VALUE_MINMAX<0x92, int8_t, 1,
                                                                                   [](const RequestResponseFrame& frame, size_t offset) -> int8_t {
                                                                                       return frame.getUInt8(offset) - 40;
                                                                                   }>;

class RequestResponse_MOSFET_STATUS : public RequestResponseCommand<0x93> {
public:
    enum class ChargeStatus : uint8_t { Stationary = 0x00,
                                        Charge = 0x01,
                                        Discharge = 0x02 };
    ChargeStatus chargeStatus = ChargeStatus::Stationary;
    bool chargeMosState = false;
    bool dischargeMosState = false;
    uint8_t bmsLifeCycle = 0;
    float residualCapacityAh = 0;
protected:
    bool processFrame(const RequestResponseFrame& frame, const size_t) override {
        chargeStatus = static_cast<ChargeStatus>(frame.getUInt8(0));
        chargeMosState = frame.getUInt8(1);
        dischargeMosState = frame.getUInt8(2);
        bmsLifeCycle = frame.getUInt8(3);
        residualCapacityAh = static_cast<float>(frame.getUInt32(4)) / 1000.0f;
        setValid();
        return true;
    }
};

class RequestResponse_PACK_INFORMATION : public RequestResponseCommand<0x94> {
public:
    uint8_t numberOfCells = 0;
    uint8_t numberOfSensors = 0;
    bool chargerStatus = false;
    bool loadStatus = false;
    std::array<bool, 8> dioStates{};
    uint16_t cycles = 0;
protected:
    bool processFrame(const RequestResponseFrame& frame, const size_t) override {
        numberOfCells = frame.getUInt8(0);
        numberOfSensors = frame.getUInt8(1);
        chargerStatus = frame.getUInt8(2);
        loadStatus = frame.getUInt8(3);
        for (size_t i = 0; i < 8; i++)
            dioStates[i] = frame.getBit(4, i);
        cycles = frame.getUInt16(5);
        setValid();
        return true;
    }
};

template<uint8_t COMMAND, typename T, size_t ITEMS_MAX, size_t ITEMS_PER_FRAME, auto CONVERTER>
class RequestResponse_TYPE_ARRAY : public RequestResponseCommand<COMMAND> {
public:
    std::vector<T> values;
    bool setCount(const size_t count) {
        if (count > 0 && count < ITEMS_MAX) {
            values.resize(count);
            setResponseCount((count / ITEMS_PER_FRAME) + 1);
            return true;
        }
        return false;
    }
    bool isRequestable() const override {
        return !values.empty();
    }
protected:
    using RequestResponseCommand<COMMAND>::setValid;
    using RequestResponseCommand<COMMAND>::setResponseCount;
    bool processFrame(const RequestResponseFrame& frame, const size_t frameNum) override {
        if (frame.getUInt8(0) != frameNum || frame.getUInt8(0) > (values.size() / ITEMS_PER_FRAME)) return false;
        for (size_t i = 0; i < ITEMS_PER_FRAME && ((frameNum * ITEMS_PER_FRAME) + i) < values.size(); i++)
            values[(frameNum * ITEMS_PER_FRAME) + i] = CONVERTER(frame, i);
        if (frameNum == (values.size() / ITEMS_PER_FRAME) + 1)
            setValid();
        return true;
    }
};

using RequestResponse_CELL_VOLTAGES = RequestResponse_TYPE_ARRAY<0x95, float, 48, 3,
                                                                 [](const RequestResponseFrame& frame, size_t i) -> float {
                                                                     return static_cast<float>(frame.getUInt16(1 + i * 2)) / 1000.0f;
                                                                 }>;

using RequestResponse_CELL_TEMPERATURES = RequestResponse_TYPE_ARRAY<0x96, int8_t, 16, 7,
                                                                     [](const RequestResponseFrame& frame, size_t i) -> int8_t {
                                                                         return frame.getUInt8(1 + i) - 40;
                                                                     }>;

using RequestResponse_CELL_BALANCES = RequestResponse_TYPE_ARRAY<0x97, bool, 48, 48,
                                                                 [](const RequestResponseFrame& frame, size_t i) -> bool {
                                                                     return frame.getBit(i);
                                                                 }>;

class RequestResponse_FAILURE_STATUS : public RequestResponseCommand<0x98> {
    static constexpr size_t NUM_FAILURE_BYTES = 7;
    static constexpr size_t NUM_FAILURE_CODES = NUM_FAILURE_BYTES * 8;
public:
    bool failureShow = false;
    std::bitset<NUM_FAILURE_CODES> failureBits;
    size_t failureCount = 0;
    size_t getFailureList(const char** output, const size_t maxFailures) const {
        size_t count = 0;
        for (size_t i = 0; i < NUM_FAILURE_CODES && count < maxFailures; ++i)
            if (failureBits[i])
                output[count++] = FAILURE_DESCRIPTIONS[i];
        return count;
    }
protected:
    bool processFrame(const RequestResponseFrame& frame, const size_t) override {
        failureCount = 0;
        for (size_t index = 0; index < NUM_FAILURE_CODES; ++index)
            if ((failureBits[index] = frame.getBit(index >> 3, index & 0x07)))
                failureCount++;
        failureShow = frame.getUInt8(7) == 0x03;
        setValid();
        return true;
    }
private:
    static constexpr std::array<const char*, NUM_FAILURE_CODES> FAILURE_DESCRIPTIONS = {
        "Cell voltage high level 1", "Cell voltage high level 2", "Cell voltage low level 1", "Cell voltage low level 2", "Pack voltage high level 1", "Pack voltage high level 2", "Pack voltage low level 1", "Pack voltage low level 2",                                                                // Byte 0
        "Charge temperature high level 1", "Charge temperature high level 2", "Charge temperature low level 1", "Charge temperature low level 2", "Discharge temperature high level 1", "Discharge temperature high level 2", "Discharge temperature low level 1", "Discharge temperature low level 2",    // Byte 1
        "Charge current high level 1", "Charge current high level 2", "Discharge current high level 1", "Discharge current high level 2", "SOC high level 1", "SOC high level 2", "SOC low level 1", "SOC low level 2",                                                                                    // Byte 2
        "Cell voltage difference high level 1", "Cell voltage difference high level 2", "Cell temperature difference high level 1", "Cell temperature difference high level 2", "Reserved 3-4", "Reserved 3-5", "Reserved 3-6", "Reserved 3-7",                                                            // Byte 3
        "Charge MOSFET temperature high", "Discharge MOSFET temperature high", "Charge MOSFET temperature sensor fail", "Discharge MOSFET temperature sensor fail", "Charge MOSFET adhesion fail", "Discharge MOSFET adhesion fail", "Charge MOSFET breaker fail", "Discharge MOSFET breaker fail",        // Byte 4
        "AFE acquisition module fail", "Voltage sensor fail", "Temperature sensor fail", "EEPROM storage fail", "RTC fail", "Precharge fail", "Vehicle communication fail", "Network communication fail",                                                                                                  // Byte 5
        "Current sensor module fail", "Voltage sensor module fail", "Short circuit protection fail", "Low voltage no charging", "MOS GPS or soft switch MOS off", "Reserved 6-5", "Reserved 6-6", "Reserved 6-7",                                                                                          // Byte 6
    };
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

template<uint8_t COMMAND>
class RequestResponse_TYPE_ONCE : public RequestResponseCommand<COMMAND> {
protected:
    using RequestResponseCommand<COMMAND>::setValid;
    bool processFrame(const RequestResponseFrame& frame, const size_t) override {
        setValid();
        return true;
    }
};

template<uint8_t COMMAND>
class RequestResponse_TYPE_ONOFF : public RequestResponseCommand<COMMAND> {
public:
    enum class Command : uint8_t { Off = 0x00,
                                   On = 0x01 };
    RequestResponseFrame prepareRequest(const Command command) {
        return RequestResponse::prepareRequest().setUInt8(4, static_cast<uint8_t>(command)).finalize();
    }
protected:
    using RequestResponseCommand<COMMAND>::setValid;
    bool processFrame(const RequestResponseFrame& frame, const size_t) override {
        setValid();
        return true;
    }
};

using RequestResponse_BMS_RESET = RequestResponse_TYPE_ONCE<0x00>;

using RequestResponse_MOSFET_DISCHARGE = RequestResponse_TYPE_ONOFF<0xD9>;

using RequestResponse_MOSFET_CHARGE = RequestResponse_TYPE_ONOFF<0xDA>;

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class RequestResponseManager {
public:

    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void onValidResponse(RequestResponse* response) = 0;
    };

    void registerListener(Listener* listener) {
        _listeners.push_back(listener);
    }
    void unregisterListener(Listener* listener) {
        auto pos = std::find(_listeners.begin(), _listeners.end(), listener);
        if (pos != _listeners.end())
            _listeners.erase(pos);
    }

    bool receiveFrame(const RequestResponseFrame& frame) {
        auto it = _requestsMap.find(frame.getCommand());
        if (it != _requestsMap.end() && it->second->processResponse(frame)) {
            if (it->second->isValid())
                notifyListeners(it->second);
            return true;
        }
        return false;
    }

    explicit RequestResponseManager(const std::vector<RequestResponse*>& requests)
        : _requests(requests) {
        for (auto request : _requests)
            _requestsMap[request->getCommand()] = request;
    }

private:
    void notifyListeners(RequestResponse* response) {
        for (auto listener : _listeners)
            listener->onValidResponse(response);
    }

    const std::vector<RequestResponse*> _requests;
    std::map<uint8_t, RequestResponse*> _requestsMap;
    std::vector<Listener*> _listeners;
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class DalyBmsSerialConnector : public RequestResponseFrame::Receiver {
public:
    explicit DalyBmsSerialConnector(HardwareSerial& serial, const RequestResponseFrame::Receiver::Handler& handler)
        : RequestResponseFrame::Receiver(handler), _serial(serial) {}
    void begin(const unsigned long baud = 9600) {
        _serial.begin(baud);
    }
protected:
    bool getByte(uint8_t* byte) override {
        if (_serial.available() > 0) {
            *byte = _serial.read();
            return true;
        }
        return false;
    }
    bool sendBytes(const uint8_t* data, const size_t size) override {
        _serial.write(data, size);
        return true;
    }
private:
    HardwareSerial& _serial;
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class DalyBmsInterface : protected RequestResponseManager::Listener {
public:
    struct Information {    // static, unofficial
        RequestResponse_BMS_HARDWARE_CONFIG hardware_config;
        RequestResponse_BMS_HARDWARE_VERSION hardware_version;
        RequestResponse_BMS_FIRMWARE_INDEX firmware_index;
        RequestResponse_BMS_SOFTWARE_VERSION software_version;
        RequestResponse_BATTERY_RATINGS battery_ratings;
        RequestResponse_BATTERY_CODE battery_code;
        RequestResponse_BATTERY_INFO battery_info;
        RequestResponse_BATTERY_STAT battery_stat;
        RequestResponse_BMS_RTC rtc;
        void forEach(const std::function<void(RequestResponse*)>& fn) {
            fn(&hardware_config);
            fn(&hardware_version);
            fn(&firmware_index);
            fn(&software_version);
            fn(&battery_ratings);
            fn(&battery_code);
            fn(&battery_info);
            fn(&battery_stat);
            fn(&rtc);
        }
    } information;
    struct Thresholds {    // static, unofficial
        RequestResponse_PACK_VOLTAGES_THRESHOLDS pack_voltages;
        RequestResponse_PACK_CURRENTS_THRESHOLDS pack_currents;
        RequestResponse_PACK_TEMPERATURE_THRESHOLDS pack_temperatures;
        RequestResponse_PACK_SOC_THRESHOLDS pack_soc;
        RequestResponse_CELL_VOLTAGES_THRESHOLDS cell_voltages;
        RequestResponse_CELL_SENSORS_THRESHOLDS cell_sensors;
        RequestResponse_CELL_BALANCES_THRESHOLDS cell_balances;
        RequestResponse_PACK_SHORTCIRCUIT_THRESHOLDS pack_shortcircuit;
        void forEach(const std::function<void(RequestResponse*)>& fn) {
            fn(&pack_voltages);
            fn(&pack_currents);
            fn(&pack_temperatures);
            fn(&pack_soc);
            fn(&cell_voltages);
            fn(&cell_sensors);
            fn(&cell_balances);
            fn(&pack_shortcircuit);
        }
    } thresholds;
    struct Status {
        RequestResponse_PACK_STATUS pack;
        RequestResponse_CELL_VOLTAGES_MINMAX cell_voltages;
        RequestResponse_CELL_TEMPERATURES_MINMAX cell_temperatures;
        RequestResponse_MOSFET_STATUS fets;
        RequestResponse_PACK_INFORMATION info;
        RequestResponse_FAILURE_STATUS failures;
        void forEach(const std::function<void(RequestResponse*)>& fn) {
            fn(&pack);
            fn(&cell_voltages);
            fn(&cell_temperatures);
            fn(&fets);
            fn(&info);
            fn(&failures);
        }
    } status;
    struct Diagnostics {
        RequestResponse_CELL_VOLTAGES voltages;
        RequestResponse_CELL_TEMPERATURES temperatures;
        RequestResponse_CELL_BALANCES balances;
        void forEach(const std::function<void(RequestResponse*)>& fn) {
            fn(&voltages);
            fn(&temperatures);
            fn(&balances);
        }
    } diagnostics;
    struct Commands {    // unofficial
        RequestResponse_BMS_RESET reset;
        RequestResponse_MOSFET_DISCHARGE discharge;
        RequestResponse_MOSFET_CHARGE charge;
    } commands;

    explicit DalyBmsInterface(HardwareSerial& serial)
        : thresholds{}, status{}, diagnostics{}, commands{},
          _connector(serial, [this](const RequestResponseFrame& frame) {
              _manager.receiveFrame(frame);
          }),
          _manager({ &information.hardware_config, &information.hardware_version, &information.firmware_index, &information.software_version, &information.battery_ratings, &information.battery_code, &information.battery_info, &information.battery_stat, &information.rtc, &thresholds.pack_voltages, &thresholds.pack_currents, &thresholds.pack_temperatures, &thresholds.pack_soc, &thresholds.cell_voltages, &thresholds.cell_sensors, &thresholds.cell_balances, &thresholds.pack_shortcircuit, &status.pack, &status.cell_voltages, &status.cell_temperatures, &status.fets, &status.info, &status.failures, &diagnostics.voltages, &diagnostics.temperatures, &diagnostics.balances, &commands.reset, &commands.discharge, &commands.charge }) {
        _manager.registerListener(this);
    }

    void begin() {
        _connector.begin();
    }
    void loop() {
        _connector.process();
    }

    void issue(RequestResponse* request) {
        if (request->isRequestable())
            _connector.write(request->prepareRequest());
    }
    void requestStatus() {
        status.forEach([this](auto* r) {
            issue(r);
        });
    }
    void requestDiagnostics() {
        diagnostics.forEach([this](auto* r) {
            issue(r);
        });
    }
    void requestInitial() {
        information.forEach([this](auto* r) {
            issue(r);
        });
        thresholds.forEach([this](auto* r) {
            issue(r);
        });
        requestStatus();
        requestDiagnostics();
    }

protected:
    void onValidResponse(RequestResponse* response) override {
        if (response->getCommand() == status.info) {
            diagnostics.voltages.setCount(status.info.numberOfCells);
            diagnostics.temperatures.setCount(status.info.numberOfSensors);
            diagnostics.balances.setCount(status.info.numberOfCells);
            _manager.unregisterListener(this);
        }
    }

private:
    DalyBmsSerialConnector _connector;
    RequestResponseManager _manager;
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
