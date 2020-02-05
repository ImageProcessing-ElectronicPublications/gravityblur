# Gravity Blur

This is an image smoothing algorithm inspired by [JPEG Quant Smooth](https://github.com/ilyakurdyukov/jpeg-quantsmooth). This is called *gravity blur* because the pixel values gravitate towards each other, the force of this *gravity* is inversely proportional to the distance between them.

This algorithm preserves sharp edges if the distance between pixel values on the sides of the edges is greater than *range*. With more iterations, it is not only smoothes the image, but can also make it look like CG art or poster.

The original project page is [here](https://github.com/ilyakurdyukov/gravityblur).

## Usage

`gravblur [options] input.[png|jpg|jpeg] output.[png|jpg|jpeg]`

## Options

`--range f` Gravity range (0-100)  
`--sharp f` Sharpness (0-100)  
`--niter n` Number of iterations (default is 3)  
`--rgb` Process in RGB (default)  
`--yuv` Process in YUV  
`--separate ` Separate color components  
`--info n` Print gravblur debug output: 0 - silent, 3 - all (default)  

## Examples

- Images 3x zoomed.

![](https://ilyakurdyukov.github.io/gravityblur/images/lena_orig.png)
![](https://ilyakurdyukov.github.io/gravityblur/images/lena_new.png)

## Buliding

If your system have *libjpeg* and *libpng* development packages installed, just type `make`.
Or you can specify their location with `LIBS` variable to `make`.

