#include "SceneEngine.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <regex>

namespace matter {

// --- IntentParser ---

static std::string toLower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

bool IntentParser::matchesQuery(const std::string& lower) const {
    static const std::vector<std::string> queryWords = {
        "what", "is", "are", "how", "show", "tell", "check", "status",
        "anything", "which", "temperature", "temp"
    };
    for (const auto& w : queryWords) {
        if (lower.find(w) != std::string::npos &&
            lower.find("turn") == std::string::npos &&
            lower.find("set") == std::string::npos) {
            return true;
        }
    }
    return false;
}

bool IntentParser::matchesCommand(const std::string& lower, UserIntent& intent) const {
    // Turn on/off
    std::regex onOff(R"((turn|switch)\s+(on|off)(.*))", std::regex::icase);
    std::smatch match;
    if (std::regex_search(lower, match, onOff)) {
        intent.action = match[2].str();
        return true;
    }

    // Direct on/off at start
    if (lower.find("lights on") != std::string::npos ||
        lower.find("light on") != std::string::npos) {
        intent.action = "on";
        return true;
    }
    if (lower.find("lights off") != std::string::npos ||
        lower.find("light off") != std::string::npos) {
        intent.action = "off";
        return true;
    }

    // Set brightness/temperature
    std::regex setVal(R"(set\s+.*?(?:to|at)\s+(\d+))", std::regex::icase);
    if (std::regex_search(lower, match, setVal)) {
        intent.action = "set";
        intent.value = std::stoi(match[1].str());
        return true;
    }

    // Lock/unlock
    if (lower.find("lock") != std::string::npos) {
        intent.action = (lower.find("unlock") != std::string::npos) ? "unlock" : "lock";
        return true;
    }

    // Dim
    std::regex dim(R"(dim\s+.*?(?:to)?\s*(\d+))", std::regex::icase);
    if (std::regex_search(lower, match, dim)) {
        intent.action = "set";
        intent.value = std::stoi(match[1].str());
        return true;
    }

    return false;
}

std::string IntentParser::extractRoom(const std::string& lower,
                                       const std::vector<std::string>& knownRooms) const {
    for (const auto& room : knownRooms) {
        std::string lowerRoom = toLower(room);
        if (lower.find(lowerRoom) != std::string::npos) {
            return room;
        }
    }

    // Common room names as fallback
    static const std::vector<std::string> commonRooms = {
        "kitchen", "bedroom", "living room", "bathroom", "garage",
        "hallway", "office", "basement", "attic", "porch",
        "downstairs", "upstairs"
    };
    for (const auto& room : commonRooms) {
        if (lower.find(room) != std::string::npos) {
            return room;
        }
    }
    return "";
}

UserIntent IntentParser::parse(const std::string& input) const {
    UserIntent intent;
    intent.rawQuery = input;

    std::string lower = toLower(input);

    // Extract room
    intent.room = extractRoom(lower, knownRooms_);

    // Determine type
    if (lower.find("scene") != std::string::npos ||
        lower.find("activate") != std::string::npos ||
        lower.find("movie") != std::string::npos ||
        lower.find("bedtime") != std::string::npos ||
        lower.find("morning") != std::string::npos) {
        intent.type = UserIntent::Type::Scene;
    } else if (lower == "summary" || lower == "status" ||
               lower.find("give me a summary") != std::string::npos ||
               lower.find("home status") != std::string::npos) {
        intent.type = UserIntent::Type::Status;
    } else if (matchesCommand(lower, intent)) {
        intent.type = UserIntent::Type::Command;
    } else if (matchesQuery(lower)) {
        intent.type = UserIntent::Type::Query;
    } else {
        intent.type = UserIntent::Type::Unknown;
    }

    return intent;
}

// --- SceneEngine ---

SceneEngine::SceneEngine(DeviceManager& devices) : devices_(devices) {}

void SceneEngine::addScene(Scene scene) {
    scenes_.push_back(std::move(scene));
}

SceneEngine::Result SceneEngine::processIntent(const UserIntent& intent) {
    switch (intent.type) {
        case UserIntent::Type::Query:
            return processQuery(intent);
        case UserIntent::Type::Command:
            return processCommand(intent);
        case UserIntent::Type::Scene: {
            // Extract scene name from raw query
            std::string lower = toLower(intent.rawQuery);
            for (const auto& scene : scenes_) {
                if (lower.find(toLower(scene.name)) != std::string::npos) {
                    return activateScene(scene.name);
                }
            }
            return {"I don't know that scene. Available: " +
                    [this]() {
                        std::string s;
                        for (const auto& sc : scenes_) {
                            if (!s.empty()) s += ", ";
                            s += sc.name;
                        }
                        return s;
                    }(), {}};
        }
        case UserIntent::Type::Status:
            return {devices_.homeSummary(), {}};
        case UserIntent::Type::Unknown:
            return {"I'm not sure what you'd like me to do. Try asking about your devices or telling me to turn something on or off.", {}};
    }
    return {"Something went wrong.", {}};
}

SceneEngine::Result SceneEngine::processQuery(const UserIntent& intent) {
    std::ostringstream oss;
    std::string lower = toLower(intent.rawQuery);

    if (!intent.room.empty()) {
        // Room-specific query
        oss << devices_.roomSummary(intent.room);
    } else if (lower.find("on") != std::string::npos &&
               (lower.find("anything") != std::string::npos ||
                lower.find("what") != std::string::npos)) {
        // "Is anything on?" / "What's on?"
        bool found = false;
        for (auto* dev : devices_.allDevices()) {
            if (dev->isOn()) {
                if (!found) { oss << "These devices are on:\n"; found = true; }
                oss << "  " << dev->name << " (" << dev->room << ")\n";
            }
        }
        if (!found) oss << "Everything is off.";
    } else if (lower.find("temperature") != std::string::npos ||
               lower.find("temp") != std::string::npos) {
        bool found = false;
        for (auto* dev : devices_.allDevices()) {
            auto temp = dev->temperature();
            if (temp) {
                if (!found) { oss << "Temperatures:\n"; found = true; }
                oss << "  " << dev->name << " (" << dev->room << "): "
                    << std::fixed << std::setprecision(1) << *temp << "°C\n";
            }
        }
        if (!found) oss << "No temperature sensors found.";
    } else {
        oss << devices_.homeSummary();
    }

    return {oss.str(), {}};
}

SceneEngine::Result SceneEngine::processCommand(const UserIntent& intent) {
    std::vector<DeviceAction> actions;
    std::ostringstream oss;

    auto targets = intent.room.empty()
        ? devices_.allDevices()
        : devices_.devicesInRoom(intent.room);

    if (targets.empty()) {
        return {"No devices found" +
                (intent.room.empty() ? "." : " in " + intent.room + "."), {}};
    }

    for (auto* dev : targets) {
        for (auto& ep : dev->endpoints) {
            if (ep.id == 0) continue; // skip root endpoint
            if (intent.action == "on" || intent.action == "off") {
                if (ep.hasCluster(ClusterId::OnOff)) {
                    actions.push_back({
                        dev->nodeId, ep.id, ClusterId::OnOff,
                        intent.action == "on" ? "On" : "Off",
                        AttributeValue::fromBool(intent.action == "on")
                    });
                }
            } else if (intent.action == "set" && intent.value) {
                if (ep.hasCluster(ClusterId::LevelControl)) {
                    actions.push_back({
                        dev->nodeId, ep.id, ClusterId::LevelControl,
                        "MoveToLevel",
                        AttributeValue::fromInt(*intent.value)
                    });
                }
            } else if (intent.action == "lock" || intent.action == "unlock") {
                if (ep.hasCluster(ClusterId::DoorLock)) {
                    actions.push_back({
                        dev->nodeId, ep.id, ClusterId::DoorLock,
                        intent.action == "lock" ? "LockDoor" : "UnlockDoor",
                        AttributeValue::fromBool(intent.action == "lock")
                    });
                }
            }
        }
    }

    if (actions.empty()) {
        oss << "No applicable devices found for that command.";
    } else {
        oss << "OK, ";
        if (intent.action == "on" || intent.action == "off") {
            oss << "turning " << intent.action << " " << actions.size()
                << " device" << (actions.size() > 1 ? "s" : "");
        } else if (intent.action == "set") {
            oss << "setting " << actions.size() << " device"
                << (actions.size() > 1 ? "s" : "") << " to " << *intent.value;
        } else {
            oss << intent.action << "ing " << actions.size() << " device"
                << (actions.size() > 1 ? "s" : "");
        }
        if (!intent.room.empty()) oss << " in " << intent.room;
        oss << ".";
    }

    return {oss.str(), actions};
}

SceneEngine::Result SceneEngine::activateScene(const std::string& sceneName) {
    for (const auto& scene : scenes_) {
        if (scene.name == sceneName) {
            return {"Activating " + scene.name + ": " + scene.description,
                    scene.actions};
        }
    }
    return {"Scene '" + sceneName + "' not found.", {}};
}

} // namespace matter
