#include "scene.h"


void Scene::update() {

}


void Scene::draw(Window* window, Camera* camera) {
    for (int i = 0; i < objects.size(); i++) {
        if (dynamic_cast<Drawable*>(objects[i])) {
            Program* program = nullptr;
            if (dynamic_cast<RenderObject*>(objects[i]))
                program = (RenderObject*)objects[i]->ma;
            
            window->draw((Drawable*)objects[i], camera);
        }
            
    }
}