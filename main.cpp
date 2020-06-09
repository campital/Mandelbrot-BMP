#include <iostream>
#include <fstream>
#include <algorithm>

#define MAX_MANDELBROT_ITERATIONS 80
#define IMAGE_WIDTH 1920
#define IMAGE_HEIGHT 1080
#define MANDEL_WIDTH 1500
#define MANDEL_HEIGHT 1000

typedef uint32_t LONG;
typedef uint32_t DWORD;
typedef uint16_t WORD;

struct __attribute__((__packed__)) BITMAPFILEHEADER {
  WORD  bfType;
  DWORD bfSize;
  WORD  bfReserved1;
  WORD  bfReserved2;
  DWORD bfOffBits;
};

struct __attribute__((__packed__)) BITMAPINFOHEADER {
  DWORD biSize;
  LONG  biWidth;
  LONG  biHeight;
  WORD  biPlanes;
  WORD  biBitCount;
  DWORD biCompression;
  DWORD biSizeImage;
  LONG  biXPelsPerMeter;
  LONG  biYPelsPerMeter;
  DWORD biClrUsed;
  DWORD biClrImportant;
};

int inMandelbrotSet(int x, int y, int width, int height)
{
	const double normalizedX = ((double)x / (double)width) * 3 - 2;
	const double normalizedY = ((double)y / (double)height) * 2 - 1;
	double real = normalizedX;
	double imaginary = normalizedY;

	for (int i = 0; i < MAX_MANDELBROT_ITERATIONS; i++)
	{
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
	BITMAPFILEHEADER bmpHeader = {0x4D42, 54 + dataSize, 0, 0, 54}; // data obtained from MSDN and Wikipedia
	BITMAPINFOHEADER bmpInfo = { sizeof(BITMAPINFOHEADER), IMAGE_WIDTH, IMAGE_HEIGHT, 1, 24, 0, dataSize, 2835, 2835, 0, 0 };

	std::ofstream bmpOut;
	// I SPENT 10 FRIAKCING HOURS TRYING TO FIGURE OUT WHY IT WAS RANDOMLY PRINTING 0x0D. Change to ios::binary
	bmpOut.open("mandelbrot.bmp", std::ios::binary);

	// write first header to file
	for (unsigned char* pnt = (unsigned char*)&bmpHeader; pnt < (unsigned char*)&bmpHeader + sizeof(BITMAPFILEHEADER); pnt++)	{
		bmpOut << *pnt;
	}

	// write DIB header
	for (unsigned char* pnt = (unsigned char*)&bmpInfo; pnt < (unsigned char*)&bmpInfo + sizeof(BITMAPINFOHEADER); pnt++) {
		bmpOut << *pnt;
	}

	unsigned char currentPix[3] = {};
	for (int y = 0; y < IMAGE_HEIGHT; y++) {
		for (int x = 0; x < IMAGE_WIDTH; x++)
		{
			unsigned char gradientR = (unsigned char)((float)x / ((float)(IMAGE_WIDTH - 1) / 127.5) + (float)y / ((float)(IMAGE_HEIGHT - 1) / 127.5));
			unsigned char gradientG = (unsigned char)((float)(IMAGE_WIDTH - x - 1) / ((float)(IMAGE_WIDTH - 1) / 127.5) + (float)(IMAGE_HEIGHT - y - 1) / ((float)(IMAGE_HEIGHT - 1) / 127.5));
			unsigned char gradientB = (unsigned char)((float)x / ((float)(IMAGE_WIDTH - 1) / 127.5) + (float)(IMAGE_HEIGHT - y - 1) / ((float)(IMAGE_HEIGHT - 1) / 127.5));
			int mandel = inMandelbrotSet(x - (IMAGE_WIDTH - MANDEL_WIDTH) / 2, y - (IMAGE_HEIGHT - MANDEL_HEIGHT) / 2, MANDEL_WIDTH, MANDEL_HEIGHT);
			if ((IMAGE_WIDTH - MANDEL_WIDTH) / 2 < x < IMAGE_WIDTH - ((IMAGE_WIDTH - MANDEL_WIDTH) / 2) && (IMAGE_HEIGHT - MANDEL_HEIGHT) / 2 < y < IMAGE_HEIGHT - ((IMAGE_HEIGHT - MANDEL_HEIGHT) / 2) && mandel > 35) {
				int factor = 127 - ((float)mandel / MAX_MANDELBROT_ITERATIONS) * 127;
				// set R, G, B
				currentPix[0] = gradientR * ((float)factor / 127);
				currentPix[1] = gradientG * ((float)factor / 127);
				currentPix[2] = gradientB * ((float)factor / 127);
			}
			else {
				currentPix[0] = gradientR;
				currentPix[1] = gradientG;
				currentPix[2] = gradientB;
			}
			bmpOut << currentPix[2] << currentPix[1] << currentPix[0];
		}
		for (int i = 0; i < (4 + ((IMAGE_WIDTH * -3) % 4)) % 4; i++) {
			bmpOut << (unsigned char)0;
		}
	}

	bmpOut.close();
}
