#include "chunk.h"
#include "voxel_grid.h"

Chunk::Chunk(glm::ivec3 size, glm::vec3 voxel_size) {
    this->size = size;
    this->voxel_size = voxel_size;

    const size_t count = (size_t)size.x * (size_t)size.y * (size_t)size.z;

    auto v = std::make_shared<std::vector<Voxel>>(count, Voxel{});
    std::atomic_store(&voxels, std::shared_ptr<const std::vector<Voxel>>(v));

    this->vertex_layout = new VertexLayout();
    vertex_layout->add("position", 0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), 0, 0, {0.0f, 0.0f, 0.0f});
    vertex_layout->add("normal", 1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), 3 * sizeof(float), 0, {0.0f, 1.0f, 0.0f});
    vertex_layout->add("color", 2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), 6 * sizeof(float), 0, {1.0f, 1.0f, 1.0f});
}


Chunk::~Chunk() {
    delete mesh;
    delete vertex_layout;
}

void Chunk::clear_voxels() {
    edit_voxels([](std::vector<Voxel>& voxels) {
        for (auto& v : voxels) {
            Voxel voxel;
            voxel.color = glm::vec3(1.0f);
            voxel.visible = false;
            
            v = voxel;
        }
    });
}

MeshData Chunk::build(const std::vector<Voxel>& voxels, glm::ivec3 size) {
    static const glm::ivec3 adjacent_dir[] = {
        {-1, 0, 0}, { 1, 0, 0},
        { 0, 0,-1}, { 0, 0, 1},
        { 0, 1, 0}, { 0,-1, 0}
    };

    MeshData out;

    for (int x = 0; x < size.x; x++) {
        for (int y = 0; y < size.y; y++) {
            for (int z = 0; z < size.z; z++) {
                glm::ivec3 pos = glm::ivec3(x, y, z);

                if (is_free(voxels, pos, size))
                    continue;
                
                size_t id = idx(pos, size);
                glm::vec3 color = voxels[id].color;

                for (int i = 0; i < 6; i++) {
                    glm::ivec3 adjacent_pos = pos + adjacent_dir[i];
                    
                    if (!is_free(voxels, adjacent_pos, size))
                        continue;
                    
                    emit_face(out, pos, (Face)i, color);
                }
            }
        }
    }

    return out;
}


MeshData Chunk::build(const std::vector<Voxel>& self, 
                     const std::array<std::shared_ptr<const std::vector<Voxel>>,6>& nb, 
                     glm::ivec3 size){
    static const glm::ivec3 adjacent_dir[] = {
        {-1, 0, 0}, { 1, 0, 0},
        { 0, 0,-1}, { 0, 0, 1},
        { 0, 1, 0}, { 0,-1, 0}
    };

    MeshData out;

    for (int x = 0; x < size.x; x++) {
        for (int y = 0; y < size.y; y++) {
            for (int z = 0; z < size.z; z++) {
                glm::ivec3 pos = glm::ivec3(x, y, z);

                if (!solid_from(self, nb, pos, size))
                    continue;
                
                size_t id = idx(pos, size);
                glm::vec3 color = self[id].color;

                for (int i = 0; i < 6; i++) {
                    glm::ivec3 adjacent_pos = pos + adjacent_dir[i];
                    
                    if (solid_from(self, nb, adjacent_pos, size))
                        continue;
                    
                    emit_face(out, pos, (Face)i, color);
                }
            }
        }
    }

    return out;
}

void Chunk::push_vertex(std::vector<float>& v, const glm::vec3& pos, const glm::vec3& normal, const glm::vec3& color) {
    v.push_back(pos.x);
    v.push_back(pos.y);
    v.push_back(pos.z);

    v.push_back(normal.x);
    v.push_back(normal.y);
    v.push_back(normal.z);

    v.push_back(color.x);  
    v.push_back(color.y);
    v.push_back(color.z);
}

void Chunk::emit_face(MeshData& out, glm::ivec3 pos, Face f, glm::vec3 color) {
    constexpr int STRIDE = 9;

    static const glm::vec3 normals[] = {
        {-1, 0, 0}, { 1, 0, 0},
        { 0, 0,-1}, { 0, 0, 1},
        { 0, 1, 0}, { 0,-1, 0}
    };

    static const glm::ivec3 corners[][4] = {
        // Left
        { {0,0,0},{0,1,0},{0,1,1},{0,0,1} },
        // Right
        { {1,0,1},{1,1,1},{1,1,0},{1,0,0} },
        // Back (-Z)
        { {0,0,0},{0,1,0},{1,1,0},{1,0,0} },
        // Front (+Z)
        { {0,0,1},{0,1,1},{1,1,1},{1,0,1} },
        // Top (+Y)
        { {0,1,1},{0,1,0},{1,1,0},{1,1,1} },
        // Bottom (-Y)
        { {0,0,0},{0,0,1},{1,0,1},{1,0,0} }
    };

    const int fi = static_cast<int>(f);
    const glm::vec3 n = normals[fi];

    const unsigned int base = static_cast<unsigned int>(out.vertices.size() / STRIDE);
    
    for (int i = 0; i < 4; ++i) {
        glm::vec3 p = glm::vec3(pos + corners[fi][i]);
        Chunk::push_vertex(out.vertices, p, n, color);
    }

    out.indices.push_back(base + 0);
    out.indices.push_back(base + 1);
    out.indices.push_back(base + 2);
    out.indices.push_back(base + 2);
    out.indices.push_back(base + 3);
    out.indices.push_back(base + 0);
}


bool Chunk::in_bounds(glm::ivec3 pos, glm::ivec3 size) {
    return (0 <= pos.x && pos.x < size.x) &&
           (0 <= pos.y && pos.y < size.y) &&
           (0 <= pos.z && pos.z < size.z);
}

size_t Chunk::idx(glm::ivec3 pos, glm::ivec3 size) {
    return (size_t)pos.x + (size_t)size.x * ((size_t)pos.y + (size_t)size.y * (size_t)pos.z);
}

bool Chunk::is_free(const std::vector<Voxel>& voxels, glm::ivec3 pos, glm::ivec3 size) {
    if (!in_bounds(pos, size)) 
        return true;
    return !voxels[idx(pos, size)].visible; 
}

void Chunk::set_voxels(const std::vector<Voxel>& voxels, const std::vector<glm::ivec3>& positions) {
    edit_voxels([&](std::vector<Voxel>& current){
        for (int i = 0; i < voxels.size(); i++) {
            glm::ivec3 pos = positions[i];
            if (!in_bounds(pos, size))
                continue;
            current[idx(pos, size)] = voxels[i];
        }
    });
}

void Chunk::set_voxel(const Voxel& voxel, glm::ivec3 position) {
    edit_voxels([&](std::vector<Voxel>& current) {
        if (in_bounds(position, size))
            current[idx(position, size)] = voxel;
    });
}

Voxel Chunk::get_voxel(glm::ivec3 position) const {
    auto cur = std::atomic_load(&voxels);
    if (!in_bounds(position, size)) {
        throw std::out_of_range("Chunk::get_voxel: position out of bounds");
    }

    return (*cur)[idx(position, size)];
}

void Chunk::upload_mesh_gpu(MeshData& mesh_data) {
    if (mesh_data.vertices.empty() || mesh_data.indices.empty()) {
        empty_mesh = true;
        return;
    }
    empty_mesh = false;
        
    if (!mesh)
        this->mesh = new Mesh(mesh_data.vertices, mesh_data.indices, *vertex_layout);
    else
        this->mesh->update(mesh_data.vertices, mesh_data.indices);
}

bool Chunk::solid_from(const std::vector<Voxel>& self,
                       const std::array<std::shared_ptr<const std::vector<Voxel>>,6>& nb,
                       glm::ivec3 pos, glm::ivec3 csize){
    if (in_bounds(pos, csize))
        return self[idx(pos, csize)].visible;

    // Only allow "one step outside" cases. Everything else = air.
    if (pos.x == -1) {
        auto& n = nb[(int)Face::Left];
        if (!n) return false;
        return (*n)[idx({csize.x-1, pos.y, pos.z}, csize)].visible;
    }
    if (pos.x == csize.x) {
        auto& n = nb[(int)Face::Right];
        if (!n) return false;
        return (*n)[idx({0, pos.y, pos.z}, csize)].visible;
    }
    if (pos.z == -1) {
        auto& n = nb[(int)Face::Back];
        if (!n) return false;
        return (*n)[idx({pos.x, pos.y, csize.z-1}, csize)].visible;
    }
    if (pos.z == csize.z) {
        auto& n = nb[(int)Face::Front];
        if (!n) return false;
        return (*n)[idx({pos.x, pos.y, 0}, csize)].visible;
    }
    if (pos.y == -1) {
        auto& n = nb[(int)Face::Bottom];
        if (!n) return false;
        return (*n)[idx({pos.x, csize.y-1, pos.z}, csize)].visible;
    }
    if (pos.y == csize.y) {
        auto& n = nb[(int)Face::Top];
        if (!n) return false;
        return (*n)[idx({pos.x, 0, pos.z}, csize)].visible;
    }
    return false;
}


void Chunk::draw(RenderState state) {
    if (!mesh || empty_mesh)
        return;
    
    state.transform *= get_model_matrix();
    mesh->draw(state);
}