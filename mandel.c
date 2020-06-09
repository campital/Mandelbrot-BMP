#include <stdio.h>
#include <stdint.h>

#define MAX_MANDELBROT_ITERATIONS 80
#define IMAGE_WIDTH 1920
#define IMAGE_HEIGHT 1080
#define MANDEL_WIDTH 1500
#define MANDEL_HEIGHT 1000

struct __attribute__((__packed__)) BITMAPFILEHEADER {
  uint16_t bfType;
  uint32_t bfSize;
  uint16_t bfReserved1;
  uint16_t bfReserved2;
  uint32_t bfOffBits;
};

struct __attribute__((__packed__)) BITMAPINFOHEADER {
  uint32_t biSize;
  uint32_t biWidth;
  uint32_t biHeight;
  uint16_t biPlanes;
  uint16_t biBitCount;
  uint32_t biCompression;
  uint32_t biSizeImage;
  uint32_t biXPelsPerMeter;
  uint32_t biYPelsPerMeter;
  uint32_t biClrUsed;
  uint32_t biClrImportant;
};

int inMandelbrotSet(int x, int y, int width, int height)
{
    const double normalizedX = ((double)x / (double)width) * 3 - 2;
    const double normalizedY = ((double)y / (double)height) * 2 - 1;
    double real = normalizedX;
    double imaginary = normalizedY;

    for (int i = 0; i < MAX_MANDELBROT_ITERATIONS; i++) {
        double real2 = real * real;
        double imaginary2 = imaginary * imaginary;
        imaginary = real * imaginary * 2 + normalizedY;
        real = real2 - imaginary2 + normalizedX;
        if (real2 + imaginary2 > 4)
            return i;
    }
    return MAX_MANDELBROT_ITERATIONS;
}

int main()
{
    unsigned int dataSize = IMAGE_WIDTH * 3 * IMAGE_HEIGHT + ((4 + ((IMAGE_WIDTH * -3) % 4)) % 4) * IMAGE_HEIGHT;
    struct BITMAPFILEHEADER bmpHeader = {0x4D42, 54 + dataSize, 0, 0, 54}; /* data obtained from MSDN and Wikipedia */
    struct BITMAPINFOHEADER bmpInfo = { sizeof(struct BITMAPINFOHEADER), IMAGE_WIDTH, IMAGE_HEIGHT, 1, 24, 0, dataSize, 2835, 2835, 0, 0 };

    FILE* bmpOut = fopen("mandelbrot.bmp", "wb");
    if(bmpOut == NULL) {
        perror("Error opening file");
        return -1;
    }

    /* write first header to file */
    fwrite(&bmpHeader, 1, sizeof(struct BITMAPFILEHEADER), bmpOut);
    /* write DIB header */
    fwrite(&bmpInfo, 1, sizeof(struct BITMAPINFOHEADER), bmpOut);

    unsigned char currentPix[3];
    unsigned char nullByte = 0;
    for (int y = 0; y < IMAGE_HEIGHT; y++) {
        for (int x = 0; x < IMAGE_WIDTH; x++)
        {
            unsigned char gradientR = (unsigned char)((float)x / ((float)(IMAGE_WIDTH - 1) / 127.5) + (float)y / ((float)(IMAGE_HEIGHT - 1) / 127.5));
            unsigned char gradientG = (unsigned char)((float)(IMAGE_WIDTH - x - 1) / ((float)(IMAGE_WIDTH - 1) / 127.5) + (float)(IMAGE_HEIGHT - y - 1) / ((float)(IMAGE_HEIGHT - 1) / 127.5));
            unsigned char gradientB = (unsigned char)((float)x / ((float)(IMAGE_WIDTH - 1) / 127.5) + (float)(IMAGE_HEIGHT - y - 1) / ((float)(IMAGE_HEIGHT - 1) / 127.5));
            int mandel = inMandelbrotSet(x - (IMAGE_WIDTH - MANDEL_WIDTH) / 2, y - (IMAGE_HEIGHT - MANDEL_HEIGHT) / 2, MANDEL_WIDTH, MANDEL_HEIGHT);
            if (mandel > 35) {
                currentPix[0] = 0;
                currentPix[1] = 0;
                currentPix[2] = 0;
            } else {
                currentPix[0] = gradientB;
                currentPix[1] = gradientG;
                currentPix[2] = gradientR;
            }
            fwrite(currentPix, 1, 3, bmpOut);
        }
        for (int i = 0; i < (4 + ((IMAGE_WIDTH * -3) % 4)) % 4; i++) {
            fwrite(&nullByte, 1, 1, bmpOut);
        }
    }

    fclose(bmpOut);
}
