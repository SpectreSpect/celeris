#pragma once
#include "skybox.h"
#include "pbr_environment.h"
#include <memory>
#include "program.h"

class PBRSkybox : public Skybox {
public:
    PBREnvironment& pbr_environment;
    PBRSkybox(PBREnvironment &pbr_environment);
    void attach_environment(Program& program);
};