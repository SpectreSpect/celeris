#pragma once
#include "render_state.h"

class Drawable {
public:
    virtual void draw(RenderState state) = 0;
};