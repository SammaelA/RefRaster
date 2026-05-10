#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "image_utils.h"
#include "external/stb_image.h"
#include "external/stb_image_write.h"

static inline unsigned char tonemap(float x, float a_gammaInv)
{
    const int colorLDR = (int)(powf(x, a_gammaInv) * 255.0f + 0.5f);
    if (colorLDR < 0)
        return 0;
    else if (colorLDR > 255)
        return 255;
    else
        return colorLDR;
}

int save_image_f32_png_rgb(const float *data, const char *filename, int w, int h, int channels, float gamma)
{
    const float gammaInv = 1.0f/gamma;
    unsigned char *data_rgba8 = malloc(4 * w * h);

    if (channels == 0)
        return -1;
    else if (channels == 1)
    {
        for (int i=0; i<w*h; i++)
        {
            data_rgba8[4*i+0] = tonemap(data[channels*i+0], gammaInv);
            data_rgba8[4*i+1] = 0;
            data_rgba8[4*i+2] = 0;
            data_rgba8[4*i+3] = 255;
        }
    }
    else if (channels == 2)
    {
        for (int i=0; i<w*h; i++)
        {
            data_rgba8[4*i+0] = tonemap(data[channels*i+0], gammaInv);
            data_rgba8[4*i+1] = tonemap(data[channels*i+1], gammaInv);
            data_rgba8[4*i+2] = 0;
            data_rgba8[4*i+3] = 255;
        }
    }
    else if (channels >= 3)
    {
        for (int i=0; i<w*h; i++)
        {
            data_rgba8[4*i+0] = tonemap(data[channels*i+0], gammaInv);
            data_rgba8[4*i+1] = tonemap(data[channels*i+1], gammaInv);
            data_rgba8[4*i+2] = tonemap(data[channels*i+2], gammaInv);
            data_rgba8[4*i+3] = 255;
        }
    }

    //swap y
    for (int i=0; i< h/2; i++)
    {
        for (int j=0; j< w; j++)
        {
            int i1 = i*w+j;
            int i2 = (h-1-i)*w+j;
            for (int ch=0; ch<4; ch++)
            {
                unsigned char tmp = data_rgba8[4*i1+ch];
                data_rgba8[4*i1+ch] = data_rgba8[4*i2+ch];
                data_rgba8[4*i2+ch] = tmp;
            }
        }
    }

    int res = stbi_write_png(filename, w, h, 4, data_rgba8, 4*w);

    free(data_rgba8);
    return res;
}