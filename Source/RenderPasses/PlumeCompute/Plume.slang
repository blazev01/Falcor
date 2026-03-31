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
import SmokeLayer;

struct WindStructure
{
    int intensity;
    float angle;
    float3 windVector;
};

struct EruptionParams
{
    float speed;
    float altitude;
    float radius;
    float maxRadius;
    float density;
};

struct Plume
{
    uint id;

    // Core parameters
    float T0;
    float theta0;
    float U0;
    float n0;
    float z0;
    float r0;
    float rho0;

    float expansionStrength;
    float radiusMultiplier;

    // Simulation trackers
    float tStep;
    float newLayerDelay;
    float fMaxRadius;

    float airIncorporationCoeff;
    float stagnationSpeed;

    uint nbIterations;
    uint lastPpeLayerIdx;

    uint freeSphereID;
    uint fallingSphereID;

    // Smoke transition
    int maxSmoke;
    float transitionSpeed;
    float transitionDelay;

    float g;
    float minLifetime;
    float maxLifetime;

    float3 ventPosition;

    uint subspheresNumber;
    uint subsubspheresNumber;
};

// Smoke layers
RWStructuredBuffer<SmokeLayer> gSmokeLayers;

// Free spheres
RWStructuredBuffer<FreeSphereParams> gFreeSpheres;

// Sub spheres
RWStructuredBuffer<SubsphereParams> gS2Spheres;
RWStructuredBuffer<SubsphereParams> gS3Spheres;

// Special sphere groups
RWStructuredBuffer<FreeSphereParams> gStagnateSpheres;
RWStructuredBuffer<FreeSphereParams> gFallingSpheres;

// Lifetimes
RWStructuredBuffer<float> gSphereLifetime;
RWStructuredBuffer<float> gTransitionLifetime;

// Plumes
Plume gPlume;
WindStructure gWind;

float3 computeWindVector(int intensity, float angle)
{
    return intensity * float3(cos(angle), sin(angle), 0);
}

static const float PI = 3.14159265;

// Atmosphere
float computeAtmTemperature(float height)
{
    return 288.15 - 6.5 * height / 1000.0;
}

float computeAtmDensity(float height)
{
    return 352.995 * pow(1 - 0.0000225577 * height, 5.25516) 
           / (288.15 - 0.0065 * height);
}

[numthreads(128, 1, 1)]
void updateSmokeLayersCS(uint3 tid: SV_DispatchThreadID)
{
    uint i = tid.x;
    SmokeLayer layer = gSmokeLayers[i];

    float dMass = 0.0;

    layer.lifetime += gPlume.tStep;

    float atmDensity = computeAtmDensity(layer.center.z);

    // --------------------------------------------------
    // Sedimentation
    // --------------------------------------------------
    if (layer.plume != 0 && layer.center.z > 0.0)
    {
        float volume = PI * layer.r * layer.r * layer.thickness;

        float diffDensity = 0.00000005 * gPlume.tStep;
        if (layer.stagnatesLong != 0)
            diffDensity = 0.00005 * gPlume.tStep;

        if (layer.rho > diffDensity)
        {
            layer.rho -= diffDensity;
            dMass -= diffDensity * volume;
        }
    }

    // --------------------------------------------------
    // Air incorporation (convection)
    // --------------------------------------------------
    if (layer.rising != 0 && layer.stagnatesLong == 0)
    {
        float thk = gPlume.tStep * layer.v.z;

        float totalVolume = layer.thickness * PI * layer.r * layer.r;
        float totalMass = layer.rho * totalVolume;

        float windNorm = length(gWind.intensity);
        float vNorm = length(layer.v);

        float k_s = 0.09;
        float k_w = 0.9;

        float Ue = k_s * abs(vNorm - windNorm * cos(layer.theta)) +
                   k_w * abs(windNorm * sin(layer.theta));

        float rAtm = gPlume.airIncorporationCoeff * Ue;

        float atmVolume = thk * PI * (2.0 * layer.r + rAtm) * rAtm;
        float atmMass = atmDensity * atmVolume;

        float newTemp =
            (totalMass * layer.temperature +
             atmMass * computeAtmTemperature(layer.center.z))
            / (totalMass + atmMass);

        layer.temperature = newTemp;

        float atmNewVolume =
            layer.temperature * atmVolume /
            computeAtmTemperature(layer.center.z);

        dMass = atmMass;

        float massNew = totalMass + atmMass;
        float volumeNew = totalVolume + atmNewVolume;

        float rhoNew = massNew / volumeNew;
        float rNew = pow(volumeNew / PI, 1.0 / 3.0);

        if (layer.rho > atmDensity && rhoNew < atmDensity)
            layer.plume = 1;

        layer.r = rNew;
        layer.rho = rhoNew;
        layer.thickness = rNew;
    }

    // --------------------------------------------------
    // Forces
    // --------------------------------------------------
    float volume = layer.thickness * PI * layer.r * layer.r;
    float mass = layer.rho * volume;

    float surface = 2.0 * PI * layer.r * layer.r;
    float surfaceEff = 2.0 * layer.r * layer.r;

    float3 vDiff = gWind.windVector - float3(layer.v.x, layer.v.y, 0);

    float3 weight = float3(0, 0, -mass * gPlume.g);
    float3 buoyancy = float3(0, 0, atmDensity * volume * gPlume.g);

    float3 friction =
        -0.5 * atmDensity * 0.04 * surface * length(layer.v) * layer.v;

    float3 windForce =
        600.0 * length(vDiff) * vDiff * surfaceEff;

    float3 forces = weight + buoyancy + friction + windForce;

    float oldMass = mass - dMass;

    float3 v = (oldMass * layer.v + forces * gPlume.tStep) / mass;
    float3 p = layer.center + v * gPlume.tStep;

    // State transitions
    if (layer.v.z > 0 && v.z < 0 && layer.plume == 0)
    {
        layer.rising = 0;
        layer.beginFalling = 1;
    }

    if (layer.plume != 0 && layer.stagnates == 0 &&
        layer.center.z > 5000 &&
        (atmDensity - layer.rho < 0.01))
    {
        layer.stagnates = 1;
    }

    if (layer.stagnates != 0 && v.z < 0)
        layer.stagnatesLong = 1;

    if (layer.stagnates != 0 && p.z < layer.center.z)
        p.z = layer.center.z;

    layer.v = v;
    layer.center = p;
    layer.a = v / gPlume.tStep;

    // Axis update
    layer.speedAlongAxis = length(v);
    layer.plumeAxis = normalize(v);

    gSmokeLayers[i] = layer;
}
