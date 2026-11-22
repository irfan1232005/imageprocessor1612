# Image Processing Laboratory (C)

A terminal-driven educational image-processing program. The spatial and frequency-domain algorithms are implemented in C with loops, arrays, `double` arithmetic, convolution, interpolation, histograms, and a radix-2 FFT. It does **not** use OpenCV or a ready-made image-processing library.

## Build and run on Ubuntu

```bash
gcc -std=c11 -O2 -Wall -Wextra -pedantic image_lab.c -lm -o image_lab
./image_lab sample_image.ppm
```

You can also pass a path containing spaces:

```bash
./image_lab "images/my photograph.ppm"
```

## Image format

The program reads and writes PPM images (P3 text or P6 binary). PPM keeps the image-I/O code small enough to study. Convert a JPEG or PNG to PPM before loading it:

```bash
sudo apt install imagemagick
magick photograph.jpg -compress none photograph.ppm
```

Open a generated image with:

```bash
xdg-open output_sobel_gx.ppm
```

## Operations in the terminal menu

| Menu | Operation | Main mathematical idea |
|---:|---|---|
| 1 | Grayscale | `Y = 0.299R + 0.587G + 0.114B` |
| 2 | Gaussian filter | normalized Gaussian kernel and separable convolution |
| 3 | Mean filter | average of the local neighborhood |
| 4 | Median filter | middle value after sorting the neighborhood |
| 5 | Bilateral filter | spatial Gaussian weight multiplied by intensity-similarity weight |
| 6–7 | First derivative X/Y | central finite differences |
| 8–9 | Second derivative X/Y | second finite differences |
| 10–11 | Laplacian 4/8 neighbor | discrete second-derivative sum |
| 12 | Laplacian of Gaussian | Gaussian smoothing followed by a Laplacian |
| 13–14 | Sobel Gx/Gy | 3x3 derivative masks |
| 15 | Gradient magnitude | `sqrt(Gx*Gx + Gy*Gy)` |
| 16 | Gradient direction theta | `atan2(Gy, Gx)` |
| 17 | Gradient vector CSV | saves x, y, Gx, Gy, magnitude and angle in radians |
| 18 | Gradient arrow overlay | draws sampled gradient vectors on an image |
| 19–20 | Prewitt X/Y | Prewitt derivative masks |
| 21–22 | Roberts X/Y | 2x2 diagonal finite differences |
| 23 | Canny | Gaussian, Sobel, non-maximum suppression, double threshold, hysteresis |
| 24–25 | Binary/inverse threshold | pixel comparison with a supplied threshold |
| 26 | Adaptive mean threshold | local mean minus a user-supplied `C` value |
| 27 | Otsu threshold | maximizes between-class variance from the histogram |
| 28 | Multilevel Otsu | dynamic programming to minimize within-class squared error |
| 29 | Unsharp-mask sharpening | `I + amount * (I - Gaussian(I))` |
| 30 | Contrast stretching | maps observed minimum and maximum to 0 and 255 |
| 31 | Histogram equalization | cumulative histogram / CDF remapping |
| 32 | Gamma correction | `Iout = 255 * (Iin/255)^gamma` |
| 33–38 | Erosion, dilation, opening, closing, morphological gradient, boundary | binary set operations with a square structuring element |
| 39 | Translation | inverse coordinate mapping `(x-dx, y-dy)` |
| 40 | Rotation | inverse rotation around image center; expanded canvas |
| 41 | Scaling | inverse mapping with independent x/y scale factors |
| 42 | Zoom | center-based inverse mapping on a fixed canvas |
| 43 | Shear | inverse affine coordinate mapping |
| 44–45 | Bulge / squeeze | radial inverse-coordinate warp |
| 46 | Perspective warp | 3x3 homography estimated from four point correspondences |
| 47–48 | Horizontal / vertical reflection | coordinate reversal |
| 49 | Padding | constant, replicate, or reflect border pixels |
| 50 | FFT magnitude spectrum | 2D radix-2 FFT, `log(1 + magnitude)`, centered DC component |
| 51 | FFT phase spectrum | `atan2(imaginary, real)` mapped to display range |
| 52–55 | Ideal/Gaussian low-pass/high-pass | multiply the complex spectrum by a radial frequency mask, then inverse FFT |

## Key mathematical details

### Convolution

For a kernel `K` of size `(2r+1) x (2r+1)`, the implementation computes

\[
O(x,y)=\sum_{j=-r}^{r}\sum_{i=-r}^{r}K(i,j)I(x+i,y+j).
\]

The boundary policy for convolution is pixel replication. The code stores intermediate image values as `double`, so negative derivative responses are not lost during calculation.

### Gaussian filter

The one-dimensional Gaussian used by the separable filter is

\[
G(i)=\exp\left(-\frac{i^2}{2\sigma^2}\right),
\qquad
\widehat G(i)=\frac{G(i)}{\sum_jG(j)}.
\]

It first convolves horizontally and then vertically. This is mathematically equivalent to a 2D separable Gaussian kernel while requiring fewer multiplications.

### Derivatives and gradients

\[
I_x(x,y)\approx\frac{I(x+1,y)-I(x-1,y)}{2},\qquad
I_y(x,y)\approx\frac{I(x,y+1)-I(x,y-1)}{2}.
\]

The menu's Sobel components use the 3x3 Sobel masks. The gradient magnitude and direction are

\[
M=\sqrt{G_x^2+G_y^2},\qquad \theta=\operatorname{atan2}(G_y,G_x).
\]

The image coordinate system has positive y downward. The CSV records the actual signed gradient components and angle in radians. The arrow overlay normalizes arrow length for readability; it does not represent the magnitude by arrow length.

### Laplacian and LoG

\[
\nabla^2 I=I_{xx}+I_{yy}.
\]

The LoG implementation smooths the input with a Gaussian first, then applies the four-neighbor Laplacian. This reduces sensitivity to high-frequency noise before the second derivative.

### Thresholding

Binary thresholding outputs 255 when `pixel >= T` and 0 otherwise. Inverse thresholding reverses those values. Adaptive mean thresholding compares each pixel with the average in its local block minus `C`. Otsu chooses one global threshold by maximizing the between-class variance. Multilevel Otsu uses dynamic programming for 2–8 intensity classes.

### Geometric transforms

Geometric transformations use **inverse mapping**: for every output pixel, calculate the corresponding source coordinate. Bilinear interpolation estimates a value between source pixels. This avoids the holes that can occur when mapping source pixels forward into the output.

- Rotation uses an expanded canvas to reduce cropping.
- Scaling changes output dimensions.
- Zoom keeps the output dimensions fixed and changes the sampled field of view.
- Perspective warp retains the original canvas dimensions; coordinates outside it are cropped.
- Positive rotation angles are defined to appear counter-clockwise on the displayed image.

### Fourier transform

The 2D transform is computed by applying a radix-2 Cooley–Tukey FFT to each row and then each column. Input is zero-padded to power-of-two dimensions. The supported maximum dimension is 2048 pixels in either direction.

\[
F(u,v)=\sum_y\sum_x I(x,y)e^{-j2\pi(ux/W+vy/H)}.
\]

Magnitude uses `log(1 + |F|)` for display. Phase uses `atan2(Im(F), Re(F))`. Frequency filtering multiplies the complex spectrum by an ideal or Gaussian radial mask and then applies the inverse FFT.

## Output and workflow notes

- Every menu operation starts from the originally loaded grayscale image; it does not silently chain from the previous result.
- To chain operations, run the program again using a previous output PPM as the input image.
- Most outputs are grayscale PPM files. The arrow overlay is an RGB PPM. Gradient values are also available in `output_gradient_vectors.csv`.
- Signed derivative images are visualized around mid-gray: zero response is approximately 128, negative responses are darker, and positive responses are brighter. This preserves the sign for visual inspection, unlike clipping all negative values to black.
- Gradient magnitude is normalized to the output display range. Gradient theta is mapped from `[-pi, pi]` to `[0,255]` for display; the CSV contains the true angle.
- Morphological operations treat values `>=127.5` as foreground (white). For meaningful morphology, first create a binary image using thresholding or Otsu, save it, then relaunch the program with that binary image as input.
- Bilateral filtering, Canny, and 2D FFT can be computationally heavier than small-kernel filters, especially on large images.

This is an educational image-processing implementation, not a replacement for a production computer-vision library. The code is deliberately explicit so you can trace each mathematical step and explain it in a course demonstration.
