# Mandelbrot-BMP
### Generate images of areas from the Mandelbrot set
![Example Wallpaper](https://github.com/campital/Mandelbrot-BMP/raw/master/mandelbrot.bmp)

### Usage
1. Download or clone the repository
2. Compile: `gcc -O3 mandel.c -o mandel -pthread` for the pthread version or `nvcc -O3 mandel.cu -o mandel` for the CUDA version
3. Run `./mandel` to generate the default image or run `./mandel --help` for more options
