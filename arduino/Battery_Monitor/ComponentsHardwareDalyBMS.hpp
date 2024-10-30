
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include "HardwareSerial.h"

#include <cstdint>
#include <array>
#include <map>
#include <bitset>

namespace daly_bms {

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

    String toString() const {
        String result;
        result.reserve(Constants::SIZE_FRAME * 3);
        static const char HEX_CHARS[] = "0123456789ABCDEF";
        for (size_t i = 0; i < Constants::SIZE_FRAME; i++) {
            if (i > 0) result += ' ';
            result += HEX_CHARS[_data[i] >> 4];
            result += HEX_CHARS[_data[i] & 0x0F];
        }
        return result;
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

    RequestResponseFrame_Receiver()
        : _handler([](const RequestResponseFrame&) {}), _listener([](const RequestResponseFrame&) {}) {}
    virtual ~RequestResponseFrame_Receiver() = default;

    RequestResponseFrame_Receiver& registerHandler(Handler handler) {
        _handler = std::move(handler);
        return *this;
    }
    RequestResponseFrame_Receiver& registerListener(Handler listener) {
        _listener = std::move(listener);
        return *this;
    }

    virtual void begin() {}
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
        _listener(frame);
        sendBytes(frame.data(), frame.size());
        process();
    }

private:
    Handler _handler, _listener;
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
    bool setValid(const bool v = true) {
        return (_valid = v);
    }
    virtual bool processFrame(const RequestResponseFrame& frame, const size_t number) {
        return setValid();
    }
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
            return setValid();
        }
        return true;
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

typedef struct {
    uint8_t year;    // 0-99 + 2000
    uint8_t month;
    uint8_t day;
    String toString() const {
        char buffer[9];
        snprintf(buffer, sizeof(buffer), "20%02d%02d%02d", year, month, day);
        return String(buffer);
    }
} FrameTypeDateYMD;

class FrameContentDecoder {
public:
    static bool decode_Percent_d(const RequestResponseFrame& frame, size_t offset, float* value) {
        *value = static_cast<float>(frame.getUInt16(offset)) / 10.0f;
        return true;
    }
    static bool decode_Voltage_d(const RequestResponseFrame& frame, size_t offset, float* value) {
        *value = static_cast<float>(frame.getUInt16(offset)) / 10.0f;
        return true;
    }
    static bool decode_Voltage_m(const RequestResponseFrame& frame, size_t offset, float* value) {
        *value = static_cast<float>(frame.getUInt16(offset)) / 1000.0f;
        return true;
    }
    static bool decode_Current_d(const RequestResponseFrame& frame, size_t offset, float* value) {
        *value = (static_cast<float>(frame.getUInt16(offset)) - 30000) / 10.0f;
        return true;
    }
    static bool decode_Temperature(const RequestResponseFrame& frame, size_t offset, int8_t* value) {
        *value = frame.getUInt8(offset) - 40;
        return true;
    }
    static bool decode_Time_s(const RequestResponseFrame& frame, size_t offset, uint16_t* value) {
        *value = static_cast<uint16_t>(frame.getUInt8(offset)) * 60;
        return true;
    }
    static bool decode_BitNoFrameNum(const RequestResponseFrame& frame, size_t offset, uint8_t* value) {
        *value = frame.getBit(offset - 1);    // will be offset by 1 due to assumed frame num
        return true;
    }

    template<typename T>
        requires std::is_enum_v<T> && std::is_same_v<std::underlying_type_t<T>, uint8_t>
    static bool decode(const RequestResponseFrame& frame, size_t offset, T* value) {
        *value = static_cast<T>(frame.getUInt8(offset));
        return true;
    }
    static bool decode(const RequestResponseFrame& frame, size_t offset, std::array<bool, 8>* value) {
        for (size_t i = 0; i < 8; i++)
            (*value)[i] = frame.getBit(offset, i);
        return true;
    }
    static bool decode(const RequestResponseFrame& frame, size_t offset, FrameTypeDateYMD* value) {
        *value = { .year = frame.getUInt8(offset + 0), .month = frame.getUInt8(offset + 1), .day = frame.getUInt8(offset + 2) };
        return true;
    }
    static bool decode(const RequestResponseFrame& frame, size_t offset, bool* value) {
        *value = frame.getUInt8(offset);
        return true;
    }
    template<size_t N>
    static bool decode(const RequestResponseFrame& frame, size_t offset, std::array<uint8_t, N>* values) {
        for (size_t i = 0; i < N; i++)
            (*values)[i] = frame.getUInt8(offset + i);
        return true;
    }
    static bool decode(const RequestResponseFrame& frame, size_t offset, uint8_t* value) {
        *value = frame.getUInt8(offset);
        return true;
    }
    static bool decode(const RequestResponseFrame& frame, size_t offset, uint16_t* value) {
        *value = frame.getUInt16(offset);
        return true;
    }
    static bool decode(const RequestResponseFrame& frame, size_t offset, float* value, float divisor = 1.0f) {
        *value = static_cast<float>(frame.getUInt16(offset)) / divisor;
        return true;
    }
    static bool decode(const RequestResponseFrame& frame, size_t offset, double* value, double divisor = 1.0) {
        *value = static_cast<double>(frame.getUInt32(offset)) / divisor;
        return true;
    }
};

class RequestResponse_BATTERY_RATINGS : public RequestResponseCommand<0x50> {
public:
    double packCapacityAh = 0.0f;
    double nominalCellVoltageV = 0.0f;
protected:
    bool processFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid(
            FrameContentDecoder::decode(frame, 0, &packCapacityAh, 1000.0) && FrameContentDecoder::decode(frame, 4, &nominalCellVoltageV, 1000.0));
    }
};

class RequestResponse_BMS_HARDWARE_CONFIG : public RequestResponseCommand<0x51> {
public:
    uint8_t boardCount = 0;
    std::array<uint8_t, 3> cellCounts{}, sensorCounts{};
protected:
    bool processFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid(
            FrameContentDecoder::decode(frame, 0, &boardCount) && FrameContentDecoder::decode<3>(frame, 1, &cellCounts) && FrameContentDecoder::decode<3>(frame, 4, &sensorCounts));
    }
};

class RequestResponse_BATTERY_STAT : public RequestResponseCommand<0x52> {    // XXX TBC
public:
    double cumulativeChargeAh = 0.0f;
    double cumulativeDischargeAh = 0.0f;
protected:
    bool processFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid(
            FrameContentDecoder::decode(frame, 0, &cumulativeChargeAh) && FrameContentDecoder::decode(frame, 4, &cumulativeDischargeAh));
    }
};

class RequestResponse_BATTERY_INFO : public RequestResponseCommand<0x53> {    // XXX TBC
public:
    uint8_t batteryOperationMode = 0;
    uint8_t batteryType = 0;
    FrameTypeDateYMD productionDate{};
    uint16_t automaticSleepSec = 0;
    uint8_t unknown1 = 0, unknown2 = 0;
protected:
    bool processFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid(
            FrameContentDecoder::decode(frame, 0, &batteryOperationMode) && FrameContentDecoder::decode(frame, 1, &batteryType) && FrameContentDecoder::decode(frame, 2, &productionDate) && FrameContentDecoder::decode_Time_s(frame, 5, &automaticSleepSec) && FrameContentDecoder::decode(frame, 6, &unknown1) && FrameContentDecoder::decode(frame, 7, &unknown2));
    }
};

using RequestResponse_BMS_FIRMWARE_INDEX = RequestResponse_TYPE_STRING<0x54, 1>;

using RequestResponse_BATTERY_CODE = RequestResponse_TYPE_STRING<0x57, 5>;

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

template<uint8_t COMMAND, typename T, int S, auto DECODER>
class RequestResponse_THRESHOLD_MINMAX_TYPE : public RequestResponseCommand<COMMAND> {
public:
    T thresholdMax1 = T(0), thresholdMax2 = T(0);
    T thresholdMin1 = T(0), thresholdMin2 = T(0);
protected:
    using RequestResponseCommand<COMMAND>::setValid;
    bool processFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid(
            DECODER(frame, S * 0, &thresholdMax1) && DECODER(frame, S * 1, &thresholdMax2) && DECODER(frame, S * 2, &thresholdMin1) && DECODER(frame, S * 3, &thresholdMin2));
    }
};

using RequestResponse_CELL_VOLTAGES_THRESHOLDS = RequestResponse_THRESHOLD_MINMAX_TYPE<0x59, float, 2, FrameContentDecoder::decode_Voltage_m>;
using RequestResponse_PACK_VOLTAGES_THRESHOLDS = RequestResponse_THRESHOLD_MINMAX_TYPE<0x5A, float, 2, FrameContentDecoder::decode_Voltage_d>;
using RequestResponse_PACK_CURRENTS_THRESHOLDS = RequestResponse_THRESHOLD_MINMAX_TYPE<0x5B, float, 2, FrameContentDecoder::decode_Current_d>;

class RequestResponse_PACK_TEMPERATURE_THRESHOLDS : public RequestResponseCommand<0x5C> {
public:
    int8_t chargeTemperatureMax1C = 0, chargeTemperatureMax2C = 0;
    int8_t chargeTemperatureMin1C = 0, chargeTemperatureMin2C = 0;
    int8_t dischargeTemperatureMax1C = 0, dischargeTemperatureMax2C = 0;
    int8_t dischargeTemperatureMin1C = 0, dischargeTemperatureMin2C = 0;
protected:
    bool processFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid(
            FrameContentDecoder::decode_Temperature(frame, 0, &chargeTemperatureMax1C) && FrameContentDecoder::decode_Temperature(frame, 1, &chargeTemperatureMax2C) && FrameContentDecoder::decode_Temperature(frame, 2, &chargeTemperatureMin1C) && FrameContentDecoder::decode_Temperature(frame, 3, &chargeTemperatureMin2C) && FrameContentDecoder::decode_Temperature(frame, 4, &dischargeTemperatureMax1C) && FrameContentDecoder::decode_Temperature(frame, 5, &dischargeTemperatureMax2C) && FrameContentDecoder::decode_Temperature(frame, 6, &dischargeTemperatureMin1C) && FrameContentDecoder::decode_Temperature(frame, 7, &dischargeTemperatureMin2C));
    }
};

using RequestResponse_PACK_SOC_THRESHOLDS = RequestResponse_THRESHOLD_MINMAX_TYPE<0x5D, float, 2, FrameContentDecoder::decode_Percent_d>;

class RequestResponse_CELL_SENSORS_THRESHOLDS : public RequestResponseCommand<0x5E> {
public:
    float cellVoltageDiff1V = 0.0f, cellVoltageDiff2V = 0.0f;
    int8_t cellTemperatureDiff1C = 0, cellTemperatureDiff2C = 0;
protected:
    bool processFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid(
            FrameContentDecoder::decode_Voltage_m(frame, 0, &cellVoltageDiff1V) && FrameContentDecoder::decode_Voltage_m(frame, 2, &cellVoltageDiff2V) && FrameContentDecoder::decode_Temperature(frame, 4, &cellTemperatureDiff1C) && FrameContentDecoder::decode_Temperature(frame, 5, &cellTemperatureDiff2C));
    }
};

class RequestResponse_CELL_BALANCES_THRESHOLDS : public RequestResponseCommand<0x5F> {
public:
    float cellVoltageEnableThreshold = 0.0f;
    float cellVoltageAcceptableDifferential = 0.0f;
protected:
    bool processFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid(
            FrameContentDecoder::decode_Voltage_m(frame, 0, &cellVoltageEnableThreshold) && FrameContentDecoder::decode_Voltage_m(frame, 2, &cellVoltageAcceptableDifferential));
    }
};

class RequestResponse_PACK_SHORTCIRCUIT_THRESHOLDS : public RequestResponseCommand<0x60> {
public:
    float shortCircuitShutdownA = 0.0f;
    float shortCircuitSamplingR = 0.0f;
protected:
    bool processFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid(
            FrameContentDecoder::decode(frame, 0, &shortCircuitShutdownA) && FrameContentDecoder::decode(frame, 2, &shortCircuitSamplingR, 1000.0f));
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
        return setValid();
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
        return setValid(
            FrameContentDecoder::decode_Voltage_d(frame, 0, &packVoltage) && FrameContentDecoder::decode_Current_d(frame, 4, &packCurrent) && FrameContentDecoder::decode_Percent_d(frame, 6, &packSOC));
    }
};

template<uint8_t COMMAND, typename T, int S, auto DECODER>
class RequestResponse_TYPE_VALUE_MINMAX : public RequestResponseCommand<COMMAND> {
public:
    T valueMax = T(0);
    uint8_t numberMax = 0;
    T valueMin = T(0);
    uint8_t numberMin = 0;
protected:
    using RequestResponseCommand<COMMAND>::setValid;
    bool processFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid(
            DECODER(frame, 0, &valueMax) && FrameContentDecoder::decode(frame, S, &numberMax) && DECODER(frame, S + 1, &valueMin) && FrameContentDecoder::decode(frame, S + 1 + S, &numberMin));
    }
};

using RequestResponse_CELL_VOLTAGES_MINMAX = RequestResponse_TYPE_VALUE_MINMAX<0x91, float, 2, FrameContentDecoder::decode_Voltage_m>;

using RequestResponse_CELL_TEMPERATURES_MINMAX = RequestResponse_TYPE_VALUE_MINMAX<0x92, int8_t, 1, FrameContentDecoder::decode_Temperature>;

class RequestResponse_MOSFET_STATUS : public RequestResponseCommand<0x93> {
public:
    enum class ChargeStatus : uint8_t { Stationary = 0x00,
                                        Charge = 0x01,
                                        Discharge = 0x02 };
    ChargeStatus chargeStatus = ChargeStatus::Stationary;
    bool chargeMosState = false;
    bool dischargeMosState = false;
    uint8_t bmsLifeCycle = 0;
    double residualCapacityAh = 0;
protected:
    bool processFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid(
            FrameContentDecoder::decode(frame, 0, &chargeStatus) && FrameContentDecoder::decode(frame, 1, &chargeMosState) && FrameContentDecoder::decode(frame, 2, &dischargeMosState) && FrameContentDecoder::decode(frame, 3, &bmsLifeCycle) && FrameContentDecoder::decode(frame, 4, &residualCapacityAh, 1000.0));
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
        return setValid(
            FrameContentDecoder::decode(frame, 0, &numberOfCells) && FrameContentDecoder::decode(frame, 1, &numberOfSensors) && FrameContentDecoder::decode(frame, 2, &chargerStatus) && FrameContentDecoder::decode(frame, 3, &loadStatus) && FrameContentDecoder::decode(frame, 4, &dioStates) && FrameContentDecoder::decode(frame, 5, &cycles));
    }
};

template<uint8_t COMMAND, typename T, int S, size_t ITEMS_MAX, size_t ITEMS_PER_FRAME, auto DECODER>
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
            if (!DECODER(frame, 1 + i * S, &values[(frameNum * ITEMS_PER_FRAME) + i]))
                return false;    // XXX underlying will pass through the next frame though ...
        if (frameNum == (values.size() / ITEMS_PER_FRAME) + 1)
            return setValid();
        return true;
    }
};

using RequestResponse_CELL_VOLTAGES = RequestResponse_TYPE_ARRAY<0x95, float, 2, 48, 3, FrameContentDecoder::decode_Voltage_m>;

using RequestResponse_CELL_TEMPERATURES = RequestResponse_TYPE_ARRAY<0x96, int8_t, 1, 16, 7, FrameContentDecoder::decode_Temperature>;

using RequestResponse_CELL_BALANCES = RequestResponse_TYPE_ARRAY<0x97, uint8_t, 1, 48, 48, FrameContentDecoder::decode_BitNoFrameNum>;

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
            if ((failureBits[index] = frame.getBit(index)))
                failureCount++;
        failureShow = frame.getUInt8(7) == 0x03;
        return setValid();
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
class RequestResponse_TYPE_ONOFF : public RequestResponseCommand<COMMAND> {
public:
    enum class Setting : uint8_t { Off = 0x00,
                                   On = 0x01 };
    RequestResponseFrame prepareRequest(const Setting setting) {
        return RequestResponse::prepareRequest().setUInt8(4, static_cast<uint8_t>(setting)).finalize();
    }
};

using RequestResponse_BMS_RESET = RequestResponseCommand<0x00>;

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

class Interface : protected RequestResponseManager::Listener {
public:

    using Connector = RequestResponseFrame::Receiver;

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

    explicit Interface(const int id, Connector& connector)
        : thresholds{}, status{}, diagnostics{}, commands{},
          _id(id),
          _connector(connector),
          _manager({ &information.hardware_config, &information.hardware_version, &information.firmware_index, &information.software_version, &information.battery_ratings, &information.battery_code, &information.battery_info, &information.battery_stat, &information.rtc,
                     &thresholds.pack_voltages, &thresholds.pack_currents, &thresholds.pack_temperatures, &thresholds.pack_soc, &thresholds.cell_voltages, &thresholds.cell_sensors, &thresholds.cell_balances, &thresholds.pack_shortcircuit,
                     &status.pack, &status.cell_voltages, &status.cell_temperatures, &status.fets, &status.info, &status.failures,
                     &diagnostics.voltages, &diagnostics.temperatures, &diagnostics.balances,
                     &commands.reset, &commands.discharge, &commands.charge }) {
        _manager.registerListener(this);
        _connector.registerHandler([this](const RequestResponseFrame& frame) {
                      DEBUG_PRINTF("DalyBMS<%d>: recv: %s\n", _id, frame.toString().c_str());
                      _manager.receiveFrame(frame);
                  })
            .registerListener([this](const RequestResponseFrame& frame) {
                DEBUG_PRINTF("DalyBMS<%d>: send: %s\n", _id, frame.toString().c_str());
            });
    }

    void begin() {
        DEBUG_PRINTF("DalyBMS<%d>: begin\n", _id);
        _connector.begin();
        requestInitial();
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

    void dump() const {
        const auto is_valid = [](const auto& component, const char* name) {
            if (!component.isValid()) {
                DEBUG_PRINTF("  %s: <Not valid>\n", name);
                return false;
            }
            DEBUG_PRINTF("  %s: ", name);
            return true;
        };

        DEBUG_PRINTF("information:\n");
        if (is_valid(information.hardware_config, "hardware_config"))
            DEBUG_PRINTF("boards=%d, cells=%d,%d,%d, sensors=%d,%d,%d\n",
                         information.hardware_config.boardCount,
                         information.hardware_config.cellCounts[0],
                         information.hardware_config.cellCounts[1],
                         information.hardware_config.cellCounts[2],
                         information.hardware_config.sensorCounts[0],
                         information.hardware_config.sensorCounts[1],
                         information.hardware_config.sensorCounts[2]);
        if (is_valid(information.hardware_version, "hardware_version"))
            DEBUG_PRINTF("%s\n", information.hardware_version.string.c_str());
        if (is_valid(information.firmware_index, "firmware_index"))
            DEBUG_PRINTF("%s\n", information.firmware_index.string.c_str());
        if (is_valid(information.software_version, "software_version"))
            DEBUG_PRINTF("%s\n", information.software_version.string.c_str());
        if (is_valid(information.battery_ratings, "battery_ratings"))
            DEBUG_PRINTF("capacity=%.1fAh, nominal=%.1fV\n",
                         information.battery_ratings.packCapacityAh,
                         information.battery_ratings.nominalCellVoltageV);
        if (is_valid(information.battery_code, "battery_code"))
            DEBUG_PRINTF("%s\n", information.battery_code.string.c_str());
        if (is_valid(information.battery_info, "battery_info"))
            DEBUG_PRINTF("mode=%d, type=%d, date=%s, sleep=%d\n",
                         information.battery_info.batteryOperationMode,
                         information.battery_info.batteryType,
                         information.battery_info.productionDate.toString().c_str(),
                         information.battery_info.automaticSleepSec);
        if (is_valid(information.battery_stat, "battery_stat"))
            DEBUG_PRINTF("charge=%.1fAh, discharge=%.1fAh\n",
                         information.battery_stat.cumulativeChargeAh,
                         information.battery_stat.cumulativeDischargeAh);
        if (is_valid(information.rtc, "rtc"))
            DEBUG_PRINTF("dt1=%u, dt2=%u\n",
                         information.rtc.dateTime1,
                         information.rtc.dateTime2);

        DEBUG_PRINTF("\nthresholds:\n");
        if (is_valid(thresholds.pack_voltages, "pack_voltages"))
            DEBUG_PRINTF("max=%.1fV/%.1fV, min=%.1fV/%.1fV\n",
                         thresholds.pack_voltages.thresholdMax1,
                         thresholds.pack_voltages.thresholdMax2,
                         thresholds.pack_voltages.thresholdMin1,
                         thresholds.pack_voltages.thresholdMin2);
        if (is_valid(thresholds.pack_currents, "pack_currents"))
            DEBUG_PRINTF("max=%.1fA/%.1fA, min=%.1fA/%.1fA\n",
                         thresholds.pack_currents.thresholdMax1,
                         thresholds.pack_currents.thresholdMax2,
                         thresholds.pack_currents.thresholdMin1,
                         thresholds.pack_currents.thresholdMin2);
        if (is_valid(thresholds.pack_temperatures, "pack_temperatures"))
            DEBUG_PRINTF("charge max=%dC/%dC min=%dC/%dC, discharge max=%dC/%dC min=%dC/%dC\n",
                         thresholds.pack_temperatures.chargeTemperatureMax1C,
                         thresholds.pack_temperatures.chargeTemperatureMax2C,
                         thresholds.pack_temperatures.chargeTemperatureMin1C,
                         thresholds.pack_temperatures.chargeTemperatureMin2C,
                         thresholds.pack_temperatures.dischargeTemperatureMax1C,
                         thresholds.pack_temperatures.dischargeTemperatureMax2C,
                         thresholds.pack_temperatures.dischargeTemperatureMin1C,
                         thresholds.pack_temperatures.dischargeTemperatureMin2C);
        if (is_valid(thresholds.pack_soc, "pack_soc"))
            DEBUG_PRINTF("max=%.1f%%/%.1f%%, min=%.1f%%/%.1f%%\n",
                         thresholds.pack_soc.thresholdMax1,
                         thresholds.pack_soc.thresholdMax2,
                         thresholds.pack_soc.thresholdMin1,
                         thresholds.pack_soc.thresholdMin2);
        if (is_valid(thresholds.cell_voltages, "cell_voltages"))
            DEBUG_PRINTF("max=%.3fV/%.3fV, min=%.3fV/%.3fV\n",
                         thresholds.cell_voltages.thresholdMax1,
                         thresholds.cell_voltages.thresholdMax2,
                         thresholds.cell_voltages.thresholdMin1,
                         thresholds.cell_voltages.thresholdMin2);
        if (is_valid(thresholds.cell_sensors, "cell_sensors"))
            DEBUG_PRINTF("voltage diff=%.3fV/%.3fV, temp diff=%dC/%dC\n",
                         thresholds.cell_sensors.cellVoltageDiff1V,
                         thresholds.cell_sensors.cellVoltageDiff2V,
                         thresholds.cell_sensors.cellTemperatureDiff1C,
                         thresholds.cell_sensors.cellTemperatureDiff2C);
        if (is_valid(thresholds.cell_balances, "cell_balances"))
            DEBUG_PRINTF("enable=%.3fV, acceptable=%.3fV\n",
                         thresholds.cell_balances.cellVoltageEnableThreshold,
                         thresholds.cell_balances.cellVoltageAcceptableDifferential);
        if (is_valid(thresholds.pack_shortcircuit, "pack_shortcircuit"))
            DEBUG_PRINTF("shutdown=%.1fA, sampling=%.3fR\n",
                         thresholds.pack_shortcircuit.shortCircuitShutdownA,
                         thresholds.pack_shortcircuit.shortCircuitSamplingR);

        DEBUG_PRINTF("\nstatus:\n");
        if (is_valid(status.pack, "pack"))
            DEBUG_PRINTF("%.1fV, %.1fA, %.1f%%\n",
                         status.pack.packVoltage,
                         status.pack.packCurrent,
                         status.pack.packSOC);
        if (is_valid(status.cell_voltages, "cell_voltages"))
            DEBUG_PRINTF("max=%.3fV (#%d), min=%.3fV (#%d)\n",
                         status.cell_voltages.valueMax,
                         status.cell_voltages.numberMax,
                         status.cell_voltages.valueMin,
                         status.cell_voltages.numberMin);
        if (is_valid(status.cell_temperatures, "cell_temperatures"))
            DEBUG_PRINTF("max=%dC (#%d), min=%dC (#%d)\n",
                         status.cell_temperatures.valueMax,
                         status.cell_temperatures.numberMax,
                         status.cell_temperatures.valueMin,
                         status.cell_temperatures.numberMin);
        if (is_valid(status.fets, "fets"))
            DEBUG_PRINTF("charge=%s, discharge=%s, cycle=%d, capacity=%.1fAh\n",
                         status.fets.chargeMosState ? "on" : "off",
                         status.fets.dischargeMosState ? "on" : "off",
                         status.fets.bmsLifeCycle,
                         status.fets.residualCapacityAh);
        if (is_valid(status.info, "info"))
            DEBUG_PRINTF("cells=%d, sensors=%d, charger=%s, load=%s, cycles=%d\n",
                         status.info.numberOfCells,
                         status.info.numberOfSensors,
                         status.info.chargerStatus ? "on" : "off",
                         status.info.loadStatus ? "on" : "off",
                         status.info.cycles);
        if (is_valid(status.failures, "fail")) {
            DEBUG_PRINTF("show=%s, count=%d",
                         status.failures.failureShow ? "yes" : "no",
                         status.failures.failureCount);
            if (status.failures.failureCount > 0) {
                const char* failures[status.failures.failureCount];
                size_t count = status.failures.getFailureList(failures, status.failures.failureCount);
                DEBUG_PRINTF(", active=[");
                for (size_t i = 0; i < count; i++)
                    DEBUG_PRINTF("%s%s", i == 0 ? "" : ",", failures[i]);
                DEBUG_PRINTF("]");
            }
            DEBUG_PRINTF("\n");
        }

        DEBUG_PRINTF("\ndiagnostics:\n");
        if (is_valid(diagnostics.voltages, "voltages")) {
            DEBUG_PRINTF("cells=%u /", diagnostics.voltages.values.size());
            for (const auto& v : diagnostics.voltages.values)
                DEBUG_PRINTF(" %.3f", v);
            DEBUG_PRINTF("\n");
        }
        if (is_valid(diagnostics.temperatures, "temperatures")) {
            DEBUG_PRINTF("sensors=%u /", diagnostics.temperatures.values.size());
            for (const auto& t : diagnostics.temperatures.values)
                DEBUG_PRINTF(" %d", t);
            DEBUG_PRINTF("\n");
        }
        if (is_valid(diagnostics.balances, "balances")) {
            DEBUG_PRINTF("cells=%u /", diagnostics.balances.values.size());
            for (const auto& b : diagnostics.balances.values)
                DEBUG_PRINTF(" %d", b);
            DEBUG_PRINTF("\n");
        }
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
    int _id;
    Connector& _connector;
    RequestResponseManager _manager;
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class HardwareSerialConnector : public Interface::Connector {
public:
    static inline constexpr unsigned long DEFAULT_BAUD = 9600;
    explicit HardwareSerialConnector(HardwareSerial& serial, const unsigned long baud = DEFAULT_BAUD)
        : _serial(serial) {
        _serial.begin(baud);
    }
protected:
    void begin() override {
    }
    bool getByte(uint8_t* byte) override {
        if (_serial.available() > 0) {
            *byte = _serial.read();
            return true;
        }
        return false;
    }
    bool sendBytes(const uint8_t* data, const size_t size) override {
        return _serial.write(data, size) == size;
    }
private:
    HardwareSerial& _serial;
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

}    // namespace daly_bms

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
