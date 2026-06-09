#include "texture_manager.h"
#include "engine3d.h"


TextureManager::TextureManager(Engine3D& engine, const std::string& root_path) {
    init_textures(engine, root_path);
}

void TextureManager::init_textures(Engine3D& engine, const std::string& root_path) {
    fs::path p(root_path);


    Cube cube;
    Quad quad;

    int size = 4096;
    Framebuffer capture_fbo(size, size);
    capture_fbo.createDepthRenderbuffer(GL_DEPTH_COMPONENT24);
    capture_fbo.setDrawBuffer(GL_COLOR_ATTACHMENT0);

    if (!capture_fbo.isComplete())
        throw;
    
    brdf_lut = PBREnvironment::generate_brdf_lut(engine, quad.mesh);

    citrus_orchard_puresky_4k = PBREnvironment(engine, (p / "assets/hdr/citrus_orchard_puresky_4k.hdr").string(),
                                               brdf_lut, capture_fbo, cube.mesh, size);
    studio_kominka_02_4k = PBREnvironment(engine, (p / "assets/hdr/studio_kominka_02_4k.hdr").string(),
                                               brdf_lut, capture_fbo, cube.mesh, size);
    st_peters_square_night_4k = PBREnvironment(engine, (p / "assets/hdr/st_peters_square_night_4k.hdr").string(),
                                               brdf_lut, capture_fbo, cube.mesh, size);
    qwantani_moonrise_puresky_4k = PBREnvironment(engine, (p / "assets/hdr/qwantani_moonrise_puresky_4k.hdr").string(),
                                               brdf_lut, capture_fbo, cube.mesh, size);
}