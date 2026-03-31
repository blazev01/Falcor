/***************************************************************************
 # Copyright (c) 2015-24, NVIDIA CORPORATION. All rights reserved.
 #
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted provided that the following conditions
 # are met:
 #  * Redistributions of source code must retain the above copyright
 #    notice, this list of conditions and the following disclaimer.
 #  * Redistributions in binary form must reproduce the above copyright
 #    notice, this list of conditions and the following disclaimer in the
 #    documentation and/or other materials provided with the distribution.
 #  * Neither the name of NVIDIA CORPORATION nor the names of its
 #    contributors may be used to endorse or promote products derived
 #    from this software without specific prior written permission.
 #
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS "AS IS" AND ANY
 # EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 # IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 # PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 # CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 # EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 # PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 # PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 # OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 # (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 # OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **************************************************************************/

struct SubsphereParams
{
    uint parentID;
    float3 relativePosition;

    float3 center;
    float r;
    float sizeRatio;
    float lifetime;
};

struct FreeSphereParams
{
    // Characteristics
    uint id;
    float3 center;
    float r;

    float3 speed;
    float mass;
    float rho;
    float relativeDistance;
    float lifetime;

    // Rotation
    float angularSpeed;
    float3 rotationAxis;
    float currentAngle;
    float3 angleVector;

    // Diversity
    float perturbation;
    float sizeFactor;

    // State flags (use uint instead of bool)
    uint fallingUnderAtmRho;
    uint stagnate;
    uint stagnateLong;
    uint falling;
    uint fallingDisappeared;
    uint secondaryColumn;

    // Stagnation data
    int closestLayerIdx;
    float stagnationAltitude;
    float maxAltitude;
    float xyAtMaxAltitude;
    float3 centerAtMaxAltitude;
    float3 layerCenterAtMaxAltitude;
};

struct SmokeLayer
{
    float3 center;
    float3 v;
    float3 a;

    float3 plumeAxis;
    float speedAlongAxis;
    float theta;
    float3 thetaAxis;

    float r;
    float temperature;
    float rho;
    float thickness;
    float lifetime;

    // State flags
    uint rising;
    uint beginFalling;
    uint falling;
    uint plume;
    uint stagnates;
    uint stagnatesLong;
    uint secondaryPlume;
};


SubsphereParams createSubsphere()
{
    SubsphereParams s;
    s.lifetime = 0.0;
    return s;
}

FreeSphereParams createFreeSphere(
    uint sphereID,
    float3 ringCenter,
    float angleOnRing,
    float r,
    float speedZ,
    uint secondaryColumn
)
{
    FreeSphereParams s;

    float3 angleNormal = float3(cos(angleOnRing), sin(angleOnRing), 0);

    s.id = sphereID;
    s.r = r;
    s.speed = float3(0, 0, speedZ);
    s.secondaryColumn = secondaryColumn;

    s.angleVector = angleNormal;
    s.center = ringCenter + r * angleNormal;

    s.rotationAxis = float3(-sin(angleOnRing), cos(angleOnRing), 0);

    s.lifetime = 0.0;
    s.angularSpeed = speedZ / r;
    s.currentAngle = 0.0;

    s.perturbation = 0.0;
    s.stagnate = 0;
    s.stagnateLong = 0;
    s.falling = 0;
    s.fallingDisappeared = 0;

    return s;
}

FreeSphereParams createFallingSphere(
    uint sphereID,
    float3 center,
    float r,
    float rho
)
{
    FreeSphereParams s;

    s.id = sphereID;
    s.center = center;
    s.r = r;
    s.rho = rho;

    s.speed = float3(0, 0, 0);
    s.angularSpeed = 0.0;

    s.rotationAxis = float3(1, 0, 0);
    s.currentAngle = 0.0;
    s.angleVector = float3(0, 1, 0);

    s.mass = rho * (4.0 / 3.0) * 3.14159265 * r * r * r;

    s.sizeFactor = 1.0;

    s.fallingUnderAtmRho = 0;
    s.stagnate = 0;
    s.stagnateLong = 0;
    s.falling = 1;
    s.fallingDisappeared = 0;

    s.lifetime = 0.0;

    return s;
}

SmokeLayer createSmokeLayerDefault()
{
    SmokeLayer s;

    s.center = float3(0, 0, 0);
    s.v = float3(0, 0, 200);
    s.a = float3(0, 0, 0);

    s.speedAlongAxis = 200.0;
    s.theta = asin(1.0);

    s.plumeAxis = float3(0, 0, 1);
    s.thetaAxis = float3(1, 0, 0);

    s.r = 100.0;
    s.temperature = 1273.0;
    s.rho = 1.5;
    s.thickness = 100.0;

    s.rising = 1;
    s.beginFalling = 0;
    s.falling = 0;
    s.plume = 0;
    s.stagnates = 0;
    s.stagnatesLong = 0;
    s.secondaryPlume = 0;

    return s;
}

SmokeLayer createSmokeLayer(float3 v, float rho, float3 position, uint secondaryPlume)
{
    SmokeLayer s;

    s.center = float3(0, 0, 0);
    s.v = v;
    s.a = float3(0, 0, 0);

    float speed = length(v);

    s.speedAlongAxis = speed;
    s.theta = asin(1.0);
    s.plumeAxis = normalize(v);

    s.r = 100.0;
    s.temperature = 1273.0;
    s.rho = rho;
    s.thickness = 100.0;

    s.rising = 1;
    s.beginFalling = 0;
    s.falling = 0;
    s.plume = 0;
    s.stagnates = 0;
    s.stagnatesLong = 0;
    s.secondaryPlume = secondaryPlume;

    if (speed == v.z)
        s.thetaAxis = float3(1, 0, 0);
    else
        s.thetaAxis = normalize(cross(v, float3(0, 0, 1)));

    return s;
}
