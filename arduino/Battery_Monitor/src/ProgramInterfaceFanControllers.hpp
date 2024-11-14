
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class ProgramInterfaceFanControllers;
class ProgramInterfaceFanControllersStrategy {
public:
    virtual String name () const = 0;
    virtual void begin (ProgramInterfaceFanControllers &interface, OpenSmart_QuadMotorDriver &hardware) = 0;
    virtual bool setSpeed (const OpenSmart_QuadMotorDriver::MotorSpeed speed) = 0;
};

class ProgramInterfaceFanControllers : public Component, public Diagnosticable {
public:
    using FanSpeedType = OpenSmart_QuadMotorDriver::MotorSpeed;
    using FanDirectionType = OpenSmart_QuadMotorDriver::MotorDirection;
    static inline constexpr FanSpeedType FanSpeedMin = 0, FanSpeedMax = (1 << OpenSmart_QuadMotorDriver::MotorSpeedResolution) - 1;
    static inline constexpr size_t FanSpeedRange = (1 << OpenSmart_QuadMotorDriver::MotorSpeedResolution);

    typedef struct {
        OpenSmart_QuadMotorDriver::Config hardware;
        FanDirectionType DIRECTION;
        FanSpeedType MIN_SPEED, MAX_SPEED;
        std::array<int, OpenSmart_QuadMotorDriver::MotorCount> MOTOR_ORDER;
        interval_t MOTOR_ROTATE;
    } Config;

private:
    const Config &config;

    ProgramInterfaceFanControllersStrategy &_strategy;
    OpenSmart_QuadMotorDriver _hardware;

    FanSpeedType _speed = 0;
    bool _active = false;
    StatsWithValue<FanSpeedType> _speedStats;
    ActivationTracker _actives;

public:
    ProgramInterfaceFanControllers (const Config &cfg, ProgramInterfaceFanControllersStrategy &strategy) :
        config (cfg),
        _strategy (strategy),
        _hardware (config.hardware) {
        assert (config.MIN_SPEED < config.MAX_SPEED && "Bad configuration values");
    }
    ~ProgramInterfaceFanControllers () {
        end ();
    }
    void begin () override {
        DEBUG_PRINTF ("FanInterface::begin: strategy=%s\n", _strategy.name ().c_str ());
        _hardware.setDirection (config.DIRECTION);
        _hardware.setSpeed (static_cast<OpenSmart_QuadMotorDriver::MotorSpeed> (0), OpenSmart_QuadMotorDriver::MOTOR_ALL);
        _strategy.begin (*this, _hardware);
    }
    void end () {
        _hardware.setSpeed (static_cast<OpenSmart_QuadMotorDriver::MotorSpeed> (0), OpenSmart_QuadMotorDriver::MOTOR_ALL);
    }

    void setSpeed (const float speed) {
        const FanSpeedType speedNew = std::clamp (static_cast<FanSpeedType> (map<float> (speed, 0.0f, 100.0f, static_cast<float> (FanSpeedMin), static_cast<float> (FanSpeedMax))), FanSpeedMin, FanSpeedMax);
        if (speedNew != _speed) {
            DEBUG_PRINTF ("FanInterface::setSpeed: %d\n", speedNew);
            bool active = _strategy.setSpeed (_speed = speedNew);
            if (! _active && active)
                _actives++, _active = active;
            _speedStats += _speed;
        }
    }
    inline FanSpeedType getSpeed () const {
        return _speed;
    }

    inline const Config &getConfig () const {
        return config;
    }    // yuck

protected:
    void collectDiagnostics (JsonVariant &obj) const override {
        JsonObject sub = obj ["fan"].to<JsonObject> ();
        sub ["speed"] = _speedStats;
        if (_actives)
            sub ["actives"] = _actives;
        // % duty
    }
};

// -----------------------------------------------------------------------------------------------

class ProgramInterfaceFanControllersStrategy_motorAll : public ProgramInterfaceFanControllersStrategy {
    ProgramInterfaceFanControllers *_interface = nullptr;
    OpenSmart_QuadMotorDriver *_hardware = nullptr;

public:
    String name () const override {
        return "motorAll(" + ArithmeticToString (OpenSmart_QuadMotorDriver::MotorCount) + ")";
    }
    void begin (ProgramInterfaceFanControllers &interface, OpenSmart_QuadMotorDriver &hardware) override {
        _hardware = &hardware;
        _interface = &interface;
    }
    bool setSpeed (const OpenSmart_QuadMotorDriver::MotorSpeed speed) override {
        if (speed == ProgramInterfaceFanControllers::FanSpeedMin) {
            _hardware->stop (OpenSmart_QuadMotorDriver::MOTOR_ALL);
            _hardware->setSpeed (speed, OpenSmart_QuadMotorDriver::MOTOR_ALL);
        } else {
            _hardware->setSpeed (map<OpenSmart_QuadMotorDriver::MotorSpeed> (speed, ProgramInterfaceFanControllers::FanSpeedMin, ProgramInterfaceFanControllers::FanSpeedMax, _interface->getConfig ().MIN_SPEED, _interface->getConfig ().MAX_SPEED), OpenSmart_QuadMotorDriver::MOTOR_ALL);
            _hardware->setDirection (_interface->getConfig ().DIRECTION);
        }
        return speed > ProgramInterfaceFanControllers::FanSpeedMin;
    }
};

using ProgramInterfaceFanControllersStrategy_default = ProgramInterfaceFanControllersStrategy_motorAll;

class ProgramInterfaceFanControllersStrategy_motorMap : public ProgramInterfaceFanControllersStrategy {
    OpenSmart_QuadMotorDriver *_hardware = nullptr;
    OpenSmart_QuadMotorDriver::MotorSpeed _min_speed = OpenSmart_QuadMotorDriver::MotorSpeed (0), _max_speed = OpenSmart_QuadMotorDriver::MotorSpeed (0);
    std::array<OpenSmart_QuadMotorDriver::MotorSpeed, OpenSmart_QuadMotorDriver::MotorCount> _motorSpeeds;

protected:
    std::array<int, OpenSmart_QuadMotorDriver::MotorCount> _motorOrder;

public:
    String name () const override {
        return "motorMap(" + ArithmeticToString (OpenSmart_QuadMotorDriver::MotorCount) + ")";
    }
    void begin (ProgramInterfaceFanControllers &interface, OpenSmart_QuadMotorDriver &hardware) override {
        _hardware = &hardware;
        _min_speed = interface.getConfig ().MIN_SPEED;
        _max_speed = interface.getConfig ().MAX_SPEED;
        for (int motorId = 0; motorId < OpenSmart_QuadMotorDriver::MotorCount; motorId++)
            _motorOrder [motorId] = interface.getConfig ().MOTOR_ORDER [motorId], _motorSpeeds [motorId] = static_cast<OpenSmart_QuadMotorDriver::MotorSpeed> (0);
    }
    bool setSpeed (const OpenSmart_QuadMotorDriver::MotorSpeed speed) override {
        int activated = 0;
        for (int i = 0, currentThreshold = 0, totalSpeed = speed * OpenSmart_QuadMotorDriver::MotorCount; i < OpenSmart_QuadMotorDriver::MotorCount; i++, currentThreshold += ProgramInterfaceFanControllers::FanSpeedRange) {
            const int motorId = _motorOrder [i];
            OpenSmart_QuadMotorDriver::MotorSpeed motorSpeed = 0;
            if (totalSpeed >= (currentThreshold + ProgramInterfaceFanControllers::FanSpeedRange))
                motorSpeed = _max_speed;
            else if (totalSpeed > currentThreshold && totalSpeed < (currentThreshold + static_cast<int> (ProgramInterfaceFanControllers::FanSpeedRange)))
                motorSpeed = map (totalSpeed - currentThreshold, 0, ProgramInterfaceFanControllers::FanSpeedRange, _min_speed, _max_speed);
            if (motorSpeed != _motorSpeeds [motorId])
                _hardware->setSpeed (motorSpeed, static_cast<OpenSmart_QuadMotorDriver::MotorID> (motorId)), _motorSpeeds [motorId] = motorSpeed;
            if (motorSpeed > 0)
                activated++;
        }
        return activated > 0;
    }
};

class ProgramInterfaceFanControllersStrategy_motorMapWithRotation : public ProgramInterfaceFanControllersStrategy_motorMap {
    Intervalable _rotationInterval;

public:
    String name () const override {
        return "motorMapWithRotation(" + ArithmeticToString (OpenSmart_QuadMotorDriver::MotorCount) + ")";
    }
    void begin (ProgramInterfaceFanControllers &interface, OpenSmart_QuadMotorDriver &hardware) override {
        DEBUG_PRINTF ("FanInterfaceStrategy_motorMapWithRotation:: order=[");
        for (int i = 0; i < interface.getConfig ().MOTOR_ORDER.size (); i++)
            DEBUG_PRINTF ("%s%d", i == 0 ? "" : ",", interface.getConfig ().MOTOR_ORDER [i]);
        DEBUG_PRINTF ("], period=%lu\n", interface.getConfig ().MOTOR_ROTATE);
        _rotationInterval.reset (interface.getConfig ().MOTOR_ROTATE);
        ProgramInterfaceFanControllersStrategy_motorMap::begin (interface, hardware);
    }
    bool setSpeed (const OpenSmart_QuadMotorDriver::MotorSpeed speed) override {
        if (_rotationInterval) {
            DEBUG_PRINTF ("FanInterfaceStrategy_motorMapWithRotation:: rotating\n");
            std::rotate (_motorOrder.begin (), _motorOrder.begin () + 1, _motorOrder.end ());
        }
        return ProgramInterfaceFanControllersStrategy_motorMap::setSpeed (speed);
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
