#include "pbr_skybox.h"


PBRSkybox::PBRSkybox(PBREnvironment &pbr_environment) : Skybox(pbr_environment.environment), pbr_environment(pbr_environment){
}

void PBRSkybox::attach_environment(Program& program) {
    program.use();
    pbr_environment.irradiance.bind(0);
    program.set_int("irradianceMap", 0);

    pbr_environment.prefilter.bind(1);
    program.set_int("prefilterMap", 1);
    program.set_float("uPrefilterMaxMip", float(pbr_environment.prefilter.mipLevels() - 1));

    // glBindTextureUnit(2, pbr_environment.brdf_lut);
    pbr_environment.brdf_lut.bind(2);
    program.set_int("uBrdfLUT", 2);
}