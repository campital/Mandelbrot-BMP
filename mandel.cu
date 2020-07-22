#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <getopt.h>
#include <string.h>
#include <math.h>

#define MAX_MANDELBROT_ITERATIONS (80)
#define DEFAULT_OUTPUT "mandelbrot.bmp"
#define THREADS_PER_BLOCK (1024/2)

unsigned int imageWidth = 1920;
unsigned int imageHeight = 1080;
unsigned int rowPadding;

const struct option options[] = {
        {"help", 0, NULL, 1},
        {"xleft", 1, NULL, 2},
        {"xright", 1, NULL, 3},
        {"ylower", 1, NULL, 4},
        {"yupper", 1, NULL, 5},
        {"width", 1, NULL, 6},
        {"height", 1, NULL, 7},
        {"output", 1, NULL, 8},
        {0, 0, 0, 0}
};
    
const char helpMessage[] = "Usage: %s [args]\n\
Mandelbrot-BMP generates a BMP image of a specified location in the Mandelbrot set.\n\n\
Possible arguments:\n\
    --help\n\
    --xleft [value]    Set the leftmost x value to render    (double)\n\
    --xright [value]   Set the rightmost x value to render   (double)\n\
    --ylower [value]   Set the lowest y value to render      (double)\n\
    --yupper [value]   Set the highest y value to render     (double)\n\
    --width [value]    Set the width of the output image     (int)\n\
    --height [value]   Set the height of the output image    (int)\n\
    --output [value]   Set the location of the output image  (string)\n\n\
If only 3 boundary values are specified, the other can be inferred from the image aspect ratio.\n";

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

__device__ int inMandelbrotSet(double x, double y)
{
    double real = x;
    double imaginary = y;

    for (int i = 0; i < MAX_MANDELBROT_ITERATIONS; i++) {
        double real2 = real * real;
        double imaginary2 = imaginary * imaginary;
        imaginary = real * imaginary * 2 + y;
        real = real2 - imaginary2 + x;
        if (real2 + imaginary2 > 4)
            return i;
    }
    return MAX_MANDELBROT_ITERATIONS;
}

__global__ void renderPixel(unsigned char* bufStart, int width, int height, double xStart, double xRange, double yStart, double yRange, int padding, int iters)
{
    for(int i = (blockIdx.x * blockDim.x + threadIdx.x) * iters; i < (blockIdx.x * blockDim.x + threadIdx.x) * iters + iters; i++) {
        if(i >= (width * height))
            return;
        unsigned int x = i % width;
        unsigned int y = i / width;
        
        unsigned char gradientR = (unsigned char)((float)x / ((float)(width - 1) / 127.5) + (float)y / ((float)(height - 1) / 127.5));
        unsigned char gradientG = (unsigned char)((float)(width - x - 1) / ((float)(width - 1) / 127.5) + (float)(height - y - 1) / ((float)(height - 1) / 127.5));
        unsigned char gradientB = (unsigned char)((float)x / ((float)(width - 1) / 127.5) + (float)(height - y - 1) / ((float)(height - 1) / 127.5));

        unsigned int mandel = inMandelbrotSet(((double)x / (double)width) * xRange + xStart,
           ((double)y / (double)height) * yRange + yStart);

        unsigned int baseIndex = y * padding + y * width * 3 + x * 3;
        if (mandel > 4) {
            float factor = (float)(MAX_MANDELBROT_ITERATIONS - (mandel - 5)) / MAX_MANDELBROT_ITERATIONS;
            bufStart[baseIndex] = factor * gradientB;
            bufStart[baseIndex + 1] = factor * gradientG;
            bufStart[baseIndex + 2] = factor * gradientR;
        } else {
            bufStart[baseIndex] = gradientB;
            bufStart[baseIndex + 1] = gradientG;
            bufStart[baseIndex + 2] = gradientR;
        }
    }
}

void setDefaultCorners(double corners[])
{
    printf("Warning: Falling back to default Mandelbrot corners.\n");
    corners[0] = -2.4;
    corners[1] = 1.4;
    corners[2] = -0.5 * (corners[1] - corners[0]) * ((double)imageHeight / (double)imageWidth);
    corners[3] = -1 * corners[2];
}

/*
* corners should be left x, right x, lower y, upper y
* returns 1 if the program should proceed
* REMEMBER TO FREE *fileName!
*/
int getArgs(int argc, char** argv, double corners[4], char** fileName)
{
    for(int i = 0; i < 4; i++)
        corners[i] = NAN;
    *fileName = NULL;
    
    int res;
    int tmpImageHeight;
    int tmpImageWidth;
    while((res = getopt_long_only(argc, argv, "", options, NULL)) != -1) {
        switch(res) {
            case '?':
                printf("Run '%s --help' for more information\n", argv[0]);
                break;
            case 1:
                printf(helpMessage, argv[0]);
                return 0;
                break;
            case 2:
                corners[0] = strtod(optarg, NULL);
                break;
            case 3:
                corners[1] = strtod(optarg, NULL);
                break;
            case 4:
                corners[2] = strtod(optarg, NULL);
                break;
            case 5:
                corners[3] = strtod(optarg, NULL);
                break;
            case 6:
                tmpImageWidth = atoi(optarg);
                if(tmpImageWidth > 0 && tmpImageWidth < 20000)
                    imageWidth = tmpImageWidth;
                else
                    printf("Warning: Image width is not between 0 and 20000 pixels! Falling back to default.\n");
                break;
            case 7:
                tmpImageHeight = atoi(optarg);
                if(tmpImageHeight > 0 && tmpImageHeight < 20000)
                    imageHeight = tmpImageHeight;
                else
                    printf("Warning: Image height is not between 0 and 20000 pixels! Falling back to default.\n");
                break;
            case 8:
                if(*fileName != NULL)
                    free(*fileName);
                *fileName = strdup(optarg);
                break;
        }
    }
    
    if(!isnan(corners[0]) && !isnan(corners[1]) && (isnan(corners[2]) != isnan(corners[3]))) {
         double otherRange = (corners[1] - corners[0]) * ((double)imageHeight / (double)imageWidth);
         if(!isnan(corners[2]))
            corners[3] = corners[2] + otherRange;
         else if(!isnan(corners[3]))
            corners[2] = corners[3] - otherRange;
    } else if(!isnan(corners[2]) && !isnan(corners[3]) && (isnan(corners[0]) != isnan(corners[1]))) {
        double otherRange = (corners[3] - corners[2]) * ((double)imageWidth / (double)imageHeight);
         if(!isnan(corners[0]))
            corners[1] = corners[0] + otherRange;
         else if(!isnan(corners[1]))
            corners[0] = corners[1] - otherRange;
    } else if(!(!isnan(corners[0]) && !isnan(corners[1]) && !isnan(corners[2]) && !isnan(corners[3]))) {
        setDefaultCorners(corners);
    }
    
    if(*fileName == NULL)
        *fileName = strdup(DEFAULT_OUTPUT);
    
    return 1;
}

void gpuAssert(cudaError_t retVal)
{
    if(retVal != cudaSuccess) {
        printf("CUDA assertion failed: %s\n", cudaGetErrorString(retVal));
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char** argv)
{
    char* fileName;
    double corners[4];
    if(!getArgs(argc, argv, corners, &fileName))
        return 0;
    rowPadding = (4 - (imageWidth % 4)) % 4;
    
    unsigned int dataSize = imageWidth * 3 * imageHeight + rowPadding * imageHeight;
    struct BITMAPFILEHEADER bmpHeader = {0x4D42, 54 + dataSize, 0, 0, 54}; /* data obtained from MSDN and Wikipedia */
    struct BITMAPINFOHEADER bmpInfo = { sizeof(struct BITMAPINFOHEADER), imageWidth, imageHeight, 1, 24, 0, dataSize, 2835, 2835, 0, 0 };

    FILE* bmpOut = fopen(fileName, "wb");
    if(bmpOut == NULL) {
        perror("Error opening file");
        return -1;
    }

    fwrite(&bmpHeader, 1, sizeof(struct BITMAPFILEHEADER), bmpOut);
    fwrite(&bmpInfo, 1, sizeof(struct BITMAPINFOHEADER), bmpOut);

    unsigned char* gpuImg;
    gpuAssert(cudaMalloc(&gpuImg, dataSize));
    renderPixel<<<(imageWidth * imageHeight) / THREADS_PER_BLOCK + 1, THREADS_PER_BLOCK>>>(gpuImg, imageWidth, imageHeight, corners[0], corners[1] - corners[0], corners[2], corners[3] - corners[2], rowPadding, 2);

    unsigned char* img = (unsigned char*) malloc(dataSize);
    gpuAssert(cudaMemcpy(img, gpuImg, dataSize, cudaMemcpyDeviceToHost));
    fwrite(img, 1, dataSize, bmpOut);
    gpuAssert(cudaFree(gpuImg));
    free(img);
    free(fileName);
    fclose(bmpOut);
    return EXIT_SUCCESS;
}
