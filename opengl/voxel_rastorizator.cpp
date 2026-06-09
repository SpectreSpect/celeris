#include "voxel_rastorizator.h"


VoxelRastorizator::VoxelRastorizator(IGridable* gridable) {
    this->gridable = gridable;
}

float VoxelRastorizator::fmin3(float a, float b, float c) { return std::min(a, std::min(b,c)); }
float VoxelRastorizator::fmax3(float a, float b, float c) { return std::max(a, std::max(b,c)); }

bool VoxelRastorizator::plane_box_overlap(const glm::vec3& normal, const glm::vec3& vert, const glm::vec3& maxbox)
{
    glm::vec3 vmin, vmax;
    for (int q = 0; q < 3; ++q) {
        float v = vert[q];
        if (normal[q] > 0.0f) { vmin[q] = -maxbox[q] - v; vmax[q] =  maxbox[q] - v; }
        else                  { vmin[q] =  maxbox[q] - v; vmax[q] = -maxbox[q] - v; }
    }
    if (glm::dot(normal, vmin) > 0.0f) return false;
    if (glm::dot(normal, vmax) >= 0.0f) return true;
    return false;
}

bool VoxelRastorizator::tri_box_overlap(const glm::vec3& boxcenter,
                                 const glm::vec3& boxhalf,
                                 const glm::vec3& v0,
                                 const glm::vec3& v1,
                                 const glm::vec3& v2)
{
    glm::vec3 tv0 = v0 - boxcenter;
    glm::vec3 tv1 = v1 - boxcenter;
    glm::vec3 tv2 = v2 - boxcenter;

    glm::vec3 e0 = tv1 - tv0;
    glm::vec3 e1 = tv2 - tv1;
    glm::vec3 e2 = tv0 - tv2;

    auto axisTest = [&](float a, float b, float fa, float fb,
                        float v0a, float v0b, float v1a, float v1b, float v2a, float v2b,
                        float boxA, float boxB) -> bool
    {
        float p0 = a * v0a - b * v0b;
        float p1 = a * v1a - b * v1b;
        float p2 = a * v2a - b * v2b;
        float minp = std::min(p0, std::min(p1, p2));
        float maxp = std::max(p0, std::max(p1, p2));
        float rad  = fa * boxA + fb * boxB;
        return !(minp > rad || maxp < -rad);
    };

    {
        float fex = std::abs(e0.x), fey = std::abs(e0.y), fez = std::abs(e0.z);
        if (!axisTest(e0.z, e0.y, fez, fey, tv0.z, tv0.y, tv1.z, tv1.y, tv2.z, tv2.y, boxhalf.z, boxhalf.y)) return false;
        if (!axisTest(e0.z, e0.x, fez, fex, tv0.x, tv0.z, tv1.x, tv1.z, tv2.x, tv2.z, boxhalf.x, boxhalf.z)) return false;
        if (!axisTest(e0.y, e0.x, fey, fex, tv0.y, tv0.x, tv1.y, tv1.x, tv2.y, tv2.x, boxhalf.y, boxhalf.x)) return false;
    }
    {
        float fex = std::abs(e1.x), fey = std::abs(e1.y), fez = std::abs(e1.z);
        if (!axisTest(e1.z, e1.y, fez, fey, tv0.z, tv0.y, tv1.z, tv1.y, tv2.z, tv2.y, boxhalf.z, boxhalf.y)) return false;
        if (!axisTest(e1.z, e1.x, fez, fex, tv0.x, tv0.z, tv1.x, tv1.z, tv2.x, tv2.z, boxhalf.x, boxhalf.z)) return false;
        if (!axisTest(e1.y, e1.x, fey, fex, tv0.y, tv0.x, tv1.y, tv1.x, tv2.y, tv2.x, boxhalf.y, boxhalf.x)) return false;
    }
    {
        float fex = std::abs(e2.x), fey = std::abs(e2.y), fez = std::abs(e2.z);
        if (!axisTest(e2.z, e2.y, fez, fey, tv0.z, tv0.y, tv1.z, tv1.y, tv2.z, tv2.y, boxhalf.z, boxhalf.y)) return false;
        if (!axisTest(e2.z, e2.x, fez, fex, tv0.x, tv0.z, tv1.x, tv1.z, tv2.x, tv2.z, boxhalf.x, boxhalf.z)) return false;
        if (!axisTest(e2.y, e2.x, fey, fex, tv0.y, tv0.x, tv1.y, tv1.x, tv2.y, tv2.x, boxhalf.y, boxhalf.x)) return false;
    }

    float minx = fmin3(tv0.x, tv1.x, tv2.x), maxx = fmax3(tv0.x, tv1.x, tv2.x);
    if (minx > boxhalf.x || maxx < -boxhalf.x) return false;

    float miny = fmin3(tv0.y, tv1.y, tv2.y), maxy = fmax3(tv0.y, tv1.y, tv2.y);
    if (miny > boxhalf.y || maxy < -boxhalf.y) return false;

    float minz = fmin3(tv0.z, tv1.z, tv2.z), maxz = fmax3(tv0.z, tv1.z, tv2.z);
    if (minz > boxhalf.z || maxz < -boxhalf.z) return false;

    glm::vec3 normal = glm::cross(e0, e1);
    if (!VoxelRastorizator::plane_box_overlap(normal, tv0, boxhalf)) return false;

    return true;
}

std::vector<glm::ivec3> VoxelRastorizator::rasterize_triangle_to_points(
    glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, float voxel_size)
{
    glm::vec3 p0 = v0 / voxel_size;
    glm::vec3 p1 = v1 / voxel_size;
    glm::vec3 p2 = v2 / voxel_size;

    glm::vec3 mn = glm::min(p0, glm::min(p1, p2));
    glm::vec3 mx = glm::max(p0, glm::max(p1, p2));

    glm::ivec3 imin = glm::ivec3(glm::floor(mn)) - glm::ivec3(1);
    glm::ivec3 imax = glm::ivec3(glm::floor(mx)) + glm::ivec3(1);

    std::vector<glm::ivec3> out;
    out.reserve((imax.x-imin.x+1)*(imax.y-imin.y+1)*(imax.z-imin.z+1));

    const glm::vec3 half(0.5f);

    for (int x = imin.x; x <= imax.x; ++x) {
        for (int y = imin.y; y <= imax.y; ++y) {
            for (int z = imin.z; z <= imax.z; ++z) {
                glm::vec3 center = glm::vec3(x + 0.5f, y + 0.5f, z + 0.5f);
                if (VoxelRastorizator::tri_box_overlap(center, half, p0, p1, p2)) {
                    out.emplace_back(x, y, z);
                }
            }
        }
    }

    return out;
}
