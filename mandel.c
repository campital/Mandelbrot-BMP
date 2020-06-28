#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdint.h>
#include <getopt.h>
#include <math.h>

#define MAX_MANDELBROT_ITERATIONS 80
#define NUM_THREADS 54

int imageWidth = 1920;
int imageHeight = 1080;
int rowPadding;

const struct option options[] = {
        {"help", 0, NULL, 1},
        {"xleft", 1, NULL, 2},
        {"xright", 1, NULL, 3},
        {"ylower", 1, NULL, 4},
        {"yupper", 1, NULL, 5},
        {"width", 1, NULL, 6},
        {"height", 1, NULL, 7},
        {}
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
    --height [value]   Set the height of the output image    (int)\n\n\
If only 3 boundary values are specified, the other can be inferred from the image aspect ratio.\n";

struct render_info {
    unsigned char* bufStart;
    unsigned int numRows;
    unsigned int precedingRows;
    double xStart;
    double xEnd;
    double yStart;
    double yEnd;
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

int inMandelbrotSet(double x, double y)
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

void* renderSection(void* args)
{
    struct render_info* renderInfo = (struct render_info*) args;
    unsigned char* data = renderInfo->bufStart + renderInfo->precedingRows * rowPadding + renderInfo->precedingRows * imageWidth * 3;
    float factor;
    const double xRange = renderInfo->xEnd - renderInfo->xStart;
    const double yRange = renderInfo->yEnd - renderInfo->yStart;
    for (int i = 0; i < renderInfo->numRows * imageWidth; i++) {
        unsigned int x = i % imageWidth;
        unsigned int y = (i + renderInfo->precedingRows * imageWidth) / imageWidth;
        unsigned char gradientR = (unsigned char)((float)x / ((float)(imageWidth - 1) / 127.5) + (float)y / ((float)(imageHeight - 1) / 127.5));
        unsigned char gradientG = (unsigned char)((float)(imageWidth - x - 1) / ((float)(imageWidth - 1) / 127.5) + (float)(imageHeight - y - 1) / ((float)(imageHeight - 1) / 127.5));
        unsigned char gradientB = (unsigned char)((float)x / ((float)(imageWidth - 1) / 127.5) + (float)(imageHeight - y - 1) / ((float)(imageHeight - 1) / 127.5));
        
        int mandel = inMandelbrotSet(((double)x / (double)imageWidth) * xRange + renderInfo->xStart,
            ((double)y / (double)imageHeight) * yRange + renderInfo->yStart);
        
        unsigned int baseIndex = i * 3 + (y - renderInfo->precedingRows) * rowPadding;
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
    return NULL;
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
*/
int getArgs(int argc, char** argv, double corners[4])
{
    for(int i = 0; i < 4; i++)
        corners[i] = NAN;
    
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
        }
    }
    quitLoop:;
    
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
    
    return 1;
}

int main(int argc, char** argv)
{
    double corners[4];
    if(!getArgs(argc, argv, corners))
        return 0;
    rowPadding = (4 - (imageWidth % 4)) % 4;
    
    unsigned int dataSize = imageWidth * 3 * imageHeight + rowPadding * imageHeight;
    struct BITMAPFILEHEADER bmpHeader = {0x4D42, 54 + dataSize, 0, 0, 54}; /* data obtained from MSDN and Wikipedia */
    struct BITMAPINFOHEADER bmpInfo = { sizeof(struct BITMAPINFOHEADER), imageWidth, imageHeight, 1, 24, 0, dataSize, 2835, 2835, 0, 0 };

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
        renderInfo->numRows = imageHeight / NUM_THREADS;
        renderInfo->precedingRows = precedingRows;
        renderInfo->xStart = corners[0];
        renderInfo->xEnd = corners[1];
        renderInfo->yStart = corners[2];
        renderInfo->yEnd = corners[3];
        
        if(i + 1 == NUM_THREADS)
            renderInfo->numRows = imageHeight - (renderInfo->numRows * (NUM_THREADS - 1));
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
