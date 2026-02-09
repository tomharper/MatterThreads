#include "hw/darwin/DarwinBridge.h"
#include "core/Log.h"

#ifdef __APPLE__

#import <Foundation/Foundation.h>

#if __has_include(<Matter/Matter.h>) && HAS_MATTER_FRAMEWORK
#define DARWIN_MATTER_AVAILABLE 1
#import <Matter/Matter.h>
#else
#define DARWIN_MATTER_AVAILABLE 0
#endif

namespace mt::hw::darwin {

#if DARWIN_MATTER_AVAILABLE

// DarwinNode: IHardwareNode backed by MTRDeviceController
class DarwinNode : public IHardwareNode {
public:
    DarwinNode(uint64_t device_id, std::string name, const DarwinControllerConfig& config)
        : device_id_(device_id), name_(std::move(name)), config_(config) {
        MT_INFO("DarwinNode", "initializing Darwin Framework controller for " + name_);
        initController();
    }

    ~DarwinNode() override {
        if (controller_) {
            [controller_ shutdown];
        }
    }

    uint64_t deviceId() const override { return device_id_; }
    std::string name() const override { return name_; }

    Result<void> commission(uint32_t setup_code) override {
        if (!controller_) return Error("Darwin controller not initialized");

        @autoreleasepool {
            NSError* error = nil;
            MTRSetupPayload* payload = [[MTRSetupPayload alloc] init];
            payload.setUpPINCode = @(setup_code);

            // Commission using the setup code
            MT_INFO("DarwinNode", "commissioning device " + std::to_string(device_id_) + " via Darwin Framework");

            // Start pairing — this is a simplified flow
            // Real implementation would handle the full async commissioning
            commissioned_ = true;
            return Result<void>::success();
        }
    }

    Result<void> openCASESession() override {
        if (!commissioned_) return Error("Device not commissioned");
        // CASE sessions are managed internally by MTRDeviceController
        return Result<void>::success();
    }

    bool isCommissioned() const override { return commissioned_; }

    Result<AttributeValue> readAttribute(
        EndpointId ep, ClusterId cluster, AttributeId attr) override {
        if (!commissioned_) return Error("Device not commissioned");

        @autoreleasepool {
            MTRDevice* device = [MTRDevice deviceWithNodeID:@(device_id_) controller:controller_];
            if (!device) return Error("Failed to get MTRDevice");

            MTRAttributePath* path = [MTRAttributePath attributePathWithEndpointID:@(ep)
                                                                         clusterID:@(cluster)
                                                                       attributeID:@(attr)];

            NSDictionary* result = [device readAttributeWithEndpointID:@(ep)
                                                            clusterID:@(cluster)
                                                          attributeID:@(attr)
                                                               params:nil];

            if (!result) return Error("Read returned nil");

            // Convert NSObject value to AttributeValue
            id value = result[@"value"];
            if ([value isKindOfClass:[NSNumber class]]) {
                NSNumber* num = (NSNumber*)value;
                if (strcmp([num objCType], @encode(BOOL)) == 0) {
                    return AttributeValue{static_cast<bool>([num boolValue])};
                }
                return AttributeValue{static_cast<uint32_t>([num unsignedIntValue])};
            } else if ([value isKindOfClass:[NSString class]]) {
                return AttributeValue{std::string([(NSString*)value UTF8String])};
            }

            return Error("Unsupported value type");
        }
    }

    Result<void> writeAttribute(
        EndpointId ep, ClusterId cluster, AttributeId attr,
        const AttributeValue& value) override {
        if (!commissioned_) return Error("Device not commissioned");
        // Simplified — real impl would convert value to NSObject and use MTRDevice write
        return Result<void>::success();
    }

    Result<InvokeResponseData> invokeCommand(
        EndpointId ep, ClusterId cluster, CommandId cmd,
        const std::vector<uint8_t>& payload) override {
        if (!commissioned_) return Error("Device not commissioned");
        InvokeResponseData response;
        response.command = {ep, cluster, cmd};
        response.status_code = 0;
        return response;
    }

    Result<SubscriptionId> subscribe(
        EndpointId ep, ClusterId cluster, AttributeId attr,
        Duration min_interval, Duration max_interval,
        std::function<void(const AttributeValue&)> on_report) override {
        if (!commissioned_) return Error("Device not commissioned");
        // Simplified — real impl would use MTRDevice subscription APIs
        return SubscriptionId{0};
    }

    void cancelSubscription(SubscriptionId) override {}
    void tick() override {}

private:
    uint64_t device_id_;
    std::string name_;
    DarwinControllerConfig config_;
    bool commissioned_ = false;
    MTRDeviceController* controller_ = nil;

    void initController() {
        @autoreleasepool {
            NSError* error = nil;

            // Set up storage
            NSString* storagePath = [NSString stringWithUTF8String:config_.storage_path.c_str()];
            if (storagePath.length == 0) {
                storagePath = [NSTemporaryDirectory() stringByAppendingPathComponent:@"mt-darwin"];
            }

            // Create controller factory and get shared instance
            MTRDeviceControllerFactory* factory = [MTRDeviceControllerFactory sharedInstance];

            MTRDeviceControllerFactoryParams* factoryParams =
                [[MTRDeviceControllerFactoryParams alloc] initWithStorage:
                    [[MTRDeviceControllerStorageDelegate alloc] init]];

            if (![factory isRunning]) {
                [factory startControllerFactory:factoryParams error:&error];
                if (error) {
                    MT_WARN("DarwinNode", std::string("failed to start controller factory: ") +
                            [[error localizedDescription] UTF8String]);
                    return;
                }
            }

            // Create controller
            MTRDeviceControllerStartupParams* startupParams =
                [[MTRDeviceControllerStartupParams alloc] initWithIPK:
                    [[NSData alloc] initWithBytes:"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0" length:16]
                    fabricID:@(config_.fabric_id)
                    nocSigner:[[MTRTestKeys alloc] init]];

            controller_ = [factory createControllerOnNewFabric:startupParams error:&error];
            if (error) {
                MT_WARN("DarwinNode", std::string("failed to create controller: ") +
                        [[error localizedDescription] UTF8String]);
            }
        }
    }
};

std::unique_ptr<IHardwareNode> createDarwinController(
    uint64_t device_id, const std::string& name, const DarwinControllerConfig& config) {
    return std::make_unique<DarwinNode>(device_id, name, config);
}

bool isDarwinMatterAvailable() {
    return true;
}

#else // !DARWIN_MATTER_AVAILABLE

std::unique_ptr<IHardwareNode> createDarwinController(
    uint64_t, const std::string&, const DarwinControllerConfig&) {
    MT_WARN("DarwinBridge", "Matter.framework not available — build connectedhomeip Darwin first");
    return nullptr;
}

bool isDarwinMatterAvailable() {
    return false;
}

#endif // DARWIN_MATTER_AVAILABLE

} // namespace mt::hw::darwin

#endif // __APPLE__
