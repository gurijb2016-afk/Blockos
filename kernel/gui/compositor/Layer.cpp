#include "Layer.hpp"

Layer::Layer(uint32_t* buf, int x, int y, int w, int h)
{
buffer = buf;
this->x = x;
this->y = y;
width = w;
height = h;
opacity = 255;
}
