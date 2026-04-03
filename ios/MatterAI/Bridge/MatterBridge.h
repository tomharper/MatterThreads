#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/// Represents a device visible to Swift
@interface MADevice : NSObject
@property (nonatomic, assign) uint64_t nodeId;
@property (nonatomic, copy) NSString *name;
@property (nonatomic, copy) NSString *room;
@property (nonatomic, copy) NSString *vendorName;
@property (nonatomic, assign) BOOL isOn;
@property (nonatomic, assign) BOOL reachable;
@property (nonatomic, copy) NSString *stateDescription;
@property (nonatomic, strong, nullable) NSNumber *temperature;
@property (nonatomic, strong, nullable) NSNumber *humidity;
@property (nonatomic, strong, nullable) NSNumber *brightness;
@property (nonatomic, strong, nullable) NSNumber *battery;
@property (nonatomic, assign) BOOL isLocked;
@property (nonatomic, assign) BOOL hasToggle;
@end

/// Action to execute on a device
@interface MADeviceAction : NSObject
@property (nonatomic, assign) uint64_t nodeId;
@property (nonatomic, assign) uint16_t endpointId;
@property (nonatomic, copy) NSString *command;
@end

/// Result from processing a user query
@interface MAIntentResult : NSObject
@property (nonatomic, copy) NSString *response;
@property (nonatomic, strong) NSArray<MADeviceAction *> *actions;
@end

/// Bridge from C++ Matter engine to Swift
@interface MatterBridge : NSObject

- (instancetype)init;

/// Device management
- (void)addDeviceWithNodeId:(uint64_t)nodeId
                       name:(NSString *)name
                       room:(NSString *)room
                 deviceType:(uint32_t)deviceType;

- (void)updateAttributeForNode:(uint64_t)nodeId
                      endpoint:(uint16_t)endpointId
                       cluster:(uint32_t)clusterId
                     attribute:(uint32_t)attrId
                      boolValue:(BOOL)value;

- (void)updateAttributeForNode:(uint64_t)nodeId
                      endpoint:(uint16_t)endpointId
                       cluster:(uint32_t)clusterId
                     attribute:(uint32_t)attrId
                     floatValue:(float)value;

- (void)updateAttributeForNode:(uint64_t)nodeId
                      endpoint:(uint16_t)endpointId
                       cluster:(uint32_t)clusterId
                     attribute:(uint32_t)attrId
                       intValue:(int64_t)value;

/// Query
- (NSArray<MADevice *> *)allDevices;
- (NSArray<NSString *> *)roomNames;
- (NSString *)homeSummary;
- (NSString *)roomSummary:(NSString *)room;

/// AI Intent processing
- (MAIntentResult *)processNaturalLanguage:(NSString *)input;

/// TLV codec (for demo/testing)
- (NSData *)encodeTLVStructWithValues:(NSDictionary<NSNumber *, NSNumber *> *)tagValues;
- (NSDictionary<NSNumber *, NSNumber *> *)decodeTLVStruct:(NSData *)data;

/// Scene management
- (void)addSceneWithName:(NSString *)name description:(NSString *)desc;

/// Load demo home data
- (void)loadDemoHome;

/// Simulation: feed JSON responses from Dashboard/Gateway APIs
- (void)updateSimulationNodes:(NSString *)json;
- (void)updateSimulationTopology:(NSString *)json;
- (void)updateSimulationTimeline:(NSString *)json;
- (void)updateSimulationMetrics:(NSString *)json;
- (void)updateSimulationVans:(NSString *)json;
- (void)updateSimulationAlerts:(NSString *)json;

/// Simulation: query state
- (NSArray *)simulationNodes;
- (nullable id)linkInfoFrom:(uint16_t)src to:(uint16_t)dst;
- (NSArray *)simulationVans;
- (NSString *)meshSummary;
- (NSString *)fleetSummary;
- (BOOL)isMeshHealthy;
- (NSString *)answerSimulationQuery:(NSString *)query;

@end

/// Simulation node state
@interface MANodeState : NSObject
@property (nonatomic, assign) uint16_t nodeId;
@property (nonatomic, copy) NSString *role;
@property (nonatomic, copy) NSString *state;
@property (nonatomic, assign) int pid;
@property (nonatomic, assign) BOOL reachable;
@end

/// Simulation link info
@interface MALinkInfo : NSObject
@property (nonatomic, assign) float lossPercent;
@property (nonatomic, assign) float latencyMs;
@property (nonatomic, assign) BOOL up;
@property (nonatomic, assign) uint8_t lqi;
@property (nonatomic, assign) int8_t rssi;
@end

/// Van state from Gateway API
@interface MAVanState : NSObject
@property (nonatomic, copy) NSString *vanId;
@property (nonatomic, copy) NSString *name;
@property (nonatomic, copy) NSString *state;
@property (nonatomic, assign) BOOL locked;
@end

NS_ASSUME_NONNULL_END
