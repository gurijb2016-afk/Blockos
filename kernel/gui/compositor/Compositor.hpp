#pragma once
#include <vector>
#include "Layer.hpp"

class Compositor
{
private:
std::vector<Layer*> layers;

public:
void addLayer(Layer* layer);
void render(uint32_t* screen, int screenWidth, int screenHeight);
};
