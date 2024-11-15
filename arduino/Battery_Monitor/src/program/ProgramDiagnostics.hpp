
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class Diagnosticable {
protected:
    ~Diagnosticable () {};

public:
    virtual void collectDiagnostics (JsonVariant &) const = 0;
};

class DiagnosticablesManager : public Component {
public:
    typedef struct {
    } Config;

    using List = std::vector<Diagnosticable *>;

private:
    const Config &config;
    const List _diagnosticables;

public:
    DiagnosticablesManager (const Config &cfg, const List &diagnosticables) :
        config (cfg),
        _diagnosticables (diagnosticables) { }
    void collect (JsonVariant &obj) const {
        for (const auto &diagnosticable : _diagnosticables)
            diagnosticable->collectDiagnostics (obj);
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
