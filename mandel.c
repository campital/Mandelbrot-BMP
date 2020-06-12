#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdint.h>

#define MAX_MANDELBROT_ITERATIONS 80
#define IMAGE_WIDTH 1920
#define IMAGE_HEIGHT 1080
#define NUM_THREADS 54
#define MANDEL_WIDTH 1500
#define MANDEL_HEIGHT 1000

struct render_info {
    unsigned char* bufStart;
    unsigned int numRows;
    unsigned int precedingRows;
};

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

void* renderSection(void* args)
{
    struct render_info* renderInfo = (struct render_info*) args;
    unsigned char* data = renderInfo->bufStart + renderInfo->precedingRows * ((4 + ((IMAGE_WIDTH * -3) % 4)) % 4) + renderInfo->precedingRows * IMAGE_WIDTH * 3;
    float factor;
    for (int i = 0; i < renderInfo->numRows * IMAGE_WIDTH; i++) {
        unsigned int x = i % IMAGE_WIDTH;
        unsigned int y = (i + renderInfo->precedingRows * IMAGE_WIDTH) / IMAGE_WIDTH;
        unsigned char gradientR = (unsigned char)((float)x / ((float)(IMAGE_WIDTH - 1) / 127.5) + (float)y / ((float)(IMAGE_HEIGHT - 1) / 127.5));
        unsigned char gradientG = (unsigned char)((float)(IMAGE_WIDTH - x - 1) / ((float)(IMAGE_WIDTH - 1) / 127.5) + (float)(IMAGE_HEIGHT - y - 1) / ((float)(IMAGE_HEIGHT - 1) / 127.5));
        unsigned char gradientB = (unsigned char)((float)x / ((float)(IMAGE_WIDTH - 1) / 127.5) + (float)(IMAGE_HEIGHT - y - 1) / ((float)(IMAGE_HEIGHT - 1) / 127.5));
        int mandel = inMandelbrotSet(x - (IMAGE_WIDTH - MANDEL_WIDTH) / 2, y - (IMAGE_HEIGHT - MANDEL_HEIGHT) / 2, MANDEL_WIDTH, MANDEL_HEIGHT);
        
        unsigned int baseIndex = i * 3 + (y - renderInfo->precedingRows) * ((4 + ((IMAGE_WIDTH * -3) % 4)) % 4);
        if (mandel > 4) {
            factor = (float)(MAX_MANDELBROT_ITERATIONS - (mandel - 5)) / MAX_MANDELBROT_ITERATIONS;
            data[baseIndex] = factor * gradientB;
            data[baseIndex + 1] = factor * gradientG;
            data[baseIndex + 2] = factor * gradientR;
        } else {
            data[baseIndex] = gradientB;
            data[baseIndex + 1] = gradientG;
            data[baseIndex + 2] = gradientR;
        }
    }
    free(args);
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

    fwrite(&bmpHeader, 1, sizeof(struct BITMAPFILEHEADER), bmpOut);
    fwrite(&bmpInfo, 1, sizeof(struct BITMAPINFOHEADER), bmpOut);

    unsigned char* img = calloc(dataSize, 1);
    unsigned int precedingRows = 0;

    pthread_attr_t attr;
    pthread_t threads[NUM_THREADS];
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    for(int i = 0; i < NUM_THREADS; i++) {
        struct render_info* renderInfo = malloc(sizeof(struct render_info));
        renderInfo->bufStart = img;
        renderInfo->numRows = IMAGE_HEIGHT / NUM_THREADS;
        renderInfo->precedingRows = precedingRows;
        
        if(i + 1 == NUM_THREADS)
            renderInfo->numRows = IMAGE_HEIGHT - (renderInfo->numRows * (NUM_THREADS - 1));
        if(pthread_create(&threads[i], &attr, renderSection, renderInfo))
            return -1;
        precedingRows += renderInfo->numRows;
    }
    pthread_attr_destroy(&attr);

    void* ret;
    for(int i = 0; i < NUM_THREADS; i++) {
        if(pthread_join(threads[i], &ret))
            return -1;
    }
    fwrite(img, 1, dataSize, bmpOut);

    free(img);
    fclose(bmpOut);
}
