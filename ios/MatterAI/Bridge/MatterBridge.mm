#import "MatterBridge.h"
#include "DeviceModel.h"
#include "TLVCodec.h"
#include "SceneEngine.h"
#include "SimulationModel.h"

// --- Obj-C wrapper implementations ---

@implementation MADevice
@end

@implementation MADeviceAction
@end

@implementation MAIntentResult
@end

@implementation MANodeState
@end

@implementation MALinkInfo
@end

@implementation MAVanState
@end

// --- MatterBridge ---

@implementation MatterBridge {
    matter::DeviceManager _deviceManager;
    matter::SceneEngine* _sceneEngine;
    matter::IntentParser _intentParser;
    matter::sim::SimulationState _simState;
}

- (instancetype)init {
    self = [super init];
    if (self) {
        _sceneEngine = new matter::SceneEngine(_deviceManager);
    }
    return self;
}

- (void)dealloc {
    delete _sceneEngine;
}

#pragma mark - Device Management

- (void)addDeviceWithNodeId:(uint64_t)nodeId
                       name:(NSString *)name
                       room:(NSString *)room
                 deviceType:(uint32_t)deviceType {
    matter::Device device;
    device.nodeId = nodeId;
    device.name = [name UTF8String];
    device.room = [room UTF8String];
    device.reachable = true;

    // Add a default endpoint
    matter::Endpoint ep;
    ep.id = 1;
    ep.deviceType = static_cast<matter::DeviceType>(deviceType);

    // Initialize default attributes based on device type
    auto dt = static_cast<matter::DeviceType>(deviceType);
    uint32_t onOffKey = (static_cast<uint32_t>(matter::ClusterId::OnOff) << 16) |
                        static_cast<uint32_t>(matter::AttributeId::OnOff);
    uint32_t levelKey = (static_cast<uint32_t>(matter::ClusterId::LevelControl) << 16) |
                        static_cast<uint32_t>(matter::AttributeId::CurrentLevel);
    uint32_t tempKey = (static_cast<uint32_t>(matter::ClusterId::TemperatureMeas) << 16) |
                       static_cast<uint32_t>(matter::AttributeId::MeasuredValue);
    uint32_t lockKey = (static_cast<uint32_t>(matter::ClusterId::DoorLock) << 16) |
                       static_cast<uint32_t>(matter::AttributeId::LockState);

    switch (dt) {
        case matter::DeviceType::OnOffLight:
            ep.attributes[onOffKey] = matter::AttributeValue::fromBool(false);
            break;
        case matter::DeviceType::DimmableLight:
            ep.attributes[onOffKey] = matter::AttributeValue::fromBool(false);
            ep.attributes[levelKey] = matter::AttributeValue::fromInt(0);
            break;
        case matter::DeviceType::TempSensor:
            ep.attributes[tempKey] = matter::AttributeValue::fromFloat(20.0f);
            break;
        case matter::DeviceType::DoorLock:
            ep.attributes[lockKey] = matter::AttributeValue::fromBool(true);
            break;
        default:
            ep.attributes[onOffKey] = matter::AttributeValue::fromBool(false);
            break;
    }

    device.endpoints.push_back(std::move(ep));
    _deviceManager.addDevice(std::move(device));

    // Update intent parser with known rooms
    _intentParser.setKnownRooms(_deviceManager.roomNames());
}

- (void)updateAttributeForNode:(uint64_t)nodeId
                      endpoint:(uint16_t)endpointId
                       cluster:(uint32_t)clusterId
                     attribute:(uint32_t)attrId
                      boolValue:(BOOL)value {
    _deviceManager.updateAttribute(nodeId, endpointId,
                                    static_cast<matter::ClusterId>(clusterId),
                                    static_cast<matter::AttributeId>(attrId),
                                    matter::AttributeValue::fromBool(value));
}

- (void)updateAttributeForNode:(uint64_t)nodeId
                      endpoint:(uint16_t)endpointId
                       cluster:(uint32_t)clusterId
                     attribute:(uint32_t)attrId
                     floatValue:(float)value {
    _deviceManager.updateAttribute(nodeId, endpointId,
                                    static_cast<matter::ClusterId>(clusterId),
                                    static_cast<matter::AttributeId>(attrId),
                                    matter::AttributeValue::fromFloat(value));
}

- (void)updateAttributeForNode:(uint64_t)nodeId
                      endpoint:(uint16_t)endpointId
                       cluster:(uint32_t)clusterId
                     attribute:(uint32_t)attrId
                       intValue:(int64_t)value {
    _deviceManager.updateAttribute(nodeId, endpointId,
                                    static_cast<matter::ClusterId>(clusterId),
                                    static_cast<matter::AttributeId>(attrId),
                                    matter::AttributeValue::fromInt(value));
}

#pragma mark - Queries

- (NSArray<MADevice *> *)allDevices {
    NSMutableArray *result = [NSMutableArray array];
    for (const auto& dev : _deviceManager.devices()) {
        MADevice *d = [[MADevice alloc] init];
        d.nodeId = dev.nodeId;
        d.name = [NSString stringWithUTF8String:dev.name.c_str()];
        d.room = [NSString stringWithUTF8String:dev.room.c_str()];
        d.isOn = dev.isOn();
        d.reachable = dev.reachable;
        d.stateDescription = [NSString stringWithUTF8String:dev.stateDescription().c_str()];
        if (auto temp = dev.temperature()) {
            d.temperature = @(*temp);
        }
        if (auto lvl = dev.level()) {
            d.brightness = @(*lvl);
        }
        [result addObject:d];
    }
    return result;
}

- (NSArray<NSString *> *)roomNames {
    NSMutableArray *result = [NSMutableArray array];
    for (const auto& room : _deviceManager.roomNames()) {
        [result addObject:[NSString stringWithUTF8String:room.c_str()]];
    }
    return result;
}

- (NSString *)homeSummary {
    return [NSString stringWithUTF8String:_deviceManager.homeSummary().c_str()];
}

- (NSString *)roomSummary:(NSString *)room {
    return [NSString stringWithUTF8String:_deviceManager.roomSummary([room UTF8String]).c_str()];
}

#pragma mark - AI Intent Processing

- (MAIntentResult *)processNaturalLanguage:(NSString *)input {
    auto intent = _intentParser.parse([input UTF8String]);
    auto result = _sceneEngine->processIntent(intent);

    MAIntentResult *r = [[MAIntentResult alloc] init];
    r.response = [NSString stringWithUTF8String:result.response.c_str()];

    NSMutableArray *actions = [NSMutableArray array];
    for (const auto& action : result.actions) {
        MADeviceAction *a = [[MADeviceAction alloc] init];
        a.nodeId = action.nodeId;
        a.endpointId = action.endpointId;
        a.command = [NSString stringWithUTF8String:action.command.c_str()];
        [actions addObject:a];
    }
    r.actions = actions;
    return r;
}

#pragma mark - TLV Codec

- (NSData *)encodeTLVStructWithValues:(NSDictionary<NSNumber *, NSNumber *> *)tagValues {
    matter::tlv::Writer writer;
    writer.startStructure();
    for (NSNumber *tag in tagValues) {
        NSNumber *val = tagValues[tag];
        writer.putInt([tag unsignedCharValue], [val longLongValue]);
    }
    writer.endContainer();
    return [NSData dataWithBytes:writer.data().data() length:writer.size()];
}

- (NSDictionary<NSNumber *, NSNumber *> *)decodeTLVStruct:(NSData *)data {
    std::vector<uint8_t> bytes(static_cast<const uint8_t*>(data.bytes),
                                static_cast<const uint8_t*>(data.bytes) + data.length);
    auto elements = matter::tlv::Reader::decodeAll(bytes);

    NSMutableDictionary *result = [NSMutableDictionary dictionary];
    for (const auto& elem : elements) {
        if (elem.contextTag) {
            result[@(*elem.contextTag)] = @(elem.intVal);
        }
    }
    return result;
}

#pragma mark - Scenes

- (void)addSceneWithName:(NSString *)name description:(NSString *)desc {
    matter::Scene scene;
    scene.name = [name UTF8String];
    scene.description = [desc UTF8String];
    _sceneEngine->addScene(std::move(scene));
}

#pragma mark - Demo Data

- (void)loadDemoHome {
    // Living room
    [self addDeviceWithNodeId:1 name:@"Ceiling Light" room:@"Living Room"
                   deviceType:static_cast<uint32_t>(matter::DeviceType::DimmableLight)];
    [self addDeviceWithNodeId:2 name:@"Floor Lamp" room:@"Living Room"
                   deviceType:static_cast<uint32_t>(matter::DeviceType::DimmableLight)];
    [self addDeviceWithNodeId:3 name:@"Temperature Sensor" room:@"Living Room"
                   deviceType:static_cast<uint32_t>(matter::DeviceType::TempSensor)];

    // Kitchen
    [self addDeviceWithNodeId:4 name:@"Kitchen Lights" room:@"Kitchen"
                   deviceType:static_cast<uint32_t>(matter::DeviceType::OnOffLight)];
    [self addDeviceWithNodeId:5 name:@"Under Cabinet" room:@"Kitchen"
                   deviceType:static_cast<uint32_t>(matter::DeviceType::DimmableLight)];

    // Bedroom
    [self addDeviceWithNodeId:6 name:@"Bedside Lamp" room:@"Bedroom"
                   deviceType:static_cast<uint32_t>(matter::DeviceType::DimmableLight)];
    [self addDeviceWithNodeId:7 name:@"Thermostat" room:@"Bedroom"
                   deviceType:static_cast<uint32_t>(matter::DeviceType::TempSensor)];

    // Front door
    [self addDeviceWithNodeId:8 name:@"Front Door Lock" room:@"Hallway"
                   deviceType:static_cast<uint32_t>(matter::DeviceType::DoorLock)];
    [self addDeviceWithNodeId:9 name:@"Porch Light" room:@"Hallway"
                   deviceType:static_cast<uint32_t>(matter::DeviceType::OnOffLight)];

    // Set some devices on
    [self updateAttributeForNode:1 endpoint:1 cluster:0x0006 attribute:0x0000 boolValue:YES];
    [self updateAttributeForNode:1 endpoint:1 cluster:0x0008 attribute:0x0000 intValue:200];
    [self updateAttributeForNode:4 endpoint:1 cluster:0x0006 attribute:0x0000 boolValue:YES];
    [self updateAttributeForNode:3 endpoint:1 cluster:0x0402 attribute:0x0000 floatValue:21.5f];
    [self updateAttributeForNode:7 endpoint:1 cluster:0x0402 attribute:0x0000 floatValue:19.2f];

    // Add some scenes
    [self addSceneWithName:@"Movie" description:@"Dim living room, everything else off"];
    [self addSceneWithName:@"Bedtime" description:@"All lights off, lock doors"];
    [self addSceneWithName:@"Morning" description:@"Kitchen and hallway lights on"];
}

#pragma mark - Simulation Bridge

- (void)updateSimulationNodes:(NSString *)json {
    _simState.updateNodes([json UTF8String]);
}

- (void)updateSimulationTopology:(NSString *)json {
    _simState.updateTopology([json UTF8String]);
}

- (void)updateSimulationTimeline:(NSString *)json {
    _simState.updateTimeline([json UTF8String]);
}

- (void)updateSimulationMetrics:(NSString *)json {
    _simState.updateMetrics([json UTF8String]);
}

- (void)updateSimulationVans:(NSString *)json {
    _simState.updateVans([json UTF8String]);
}

- (void)updateSimulationAlerts:(NSString *)json {
    _simState.updateAlerts([json UTF8String]);
}

- (NSArray<MANodeState *> *)simulationNodes {
    NSMutableArray *result = [NSMutableArray array];
    for (const auto& node : _simState.nodes()) {
        MANodeState *n = [[MANodeState alloc] init];
        n.nodeId = node.nodeId;
        n.role = [NSString stringWithUTF8String:node.role.c_str()];
        n.state = [NSString stringWithUTF8String:node.state.c_str()];
        n.pid = node.pid;
        n.reachable = node.reachable;
        [result addObject:n];
    }
    return result;
}

- (MALinkInfo *)linkInfoFrom:(uint16_t)src to:(uint16_t)dst {
    if (src >= 4 || dst >= 4) return nil;
    const auto& link = _simState.topology()[src][dst];
    MALinkInfo *info = [[MALinkInfo alloc] init];
    info.lossPercent = link.lossPercent;
    info.latencyMs = link.latencyMs;
    info.up = link.up;
    info.lqi = link.lqi;
    info.rssi = link.rssi;
    return info;
}

- (NSArray<MAVanState *> *)simulationVans {
    NSMutableArray *result = [NSMutableArray array];
    for (const auto& van : _simState.vans()) {
        MAVanState *v = [[MAVanState alloc] init];
        v.vanId = [NSString stringWithUTF8String:van.vanId.c_str()];
        v.name = [NSString stringWithUTF8String:van.name.c_str()];
        v.state = [NSString stringWithUTF8String:van.state.c_str()];
        v.locked = van.locked;
        [result addObject:v];
    }
    return result;
}

- (NSString *)meshSummary {
    return [NSString stringWithUTF8String:_simState.meshSummary().c_str()];
}

- (NSString *)fleetSummary {
    return [NSString stringWithUTF8String:_simState.fleetSummary().c_str()];
}

- (BOOL)isMeshHealthy {
    return _simState.isHealthy();
}

- (NSString *)answerSimulationQuery:(NSString *)query {
    return [NSString stringWithUTF8String:_simState.answerSimulationQuery([query UTF8String]).c_str()];
}

@end
