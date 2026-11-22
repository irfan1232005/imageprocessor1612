# Digital Image Processing Laboratory in C

A terminal-based **Digital Image Processing Laboratory** developed in C to explore and implement fundamental and advanced digital image processing techniques through mathematical operations.

The project aims to provide an interactive environment where users can load an image, select different processing operations from a terminal menu, customize parameters, and save the resulting images.

The primary objective is to understand how image processing algorithms work internally by implementing their mathematical foundations using **C, user-defined functions, arrays, pointers, loops, convolution, and pixel-level operations**, rather than depending on high-level image processing frameworks such as OpenCV.

## Features & Project Scope

### 1. Image Fundamentals
- Grayscale conversion
- Image intensity and pixel manipulation
- Image resizing and scaling
- Image addition and subtraction
- Image blending and basic arithmetic operations

### 2. Image Filtering and Denoising
- Gaussian filtering
- Mean (average) filtering
- Median filtering
- Bilateral filtering
- Image smoothing and noise reduction
- Gaussian derivative filtering

### 3. Derivative and Laplacian Operations
- First-order derivatives
- Second-order derivatives
- Horizontal and vertical derivatives
- Laplacian filtering
- Laplacian of Gaussian (LoG)
- Gaussian smoothing followed by derivative operations

### 4. Gradient Analysis
- Horizontal gradient (\(G_x\))
- Vertical gradient (\(G_y\))
- Gradient magnitude
- Gradient direction and angle (\(\theta\))
- Gradient direction vectors
- Gradient visualization

### 5. Edge Detection
- Sobel edge detection
- Prewitt edge detection
- Roberts edge detection
- Laplacian-based edge detection
- Canny edge detection

### 6. Image Thresholding and Segmentation
- Binary thresholding
- Inverse binary thresholding
- Adaptive mean thresholding
- Otsu's thresholding
- Multilevel thresholding
- User-defined threshold values
- Basic image segmentation

### 7. Geometric Transformations and Image Warping
- Translation and shifting
- Rotation at user-defined angles
- Image scaling and resizing
- Zooming in and out
- Squeezing and bulging transformations
- Perspective transformation and warping
- Shearing and skewing
- Horizontal and vertical reflection
- Image coordinate mapping and interpolation

### 8. Image Padding
- Constant padding
- Replicate padding
- Reflective padding

### 9. Image Enhancement and Sharpening
- Image sharpening
- Contrast stretching
- Histogram equalization
- Gamma correction
- Brightness and contrast adjustment
- Image enhancement and restoration

### 10. Morphological Image Processing
- Erosion
- Dilation
- Opening
- Closing
- Morphological gradient
- Boundary extraction
- Skeletonization and thinning
- Top-hat and black-hat transformations

### 11. Frequency-Domain Processing
- Two-dimensional Discrete Fourier Transform (DFT)
- Fast Fourier Transform (FFT)
- Inverse Fourier Transform
- Magnitude spectrum
- Phase spectrum
- Frequency spectrum visualization
- Frequency shifting (`fftshift`)
- Ideal and Gaussian low-pass filtering
- High-pass filtering
- Frequency-domain image enhancement
- Periodic noise analysis and removal

## Mathematical Implementation

Each processing operation is designed as an individual user-defined C function.

The project focuses on implementing the underlying mathematical concepts, including:

- Convolution and kernel operations
- Gaussian distribution and kernel normalization
- First-order and second-order numerical differentiation
- Gradient magnitude and direction calculations
- Threshold-based pixel classification
- Coordinate transformations and inverse mapping
- Bilinear interpolation
- Morphological neighborhood operations
- Histogram-based calculations
- Fourier transform and frequency-domain filtering

## Interactive Terminal Interface

Users can:
1. Provide an image path through the command line.
2. Load an image into the program.
3. Select an image processing category from an interactive menu.
4. Choose a specific operation.
5. Enter custom parameters such as rotation angle, Gaussian sigma, kernel size, and threshold value.
6. Execute the selected algorithm.
7. Save the processed image for visual comparison.

## Technologies

- **Programming Language:** C
- **Core Concepts:** Arrays, pointers, structures, loops, functions, memory management
- **Mathematics:** Linear algebra, calculus, convolution, numerical differentiation, Fourier analysis
- **Interface:** Command-line / Terminal
- **Development Environment:** Linux / Ubuntu

## Project Objective

This project is intended as a practical exploration of digital image processing, connecting mathematical theory with low-level programming.

Rather than treating image processing as a collection of ready-made library functions, the goal is to understand how individual pixels are manipulated, how mathematical kernels operate on images, and how spatial and frequency-domain techniques transform visual information.
