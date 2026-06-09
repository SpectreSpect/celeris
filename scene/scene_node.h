#pragma once
#include "transform.h"
#include <vector>
#include <memory>

class SceneNode {
public:
    Transform transform;
    SceneNode* parent = nullptr;
    std::vector<std::unique_ptr<SceneNode>> children;
};