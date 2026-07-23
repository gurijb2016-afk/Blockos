#include "Compositor.hpp"

void Compositor::addLayer(Layer* layer)
{
layers.push_back(layer);
}

void Compositor::render(uint32_t* screen, int screenWidth, int screenHeight)
{
for (auto layer : layers)
{
for (int y = 0; y < layer->height; y++)
{
for (int x = 0; x < layer->width; x++)
{
int sx = layer->x + x;
int sy = layer->y + y;

```
            if (sx < 0 || sy < 0 || sx >= screenWidth || sy >= screenHeight)
                continue;

            screen[sy * screenWidth + sx] = layer->buffer[y * layer->width + x];
        }
    }
}
```

}
