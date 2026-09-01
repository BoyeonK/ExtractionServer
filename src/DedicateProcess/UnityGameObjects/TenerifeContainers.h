#pragma once

#include "Container.h"

class VehicleContainer : public Container {
public:
    static constexpr uint32_t VEHICLE_CONTAINER_VOLUME = 8;

protected:
    VehicleContainer(uint32_t objectId, ObjectType objectType, const Vector3& position, float yawAngle)
    : Container(objectId, objectType, true, position) {
        this->yawAngle = yawAngle;
        InitializeSlots(VEHICLE_CONTAINER_VOLUME);
    }
};

class TenerifeBlueCar : public VehicleContainer {
public:
    TenerifeBlueCar(uint32_t objectId, const Vector3& position, float yawAngle)
    : VehicleContainer(objectId, ObjectType::TenerifeBlueCar, position, yawAngle) {}
};

class TenerifeYellowCar : public VehicleContainer {
public:
    TenerifeYellowCar(uint32_t objectId, const Vector3& position, float yawAngle)
    : VehicleContainer(objectId, ObjectType::TenerifeYellowCar, position, yawAngle) {}
};

class TenerifeBrownCar : public VehicleContainer {
public:
    TenerifeBrownCar(uint32_t objectId, const Vector3& position, float yawAngle)
    : VehicleContainer(objectId, ObjectType::TenerifeBrownCar, position, yawAngle) {}
};

class TenerifeRedCar : public VehicleContainer {
public:
    TenerifeRedCar(uint32_t objectId, const Vector3& position, float yawAngle)
    : VehicleContainer(objectId, ObjectType::TenerifeRedCar, position, yawAngle) {}
};

class TenerifeBus : public VehicleContainer {
public:
    TenerifeBus(uint32_t objectId, const Vector3& position, float yawAngle)
    : VehicleContainer(objectId, ObjectType::TenerifeBus, position, yawAngle) {}
};
