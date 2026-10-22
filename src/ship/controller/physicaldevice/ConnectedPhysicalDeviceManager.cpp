#include "ship/controller/physicaldevice/ConnectedPhysicalDeviceManager.h"
#include "ship/Context.h"
#include "ship/config/ConsoleVariable.h"
#include "ship/utils/StringHelper.h"
#include <spdlog/spdlog.h>

namespace Ship {
ConnectedPhysicalDeviceManager::ConnectedPhysicalDeviceManager() {
}

ConnectedPhysicalDeviceManager::~ConnectedPhysicalDeviceManager() {
}

std::unordered_map<int32_t, SDL_GameController*>
ConnectedPhysicalDeviceManager::GetConnectedSDLGamepadsForPort(uint8_t portIndex) {
    std::unordered_map<int32_t, SDL_GameController*> result;

    for (const auto& [instanceId, gamepad] : mConnectedSDLGamepads) {
        if (!PortIsIgnoringInstanceId(portIndex, instanceId)) {
            result[instanceId] = gamepad;
        }
    }

    return result;
}

std::unordered_map<int32_t, std::string> ConnectedPhysicalDeviceManager::GetConnectedSDLGamepadNames() {
    return mConnectedSDLGamepadNames;
}

std::unordered_set<int32_t> ConnectedPhysicalDeviceManager::GetIgnoredInstanceIdsForPort(uint8_t portIndex) {
    return mIgnoredInstanceIds[portIndex];
}

bool ConnectedPhysicalDeviceManager::PortIsIgnoringInstanceId(uint8_t portIndex, int32_t instanceId) {
    return GetIgnoredInstanceIdsForPort(portIndex).contains(instanceId);
}

bool ConnectedPhysicalDeviceManager::PortHasExplicitSDLGamepadAssignments(uint8_t portIndex) {
    auto context = Context::GetInstance();
    auto consoleVariables = context == nullptr ? nullptr : context->GetConsoleVariables();
    if (consoleVariables == nullptr) {
        return false;
    }

    const std::string cvarKey =
        StringHelper::Sprintf(CVAR_PREFIX_CONTROLLERS ".Port%d.HasAssignedSDLGamepadGUIDs", portIndex + 1);

    return consoleVariables->GetInteger(cvarKey.c_str(), 0) != 0;
}

void ConnectedPhysicalDeviceManager::IgnoreInstanceIdForPort(uint8_t portIndex, int32_t instanceId) {
    if (portIndex >= 4) {
        return;
    }

    // Player 1 previously allowed every connected gamepad implicitly. On the first explicit
    // change, convert that implicit set into a saved assignment so deselecting a controller
    // actually sticks across restarts.
    if ((portIndex == 0) && !PortHasExplicitSDLGamepadAssignments(portIndex)) {
        std::unordered_set<std::string> guids;

        for (const auto& [connectedInstanceId, connectedGUID] : mConnectedSDLGamepadGUIDs) {
            if (!PortIsIgnoringInstanceId(portIndex, connectedInstanceId)) {
                guids.insert(connectedGUID);
            }
        }

        SetAssignedSDLGamepadGUIDsForPort(portIndex, guids);
    }

    mIgnoredInstanceIds[portIndex].insert(instanceId);

    auto guid = mConnectedSDLGamepadGUIDs.find(instanceId);
    if (guid == mConnectedSDLGamepadGUIDs.end()) {
        return;
    }

    for (const auto& [otherInstanceId, otherGUID] : mConnectedSDLGamepadGUIDs) {
        if (otherInstanceId != instanceId && otherGUID == guid->second &&
            !PortIsIgnoringInstanceId(portIndex, otherInstanceId)) {
            return;
        }
    }

    auto guids = GetAssignedSDLGamepadGUIDsForPort(portIndex);
    if (guids.erase(guid->second) != 0) {
        SetAssignedSDLGamepadGUIDsForPort(portIndex, guids);
    }
}

void ConnectedPhysicalDeviceManager::UnignoreInstanceIdForPort(uint8_t portIndex, int32_t instanceId) {
    if (portIndex >= 4) {
        return;
    }

    if ((portIndex == 0) && !PortHasExplicitSDLGamepadAssignments(portIndex)) {
        std::unordered_set<std::string> guids;

        for (const auto& [connectedInstanceId, connectedGUID] : mConnectedSDLGamepadGUIDs) {
            if (!PortIsIgnoringInstanceId(portIndex, connectedInstanceId)) {
                guids.insert(connectedGUID);
            }
        }

        SetAssignedSDLGamepadGUIDsForPort(portIndex, guids);
    }

    mIgnoredInstanceIds[portIndex].erase(instanceId);

    auto guid = mConnectedSDLGamepadGUIDs.find(instanceId);
    if (guid == mConnectedSDLGamepadGUIDs.end()) {
        return;
    }

    auto guids = GetAssignedSDLGamepadGUIDsForPort(portIndex);
    if (guids.insert(guid->second).second) {
        SetAssignedSDLGamepadGUIDsForPort(portIndex, guids);
    }
}

std::unordered_set<std::string>
ConnectedPhysicalDeviceManager::GetAssignedSDLGamepadGUIDsForPort(uint8_t portIndex) {
    std::unordered_set<std::string> guids;
    auto context = Context::GetInstance();
    auto consoleVariables = context == nullptr ? nullptr : context->GetConsoleVariables();
    if (consoleVariables == nullptr) {
        return guids;
    }

    const std::string cvarKey =
        StringHelper::Sprintf(CVAR_PREFIX_CONTROLLERS ".Port%d.AssignedSDLGamepadGUIDs", portIndex + 1);
    const std::string assignedGUIDs = consoleVariables->GetString(cvarKey.c_str(), "");
    for (const auto& guid : StringHelper::Split(assignedGUIDs, ",")) {
        if (!guid.empty()) {
            guids.insert(guid);
        }
    }

    return guids;
}

void ConnectedPhysicalDeviceManager::SetAssignedSDLGamepadGUIDsForPort(
    uint8_t portIndex, const std::unordered_set<std::string>& guids) {
    auto context = Context::GetInstance();
    auto consoleVariables = context == nullptr ? nullptr : context->GetConsoleVariables();
    if (consoleVariables == nullptr) {
        return;
    }

    std::string joinedGUIDs;
    for (const auto& guid : guids) {
        if (!joinedGUIDs.empty()) {
            joinedGUIDs += ",";
        }
        joinedGUIDs += guid;
    }

    const std::string cvarKey =
        StringHelper::Sprintf(CVAR_PREFIX_CONTROLLERS ".Port%d.AssignedSDLGamepadGUIDs", portIndex + 1);
    const std::string markerKey =
        StringHelper::Sprintf(CVAR_PREFIX_CONTROLLERS ".Port%d.HasAssignedSDLGamepadGUIDs", portIndex + 1);
    consoleVariables->SetString(cvarKey.c_str(), joinedGUIDs.c_str());
    consoleVariables->SetInteger(markerKey.c_str(), 1);
    consoleVariables->Save();
}

void ConnectedPhysicalDeviceManager::HandlePhysicalDeviceConnect(int32_t sdlDeviceIndex) {
    RefreshConnectedSDLGamepads();
}

void ConnectedPhysicalDeviceManager::HandlePhysicalDeviceDisconnect(int32_t sdlJoystickInstanceId) {
    RefreshConnectedSDLGamepads();
}

void ConnectedPhysicalDeviceManager::RefreshConnectedSDLGamepads() {
    mConnectedSDLGamepads.clear();
    mConnectedSDLGamepadNames.clear();
    mConnectedSDLGamepadGUIDs.clear();

    std::unordered_set<std::string> assignedGUIDsByPort[4];
    bool portHasExplicitAssignments[4];
    for (uint8_t port = 0; port < 4; port++) {
        assignedGUIDsByPort[port] = GetAssignedSDLGamepadGUIDsForPort(port);
        portHasExplicitAssignments[port] = PortHasExplicitSDLGamepadAssignments(port);
        mIgnoredInstanceIds[port].clear();
    }

    static SDL_JoystickGUID sZeroGuid;

    for (int32_t i = 0; i < SDL_NumJoysticks(); i++) {

        SDL_JoystickGUID deviceGUID = SDL_JoystickGetDeviceGUID(i);
        if (SDL_memcmp(&deviceGUID, &sZeroGuid, sizeof(deviceGUID)) == 0) {
            SPDLOG_WARN(
                "Calling SDL JoystickGetDeviceGUID with index ({:d}) returned zero GUID. This is likely due to an "
                "invalid index. Refer to https://wiki.libsdl.org/SDL2/SDL_JoystickGetDeviceGUID for more information.",
                i);
            continue;
        }

        char deviceGuidCStr[33] = "";
        SDL_JoystickGetGUIDString(deviceGUID, deviceGuidCStr, sizeof(deviceGuidCStr));

        if (!SDL_IsGameController(i)) {
            SPDLOG_WARN("SDL Joystick (GUID: {}) not recognized as gamepad."
                        "This is likely due to a missing mapping string in gamecontrollerdb.txt."
                        "Refer to https://github.com/mdqinc/SDL_GameControllerDB for more information.",
                        deviceGuidCStr);
            continue;
        }

        auto gamepad = SDL_GameControllerOpen(i);
        if (gamepad == nullptr) {
            SPDLOG_ERROR("SDL GameControllerOpen error (GUID: {}): {}", deviceGuidCStr, SDL_GetError());
            continue;
        }

        auto instanceId = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(gamepad));
        if (instanceId < 0) {
            SPDLOG_ERROR("SDL JoystickInstanceID error (GUID: {}): {}", deviceGuidCStr, SDL_GetError());
            continue;
        }

        std::string gamepadName;
        auto name = SDL_GameControllerName(gamepad);
        if (name == nullptr) {
            gamepadName = deviceGuidCStr;
            SPDLOG_WARN("SDL_GameControllerName returned null. Setting name to GUID \"{}\" instead.", gamepadName);
        } else {
            gamepadName = name;
        }

        mConnectedSDLGamepads[instanceId] = gamepad;
        mConnectedSDLGamepadNames[instanceId] = gamepadName;
        mConnectedSDLGamepadGUIDs[instanceId] = deviceGuidCStr;

        for (uint8_t port = 0; port < 4; port++) {
            bool assignedToPort = assignedGUIDsByPort[port].contains(deviceGuidCStr);

            // Until player 1 has an explicit saved assignment, let P1 use every gamepad that
            // isn't assigned to another player. This keeps the old default behavior while
            // preventing P1 from also controlling a gamepad selected for P2-P4.
            if ((port == 0) && !portHasExplicitAssignments[port]) {
                assignedToPort = true;

                for (uint8_t otherPort = 1; otherPort < 4; otherPort++) {
                    if (assignedGUIDsByPort[otherPort].contains(deviceGuidCStr)) {
                        assignedToPort = false;
                        break;
                    }
                }
            }

            if (!assignedToPort) {
                mIgnoredInstanceIds[port].insert(instanceId);
            }
        }
    }
}
} // namespace Ship
