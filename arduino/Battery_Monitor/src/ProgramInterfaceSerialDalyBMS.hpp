
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include "DalyBMSInterface.hpp"

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <tuple>

class ProgramInterfaceDalyBMS : public Component, public Diagnosticable {

public:
    struct Config {
        daly_bms::Interface::Config manager, balance;
        interval_t intervalInstant, intervalStatus, intervalDiagnostics;
    };

private:
    const Config &config;

    daly_bms::Interfaces dalyInterfaces;
    Intervalable intervalInstant, intervalStatus, intervalDiagnostics;
    Enableable enabled;

public:
    explicit ProgramInterfaceDalyBMS (const Config &conf) :
        config (conf),
        dalyInterfaces ({
            .interfaces = { config.manager, config.balance }
    }),
        intervalInstant (config.intervalInstant), intervalStatus (config.intervalStatus), intervalDiagnostics (config.intervalDiagnostics) { }    // XXX need to randomise to prevent synchronicity

    void begin () override {
        // if (dalyInterfaces.begin())
        //     DEBUG_PRINTF("DalyBMSManager::begin: daly begin failed\n");
        // else {
        //     enabled ++;
        //     intervalStatus.setat(RandomNumber::get<interval_t>(config.intervalStatus));
        //     intervalDiagnostics.setat(RandomNumber::get<interval_t>(config.intervalDiagnostics));
        // }
    }
    void process () override {
        // if (enabled) {
        //     if (intervalInstant) dalyInterfaces.requestInstant();
        //     if (intervalStatus) dalyInterfaces.requestStatus();
        //     if (intervalDiagnostics) dalyInterfaces.requestDiagnostics(), dalyInterfaces.updateInitial();
        //     dalyInterfaces.process();
        // }
    }

    struct Instant {
        float voltage, current, charge;
    };
    Instant instant () const {
        // dalyInterfaces ["name"].status.status;
        return { .voltage = 0.0f, .current = 0.0f, .charge = 0.0f };
    }

protected:
    void collectDiagnostics (JsonVariant &obj) const override {
        // last spoke
        // total comms
        // diags of each (but not deep diags!)
        // alarms
        // failure details
        // soc empty, low, nearlycharged, charged
        //
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
