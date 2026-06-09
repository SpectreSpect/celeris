#pragma once

#include <iostream>

#include "mesh_data.h"
#include "mesh.h"

class MeshLoader {
public:
    virtual MeshData load_mesh(std::string& file_path) const = 0;
};