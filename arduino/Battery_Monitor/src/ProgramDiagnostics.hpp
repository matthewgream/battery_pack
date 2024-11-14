
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class Diagnosticable {
protected:
    ~Diagnosticable () {};

public:
    typedef std::vector<Diagnosticable *> List;
    virtual void collectDiagnostics (JsonVariant &) const = 0;
};

class ProgramDiagnostics : public Component {
public:
    typedef struct {
    } Config;

private:
    const Config &config;

    const Diagnosticable::List _diagnosticables;

public:
    ProgramDiagnostics (const Config &cfg, const Diagnosticable::List &diagnosticables) :
        config (cfg),
        _diagnosticables (diagnosticables) { }
    void collect (JsonVariant &obj) const {
        for (const auto &diagnosticable : _diagnosticables)
            diagnosticable->collectDiagnostics (obj);
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
