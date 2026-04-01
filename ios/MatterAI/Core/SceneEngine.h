#pragma once

#include "DeviceModel.h"
#include <string>
#include <vector>
#include <functional>

namespace matter {

// An action to perform on a device
struct DeviceAction {
    uint64_t nodeId;
    uint16_t endpointId;
    ClusterId cluster;
    std::string command;      // e.g. "On", "Off", "MoveToLevel"
    AttributeValue argument;  // command parameter
};

// A scene = a named collection of device actions
struct Scene {
    std::string name;
    std::string description;
    std::vector<DeviceAction> actions;
};

// Intent parsed from natural language
struct UserIntent {
    enum class Type {
        Query,      // "is anything on downstairs?"
        Command,    // "turn off the kitchen lights"
        Scene,      // "activate movie mode"
        Status,     // "give me a summary"
        Unknown,
    };

    Type type = Type::Unknown;
    std::string room;              // optional room filter
    std::string deviceName;        // optional specific device
    std::string action;            // "on", "off", "set", etc.
    std::optional<int> value;      // brightness level, temperature, etc.
    std::string rawQuery;
};

// Parses natural language into structured intents
// This is the C++ side — on-device Apple Intelligence will enhance this
class IntentParser {
public:
    UserIntent parse(const std::string& input) const;

private:
    // Keyword-based parsing (baseline before AI enhancement)
    bool matchesQuery(const std::string& lower) const;
    bool matchesCommand(const std::string& lower, UserIntent& intent) const;
    std::string extractRoom(const std::string& lower, const std::vector<std::string>& knownRooms) const;

    std::vector<std::string> knownRooms_;

public:
    void setKnownRooms(const std::vector<std::string>& rooms) { knownRooms_ = rooms; }
};

// Executes intents against the device model
class SceneEngine {
public:
    explicit SceneEngine(DeviceManager& devices);

    // Register a named scene
    void addScene(Scene scene);

    // Process a user intent and return response + actions
    struct Result {
        std::string response;           // text response for the user
        std::vector<DeviceAction> actions; // commands to execute
    };

    Result processIntent(const UserIntent& intent);
    Result processQuery(const UserIntent& intent);
    Result processCommand(const UserIntent& intent);
    Result activateScene(const std::string& sceneName);

    const std::vector<Scene>& scenes() const { return scenes_; }

private:
    DeviceManager& devices_;
    std::vector<Scene> scenes_;
};

} // namespace matter
