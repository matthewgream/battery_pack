
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class FanManager : public Component, public Diagnosticable {
public:
    typedef struct {
    } Config;

    using TargetSet = std::pair<float, float>;
    using TargetSetFunc = std::function<TargetSet ()>;

private:
    const Config &config;

    FanInterface &_fan;
    PidController<double> &_controllerAlgorithm;    // XXX should be an abstract interface
    AlphaSmoothing<double> &_smootherAlgorithm;     // XXX should be an abstract interface

    const TargetSetFunc _targetValues;
    float _value = 0.0f;
    Stats<float> _statsValue;

public:
    FanManager (const Config &cfg, FanInterface &fan, PidController<double> &controller, AlphaSmoothing<double> &smoother, const TargetSetFunc targetValues) :
        config (cfg),
        _fan (fan),
        _controllerAlgorithm (controller),
        _smootherAlgorithm (smoother),
        _targetValues (targetValues) {
    }
    void process () override {
        const TargetSet targets (_targetValues ());
        const float &setpoint = targets.first, &current = targets.second;
        if (current < setpoint) {
            DEBUG_PRINTF ("FanManager::process: setpoint=%.2f, current=%.2f\n", setpoint, current);
            _fan.setSpeed (0.0f);
        } else {
            const double speedCalculated = _controllerAlgorithm.apply (setpoint, current);
            const double speedConstrained = std::clamp (map<double> (speedCalculated, -100.0, 100.0, 0.0, 100.0), 0.0, 100.0);    // XXX is this correct?
            const double speedSmoothed = _smootherAlgorithm.apply (speedConstrained);
            DEBUG_PRINTF ("FanManager::process: setpoint=%.2f, current=%.2f --> calculated=%.2e, constrained=%.2e, smoothed=%.2e\n", setpoint, current, speedCalculated, speedConstrained, speedSmoothed);
            _value = static_cast<float> (speedSmoothed);
            _fan.setSpeed (_value);    // percentage: 0% -> 100%
            _statsValue += _value;
        }
    }

protected:
    void collectDiagnostics (JsonVariant &obj) const override {
        JsonObject sub = obj ["fan"].to<JsonObject> ();
        sub ["pid"] = _controllerAlgorithm;
        sub ["speed"] = _statsValue;
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
