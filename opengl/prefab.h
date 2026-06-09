#include "scene_object.h"
#include "scene.h"

class Prefab {
public:
    virtual SceneObject* instantiate(Scene* scene) = 0;
    virtual ~Prefab() = default;
};
