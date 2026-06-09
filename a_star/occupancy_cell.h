#pragma once

class OccupancyCell {
public:
    float curvature = 0;
    bool solid = false;
    
    OccupancyCell(float curvature = 0, bool solid = false);
};