
#include "scene_object.h"
#include "mesh.h"
#include "material_instance.h"
#include "drawable.h"

class RenderObject : public SceneObject, public Drawable{
public:
    Mesh* mesh = nullptr;
    MaterialInstance* material;

    void draw(RenderState state) override;
};
