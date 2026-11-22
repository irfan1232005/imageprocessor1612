/*
 * IMAGE PROCESSING LABORATORY - C IMPLEMENTATION
 * ==============================================
 * Educational implementation of image-processing mathematics.
 * No OpenCV or image-processing library is used.
 *
 * Input/output format: PPM (P3 or P6), RGB.
 * Processing operations work on a grayscale double-precision image.
 *
 * Compile:
 *   gcc -std=c11 -O2 -Wall -Wextra -pedantic image_lab.c -lm -o image_lab
 *
 * Run:
 *   ./image_lab input.ppm
 *
 * Examples:
 *   magick photograph.jpg -compress none photograph.ppm
 *   ./image_lab photograph.ppm
 *
 * Mathematical conventions:
 *   x increases to the right; y increases downward (normal image coordinates).
 *   Derivative outputs are signed doubles. When saved for viewing, they are
 *   mapped around mid-gray so negative and positive responses both remain visible.
 *   Angles are measured in radians by atan2(Gy,Gx); a visual PPM maps [-pi,pi]
 *   to [0,255]. The CSV file stores the actual numerical angles.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <errno.h>

#define PI 3.14159265358979323846
#define MAX_PATH_LEN 512
#define MAX_KERNEL_SIZE 51

/* ================================ DATA TYPES ================================ */

typedef struct {
    int width;
    int height;
    int channels;                 /* RGB = 3 */
    unsigned char *data;          /* RGBRGBRGB... */
} ColorImage;

typedef struct {
    int width;
    int height;
    double *data;                 /* one floating-point intensity per pixel */
} GrayImage;

typedef struct {
    double gx;
    double gy;
} Gradient;

/* ============================== MEMORY / INDEX ============================== */

static ColorImage *create_color_image(int width, int height)
{
    if (width <= 0 || height <= 0) return NULL;
    ColorImage *img = (ColorImage *)malloc(sizeof(*img));
    if (!img) return NULL;
    img->width = width;
    img->height = height;
    img->channels = 3;
    img->data = (unsigned char *)calloc((size_t)width * height * 3, 1);
    if (!img->data) { free(img); return NULL; }
    return img;
}

static GrayImage *create_gray_image(int width, int height)
{
    if (width <= 0 || height <= 0) return NULL;
    GrayImage *img = (GrayImage *)malloc(sizeof(*img));
    if (!img) return NULL;
    img->width = width;
    img->height = height;
    img->data = (double *)calloc((size_t)width * height, sizeof(double));
    if (!img->data) { free(img); return NULL; }
    return img;
}

static void free_color_image(ColorImage *img)
{
    if (img) { free(img->data); free(img); }
}

static void free_gray_image(GrayImage *img)
{
    if (img) { free(img->data); free(img); }
}

static int gi(const GrayImage *img, int x, int y)
{
    return y * img->width + x;
}

static int ci(const ColorImage *img, int x, int y)
{
    return (y * img->width + x) * 3;
}

static unsigned char clamp_byte(double value)
{
    if (value < 0.0) value = 0.0;
    if (value > 255.0) value = 255.0;
    return (unsigned char)floor(value + 0.5);
}

static int clamp_int(int value, int low, int high)
{
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

/* ================================ PPM READER ================================ */

static int ppm_next_token(FILE *fp, char *buffer, size_t capacity)
{
    int c;
    size_t n = 0;

    /* Skip whitespace and comments before a token. */
    for (;;) {
        c = fgetc(fp);
        if (c == EOF) return 0;
        if (isspace((unsigned char)c)) continue;
        if (c == '#') {
            while ((c = fgetc(fp)) != EOF && c != '\n') {}
            continue;
        }
        break;
    }

    do {
        if (n + 1 < capacity) buffer[n++] = (char)c;
        c = fgetc(fp);
    } while (c != EOF && !isspace((unsigned char)c) && c != '#');

    /* Treat a Windows CRLF header separator as one delimiter for binary P6. */
    if (c == '\r') {
        int next = fgetc(fp);
        if (next != '\n' && next != EOF) ungetc(next, fp);
    }
    if (c == '#') {
        while ((c = fgetc(fp)) != EOF && c != '\n') {}
    }
    buffer[n] = '\0';
    return n > 0;
}

static int parse_positive_int(const char *s, int *value)
{
    char *end = NULL;
    errno = 0;
    long v = strtol(s, &end, 10);
    if (errno || end == s || *end != '\0' || v <= 0 || v > 100000) return 0;
    *value = (int)v;
    return 1;
}

static ColorImage *load_ppm(const char *filename)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "Cannot open '%s': %s\n", filename, strerror(errno));
        return NULL;
    }

    char token[128];
    if (!ppm_next_token(fp, token, sizeof(token))) { fclose(fp); return NULL; }
    int is_binary = strcmp(token, "P6") == 0;
    int is_ascii = strcmp(token, "P3") == 0;
    if (!is_binary && !is_ascii) {
        fprintf(stderr, "Unsupported image format. Use P3 or P6 PPM.\n");
        fclose(fp);
        return NULL;
    }

    int width, height, maxval;
    if (!ppm_next_token(fp, token, sizeof(token)) || !parse_positive_int(token, &width) ||
        !ppm_next_token(fp, token, sizeof(token)) || !parse_positive_int(token, &height) ||
        !ppm_next_token(fp, token, sizeof(token)) || !parse_positive_int(token, &maxval) || maxval > 255) {
        fprintf(stderr, "Invalid PPM header. Width, height and max value must be valid; max value <= 255.\n");
        fclose(fp);
        return NULL;
    }

    ColorImage *img = create_color_image(width, height);
    if (!img) { fclose(fp); return NULL; }
    size_t count = (size_t)width * height * 3;

    if (is_binary) {
        /* The token reader consumes the whitespace delimiter after maxval. */
        if (fread(img->data, 1, count, fp) != count) {
            fprintf(stderr, "PPM file ended before all pixels were read.\n");
            free_color_image(img); fclose(fp); return NULL;
        }
        if (maxval != 255) {
            for (size_t i = 0; i < count; ++i)
                img->data[i] = clamp_byte((double)img->data[i] * 255.0 / maxval);
        }
    } else {
        for (size_t i = 0; i < count; ++i) {
            int v;
            if (!ppm_next_token(fp, token, sizeof(token))) {
                fprintf(stderr, "PPM file ended before all pixels were read.\n");
                free_color_image(img); fclose(fp); return NULL;
            }
            char *end = NULL;
            long parsed = strtol(token, &end, 10);
            if (end == token || *end != '\0') {
                free_color_image(img); fclose(fp); return NULL;
            }
            v = (int)parsed;
            img->data[i] = clamp_byte((double)v * 255.0 / maxval);
        }
    }

    fclose(fp);
    return img;
}

static int save_ppm(const ColorImage *img, const char *filename)
{
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "Cannot write '%s': %s\n", filename, strerror(errno));
        return 0;
    }
    fprintf(fp, "P6\n%d %d\n255\n", img->width, img->height);
    size_t count = (size_t)img->width * img->height * 3;
    int ok = fwrite(img->data, 1, count, fp) == count;
    fclose(fp);
    if (ok) printf("Saved image: %s\n", filename);
    return ok;
}

/* ============================ RGB / GRAYSCALE =============================== */

static GrayImage *convert_to_grayscale(const ColorImage *src)
{
    GrayImage *gray = create_gray_image(src->width, src->height);
    if (!gray) return NULL;
    for (int y = 0; y < src->height; ++y) {
        for (int x = 0; x < src->width; ++x) {
            int p = ci(src, x, y);
            double r = src->data[p];
            double g = src->data[p + 1];
            double b = src->data[p + 2];
            /* Luminance approximation: Y = 0.299R + 0.587G + 0.114B. */
            gray->data[gi(gray, x, y)] = 0.299 * r + 0.587 * g + 0.114 * b;
        }
    }
    return gray;
}

static ColorImage *gray_to_color(const GrayImage *src, int visualization_mode)
{
    ColorImage *out = create_color_image(src->width, src->height);
    if (!out) return NULL;

    double max_abs = 0.0;
    if (visualization_mode == 1 || visualization_mode == 2) {
        for (int i = 0; i < src->width * src->height; ++i) {
            double candidate = visualization_mode == 1 ? fabs(src->data[i]) : src->data[i];
            if (candidate > max_abs) max_abs = candidate;
        }
        if (max_abs < 1e-12) max_abs = 1.0;
    }

    for (int y = 0; y < src->height; ++y) {
        for (int x = 0; x < src->width; ++x) {
            double v = src->data[gi(src, x, y)];
            if (visualization_mode == 1) {
                /* Signed response visualization: 0 response -> gray 128. */
                v = 127.5 + 127.5 * v / max_abs;
            } else if (visualization_mode == 2) {
                /* Nonnegative visualization: scale maximum response to white. */
                v = 255.0 * v / max_abs;
            }
            unsigned char c = clamp_byte(v);
            int p = ci(out, x, y);
            out->data[p] = c;
            out->data[p + 1] = c;
            out->data[p + 2] = c;
        }
    }
    return out;
}

static int save_gray(const GrayImage *img, const char *filename, int mode)
{
    ColorImage *color = gray_to_color(img, mode);
    if (!color) return 0;
    int ok = save_ppm(color, filename);
    free_color_image(color);
    return ok;
}

/* ============================== BORDER ACCESS =============================== */

static double get_pixel_replicate(const GrayImage *img, int x, int y)
{
    x = clamp_int(x, 0, img->width - 1);
    y = clamp_int(y, 0, img->height - 1);
    return img->data[gi(img, x, y)];
}

static double get_pixel_reflect(const GrayImage *img, int x, int y)
{
    /* Reflect-101 style padding: ... c b | a b c d | c b ... */
    while (x < 0 || x >= img->width) {
        if (x < 0) x = -x;
        if (x >= img->width) x = 2 * img->width - 2 - x;
        if (img->width == 1) x = 0;
    }
    while (y < 0 || y >= img->height) {
        if (y < 0) y = -y;
        if (y >= img->height) y = 2 * img->height - 2 - y;
        if (img->height == 1) y = 0;
    }
    return img->data[gi(img, x, y)];
}

/* ================================ CONVOLUTION ================================ */

static GrayImage *convolve_2d(const GrayImage *src, const double *kernel, int ksize)
{
    if (ksize < 1 || ksize % 2 == 0) return NULL;
    GrayImage *out = create_gray_image(src->width, src->height);
    if (!out) return NULL;
    int radius = ksize / 2;

    for (int y = 0; y < src->height; ++y) {
        for (int x = 0; x < src->width; ++x) {
            double sum = 0.0;
            for (int ky = -radius; ky <= radius; ++ky) {
                for (int kx = -radius; kx <= radius; ++kx) {
                    double pixel = get_pixel_replicate(src, x + kx, y + ky);
                    double weight = kernel[(ky + radius) * ksize + (kx + radius)];
                    sum += pixel * weight;
                }
            }
            out->data[gi(out, x, y)] = sum;
        }
    }
    return out;
}

static int valid_odd_kernel(int k)
{
    return k >= 1 && k <= MAX_KERNEL_SIZE && (k % 2 == 1);
}

/* =============================== GAUSSIAN =================================== */

static GrayImage *gaussian_filter(const GrayImage *src, int ksize, double sigma)
{
    if (!valid_odd_kernel(ksize) || sigma <= 0.0) {
        printf("Gaussian filter requires an odd kernel size (1..%d) and sigma > 0.\n", MAX_KERNEL_SIZE);
        return NULL;
    }

    int radius = ksize / 2;
    double *kernel = (double *)malloc((size_t)ksize * sizeof(double));
    GrayImage *temp = create_gray_image(src->width, src->height);
    GrayImage *out = create_gray_image(src->width, src->height);
    if (!kernel || !temp || !out) {
        free(kernel); free_gray_image(temp); free_gray_image(out); return NULL;
    }

    /* 1-D Gaussian G(x)=exp(-x^2/(2 sigma^2)); normalize sum to one. */
    double sum = 0.0;
    for (int x = -radius; x <= radius; ++x) {
        double value = exp(-((double)x * x) / (2.0 * sigma * sigma));
        kernel[x + radius] = value;
        sum += value;
    }
    for (int i = 0; i < ksize; ++i) kernel[i] /= sum;

    /* Separable convolution: horizontal pass followed by vertical pass. */
    for (int y = 0; y < src->height; ++y) {
        for (int x = 0; x < src->width; ++x) {
            double value = 0.0;
            for (int k = -radius; k <= radius; ++k)
                value += get_pixel_replicate(src, x + k, y) * kernel[k + radius];
            temp->data[gi(temp, x, y)] = value;
        }
    }
    for (int y = 0; y < src->height; ++y) {
        for (int x = 0; x < src->width; ++x) {
            double value = 0.0;
            for (int k = -radius; k <= radius; ++k)
                value += get_pixel_replicate(temp, x, y + k) * kernel[k + radius];
            out->data[gi(out, x, y)] = value;
        }
    }

    free(kernel);
    free_gray_image(temp);
    return out;
}

/* ============================ DERIVATIVE OPERATORS ========================== */

static GrayImage *first_derivative_x(const GrayImage *src)
{
    /* Central difference: dI/dx ~= [I(x+1,y)-I(x-1,y)]/2. */
    const double k[9] = { 0,0,0, -0.5,0,0.5, 0,0,0 };
    return convolve_2d(src, k, 3);
}

static GrayImage *first_derivative_y(const GrayImage *src)
{
    /* Central difference: dI/dy ~= [I(x,y+1)-I(x,y-1)]/2. */
    const double k[9] = { 0,-0.5,0, 0,0,0, 0,0.5,0 };
    return convolve_2d(src, k, 3);
}

static GrayImage *second_derivative_x(const GrayImage *src)
{
    /* Second difference: d2I/dx2 ~= I(x-1)-2I(x)+I(x+1). */
    const double k[9] = { 0,0,0, 1,-2,1, 0,0,0 };
    return convolve_2d(src, k, 3);
}

static GrayImage *second_derivative_y(const GrayImage *src)
{
    /* Second difference: d2I/dy2 ~= I(y-1)-2I(y)+I(y+1). */
    const double k[9] = { 0,1,0, 0,-2,0, 0,1,0 };
    return convolve_2d(src, k, 3);
}

static GrayImage *laplacian_filter(const GrayImage *src, int eight_neighbor)
{
    /* 4-neighbor or 8-neighbor discrete approximation to d2I/dx2+d2I/dy2. */
    const double k4[9] = { 0,1,0, 1,-4,1, 0,1,0 };
    const double k8[9] = { 1,1,1, 1,-8,1, 1,1,1 };
    return convolve_2d(src, eight_neighbor ? k8 : k4, 3);
}

/* Laplacian of Gaussian: smooth first to suppress noise, then apply Laplacian. */
static GrayImage *laplacian_of_gaussian(const GrayImage *src, int ksize, double sigma)
{
    GrayImage *smooth = gaussian_filter(src, ksize, sigma);
    if (!smooth) return NULL;
    GrayImage *result = laplacian_filter(smooth, 0);
    free_gray_image(smooth);
    return result;
}

/* =============================== SOBEL GRADIENT ============================= */

static GrayImage *sobel_x(const GrayImage *src)
{
    const double k[9] = { -1,0,1, -2,0,2, -1,0,1 };
    return convolve_2d(src, k, 3);
}

static GrayImage *sobel_y(const GrayImage *src)
{
    const double k[9] = { -1,-2,-1, 0,0,0, 1,2,1 };
    return convolve_2d(src, k, 3);
}

static GrayImage *gradient_magnitude(const GrayImage *src, GrayImage **gx_out, GrayImage **gy_out)
{
    GrayImage *gx = sobel_x(src);
    GrayImage *gy = sobel_y(src);
    GrayImage *mag = create_gray_image(src->width, src->height);
    if (!gx || !gy || !mag) {
        free_gray_image(gx); free_gray_image(gy); free_gray_image(mag); return NULL;
    }
    for (int i = 0; i < src->width * src->height; ++i)
        mag->data[i] = hypot(gx->data[i], gy->data[i]); /* sqrt(Gx^2+Gy^2) */
    if (gx_out) *gx_out = gx; else free_gray_image(gx);
    if (gy_out) *gy_out = gy; else free_gray_image(gy);
    return mag;
}

static GrayImage *gradient_direction_theta(const GrayImage *src)
{
    GrayImage *gx = sobel_x(src);
    GrayImage *gy = sobel_y(src);
    GrayImage *theta = create_gray_image(src->width, src->height);
    if (!gx || !gy || !theta) {
        free_gray_image(gx); free_gray_image(gy); free_gray_image(theta); return NULL;
    }
    for (int i = 0; i < src->width * src->height; ++i) {
        double angle = atan2(gy->data[i], gx->data[i]);
        theta->data[i] = (angle + PI) * 255.0 / (2.0 * PI);
    }
    free_gray_image(gx); free_gray_image(gy);
    return theta;
}

static int save_gradient_vectors_csv(const GrayImage *src, const char *filename, int spacing)
{
    if (spacing < 1) spacing = 1;
    GrayImage *gx = sobel_x(src);
    GrayImage *gy = sobel_y(src);
    if (!gx || !gy) { free_gray_image(gx); free_gray_image(gy); return 0; }
    FILE *fp = fopen(filename, "w");
    if (!fp) { free_gray_image(gx); free_gray_image(gy); return 0; }
    fprintf(fp, "x,y,Gx,Gy,magnitude,theta_radians\n");
    for (int y = 0; y < src->height; y += spacing) {
        for (int x = 0; x < src->width; x += spacing) {
            int p = gi(src, x, y);
            double dx = gx->data[p], dy = gy->data[p];
            fprintf(fp, "%d,%d,%.8f,%.8f,%.8f,%.8f\n",
                    x, y, dx, dy, hypot(dx, dy), atan2(dy, dx));
        }
    }
    fclose(fp);
    free_gray_image(gx); free_gray_image(gy);
    printf("Saved numerical gradient vectors: %s\n", filename);
    return 1;
}

/* Bresenham line drawing, used to draw sampled gradient arrows. */
static void draw_line(ColorImage *img, int x0, int y0, int x1, int y1,
                      unsigned char r, unsigned char g, unsigned char b)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        if (x0 >= 0 && x0 < img->width && y0 >= 0 && y0 < img->height) {
            int p = ci(img, x0, y0);
            img->data[p] = r; img->data[p+1] = g; img->data[p+2] = b;
        }
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static int save_gradient_vector_overlay(const GrayImage *src, const char *filename,
                                        int spacing, double arrow_scale)
{
    if (spacing < 1) spacing = 1;
    GrayImage *gx = sobel_x(src), *gy = sobel_y(src);
    ColorImage *out = gray_to_color(src, 0);
    if (!gx || !gy || !out) {
        free_gray_image(gx); free_gray_image(gy); free_color_image(out); return 0;
    }
    for (int y = spacing / 2; y < src->height; y += spacing) {
        for (int x = spacing / 2; x < src->width; x += spacing) {
            int p = gi(src, x, y);
            double dx = gx->data[p], dy = gy->data[p];
            double mag = hypot(dx, dy);
            if (mag < 1e-9) continue;
            /* Normalize arrow length to the requested display scale. */
            double length = arrow_scale;
            int x2 = (int)lround(x + length * dx / mag);
            int y2 = (int)lround(y + length * dy / mag);
            draw_line(out, x, y, x2, y2, 255, 40, 40);
            /* Small arrow head, using two short back-facing segments. */
            double angle = atan2(dy, dx);
            double head = length * 0.35;
            int hx1 = (int)lround(x2 - head * cos(angle - 0.6));
            int hy1 = (int)lround(y2 - head * sin(angle - 0.6));
            int hx2 = (int)lround(x2 - head * cos(angle + 0.6));
            int hy2 = (int)lround(y2 - head * sin(angle + 0.6));
            draw_line(out, x2, y2, hx1, hy1, 255, 40, 40);
            draw_line(out, x2, y2, hx2, hy2, 255, 40, 40);
        }
    }
    int ok = save_ppm(out, filename);
    free_gray_image(gx); free_gray_image(gy); free_color_image(out);
    return ok;
}

/* ============================== SMOOTHING FILTERS =========================== */

static GrayImage *mean_filter(const GrayImage *src, int ksize)
{
    if (!valid_odd_kernel(ksize)) return NULL;
    int n = ksize * ksize;
    double *k = (double *)malloc((size_t)n * sizeof(double));
    if (!k) return NULL;
    for (int i = 0; i < n; ++i) k[i] = 1.0 / n;
    GrayImage *out = convolve_2d(src, k, ksize);
    free(k);
    return out;
}

static int compare_double(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

static GrayImage *median_filter(const GrayImage *src, int ksize)
{
    if (!valid_odd_kernel(ksize)) return NULL;
    int n = ksize * ksize, radius = ksize / 2;
    double *values = (double *)malloc((size_t)n * sizeof(double));
    GrayImage *out = create_gray_image(src->width, src->height);
    if (!values || !out) { free(values); free_gray_image(out); return NULL; }
    for (int y = 0; y < src->height; ++y) {
        for (int x = 0; x < src->width; ++x) {
            int q = 0;
            for (int dy = -radius; dy <= radius; ++dy)
                for (int dx = -radius; dx <= radius; ++dx)
                    values[q++] = get_pixel_replicate(src, x + dx, y + dy);
            qsort(values, (size_t)n, sizeof(double), compare_double);
            out->data[gi(out, x, y)] = values[n / 2];
        }
    }
    free(values);
    return out;
}

static GrayImage *bilateral_filter(const GrayImage *src, int ksize,
                                   double sigma_space, double sigma_range)
{
    if (!valid_odd_kernel(ksize) || sigma_space <= 0.0 || sigma_range <= 0.0) return NULL;
    GrayImage *out = create_gray_image(src->width, src->height);
    if (!out) return NULL;
    int radius = ksize / 2;
    double two_space = 2.0 * sigma_space * sigma_space;
    double two_range = 2.0 * sigma_range * sigma_range;

    for (int y = 0; y < src->height; ++y) {
        for (int x = 0; x < src->width; ++x) {
            double center = get_pixel_replicate(src, x, y);
            double weighted_sum = 0.0, weight_sum = 0.0;
            for (int dy = -radius; dy <= radius; ++dy) {
                for (int dx = -radius; dx <= radius; ++dx) {
                    double neighbor = get_pixel_replicate(src, x + dx, y + dy);
                    double spatial2 = (double)(dx * dx + dy * dy);
                    double range2 = (neighbor - center) * (neighbor - center);
                    /* w = exp(-||p-q||^2/(2 sigma_s^2)) * exp(-(Ip-Iq)^2/(2 sigma_r^2)). */
                    double w = exp(-spatial2 / two_space) * exp(-range2 / two_range);
                    weighted_sum += w * neighbor;
                    weight_sum += w;
                }
            }
            out->data[gi(out, x, y)] = weight_sum > 0.0 ? weighted_sum / weight_sum : center;
        }
    }
    return out;
}

/* ================================ THRESHOLDING =============================== */

static GrayImage *binary_threshold(const GrayImage *src, double threshold)
{
    GrayImage *out = create_gray_image(src->width, src->height);
    if (!out) return NULL;
    for (int i = 0; i < src->width * src->height; ++i)
        out->data[i] = src->data[i] >= threshold ? 255.0 : 0.0;
    return out;
}

static GrayImage *inverse_binary_threshold(const GrayImage *src, double threshold)
{
    GrayImage *out = create_gray_image(src->width, src->height);
    if (!out) return NULL;
    for (int i = 0; i < src->width * src->height; ++i)
        out->data[i] = src->data[i] >= threshold ? 0.0 : 255.0;
    return out;
}

static GrayImage *adaptive_mean_threshold(const GrayImage *src, int block_size, double C)
{
    if (!valid_odd_kernel(block_size) || block_size < 3) return NULL;
    int w = src->width, h = src->height;
    /* Integral image enables O(1) local rectangle sums. */
    double *integral = (double *)calloc((size_t)(w + 1) * (h + 1), sizeof(double));
    GrayImage *out = create_gray_image(w, h);
    if (!integral || !out) { free(integral); free_gray_image(out); return NULL; }
    int stride = w + 1;
    for (int y = 1; y <= h; ++y) {
        double row_sum = 0.0;
        for (int x = 1; x <= w; ++x) {
            row_sum += src->data[gi(src, x - 1, y - 1)];
            integral[y * stride + x] = integral[(y - 1) * stride + x] + row_sum;
        }
    }

    int radius = block_size / 2;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int x1 = clamp_int(x - radius, 0, w - 1);
            int y1 = clamp_int(y - radius, 0, h - 1);
            int x2 = clamp_int(x + radius, 0, w - 1);
            int y2 = clamp_int(y + radius, 0, h - 1);
            double sum = integral[(y2 + 1) * stride + (x2 + 1)]
                       - integral[y1 * stride + (x2 + 1)]
                       - integral[(y2 + 1) * stride + x1]
                       + integral[y1 * stride + x1];
            double mean = sum / ((x2 - x1 + 1) * (y2 - y1 + 1));
            double threshold = mean - C;
            out->data[gi(out, x, y)] = src->data[gi(src, x, y)] >= threshold ? 255.0 : 0.0;
        }
    }
    free(integral);
    return out;
}

static int otsu_threshold_value(const GrayImage *src)
{
    unsigned long long hist[256] = {0};
    unsigned long long total = (unsigned long long)src->width * src->height;
    double sum_total = 0.0;
    for (int i = 0; i < (int)total; ++i) {
        int v = clamp_int((int)lround(src->data[i]), 0, 255);
        hist[v]++;
        sum_total += v;
    }

    unsigned long long background_count = 0;
    double background_sum = 0.0;
    double best_variance = -1.0;
    int best_threshold = 0;

    for (int t = 0; t < 256; ++t) {
        background_count += hist[t];
        background_sum += (double)t * hist[t];
        if (background_count == 0) continue;
        unsigned long long foreground_count = total - background_count;
        if (foreground_count == 0) break;
        double mean_b = background_sum / background_count;
        double mean_f = (sum_total - background_sum) / foreground_count;
        double diff = mean_b - mean_f;
        double between = (double)background_count * foreground_count * diff * diff;
        if (between > best_variance) {
            best_variance = between;
            best_threshold = t;
        }
    }
    return best_threshold;
}

static GrayImage *otsu_threshold(const GrayImage *src, int *threshold_used)
{
    int threshold = otsu_threshold_value(src);
    if (threshold_used) *threshold_used = threshold;
    return binary_threshold(src, threshold);
}

/* Multi-level Otsu: dynamic programming minimizes within-class histogram SSE. */
static GrayImage *multilevel_otsu_threshold(const GrayImage *src, int classes)
{
    if (classes < 2 || classes > 8) {
        printf("Choose between 2 and 8 classes.\n");
        return NULL;
    }

    unsigned long long hist[256] = {0};
    for (int i = 0; i < src->width * src->height; ++i) {
        int v = clamp_int((int)lround(src->data[i]), 0, 255);
        hist[v]++;
    }

    double cw[256] = {0}, cs[256] = {0}, cs2[256] = {0};
    for (int i = 0; i < 256; ++i) {
        double w = (double)hist[i];
        cw[i] = w + (i ? cw[i - 1] : 0.0);
        cs[i] = w * i + (i ? cs[i - 1] : 0.0);
        cs2[i] = w * i * i + (i ? cs2[i - 1] : 0.0);
    }

    /* Cost of assigning histogram values [a,b] to one constant class mean. */
    double interval_cost[256][256];
    for (int a = 0; a < 256; ++a) {
        for (int b = a; b < 256; ++b) {
            double w = cw[b] - (a ? cw[a - 1] : 0.0);
            double sum = cs[b] - (a ? cs[a - 1] : 0.0);
            double sum2 = cs2[b] - (a ? cs2[a - 1] : 0.0);
            interval_cost[a][b] = w > 0.0 ? sum2 - sum * sum / w : 0.0;
        }
    }

    double dp[9][256];
    int prev[9][256];
    for (int k = 0; k <= classes; ++k)
        for (int t = 0; t < 256; ++t) { dp[k][t] = HUGE_VAL; prev[k][t] = -1; }

    for (int t = 0; t < 256; ++t) dp[1][t] = interval_cost[0][t];

    for (int k = 2; k <= classes; ++k) {
        for (int t = k - 1; t < 256; ++t) {
            for (int s = k - 2; s < t; ++s) {
                double candidate = dp[k - 1][s] + interval_cost[s + 1][t];
                if (candidate < dp[k][t]) {
                    dp[k][t] = candidate;
                    prev[k][t] = s;
                }
            }
        }
    }

    int boundaries[8];
    int t = 255;
    for (int k = classes; k >= 2; --k) {
        int s = prev[k][t];
        if (s < 0) s = 0;
        boundaries[k - 2] = s;
        t = s;
    }

    GrayImage *out = create_gray_image(src->width, src->height);
    if (!out) return NULL;
    for (int i = 0; i < src->width * src->height; ++i) {
        int v = clamp_int((int)lround(src->data[i]), 0, 255);
        int label = 0;
        while (label < classes - 1 && v > boundaries[label]) label++;
        /* Quantization display: class 0 -> 0, last class -> 255. */
        out->data[i] = classes == 1 ? 0.0 : 255.0 * label / (classes - 1);
    }
    return out;
}

/* ============================= GEOMETRIC WARPING ============================ */

static double bilinear_sample(const GrayImage *src, double x, double y)
{
    int x0 = (int)floor(x), y0 = (int)floor(y);
    int x1 = x0 + 1, y1 = y0 + 1;
    double dx = x - x0, dy = y - y0;
    double p00 = get_pixel_replicate(src, x0, y0);
    double p10 = get_pixel_replicate(src, x1, y0);
    double p01 = get_pixel_replicate(src, x0, y1);
    double p11 = get_pixel_replicate(src, x1, y1);
    return (1.0-dx)*(1.0-dy)*p00 + dx*(1.0-dy)*p10
         + (1.0-dx)*dy*p01 + dx*dy*p11;
}

static GrayImage *warp_inverse_map(const GrayImage *src, int out_w, int out_h,
                                   double (*map)(double, double, double *, double *, void *),
                                   void *context)
{
    GrayImage *out = create_gray_image(out_w, out_h);
    if (!out) return NULL;
    for (int y = 0; y < out_h; ++y) {
        for (int x = 0; x < out_w; ++x) {
            double sx, sy;
            if (map(x, y, &sx, &sy, context) &&
                sx >= 0.0 && sy >= 0.0 && sx <= src->width - 1.0 && sy <= src->height - 1.0)
                out->data[gi(out, x, y)] = bilinear_sample(src, sx, sy);
            else
                out->data[gi(out, x, y)] = 0.0;
        }
    }
    return out;
}

typedef struct { double dx, dy; } TranslateContext;
static double map_translate(double x, double y, double *sx, double *sy, void *ctx)
{
    TranslateContext *c = (TranslateContext *)ctx;
    *sx = x - c->dx; *sy = y - c->dy;
    return 1.0;
}

static GrayImage *translate_image(const GrayImage *src, double dx, double dy)
{
    TranslateContext c = {dx, dy};
    return warp_inverse_map(src, src->width, src->height, map_translate, &c);
}

typedef struct { double angle, old_cx, old_cy, new_cx, new_cy; } RotateContext;
static double map_rotate(double x, double y, double *sx, double *sy, void *ctx)
{
    RotateContext *c = (RotateContext *)ctx;
    double dx = x - c->new_cx, dy = y - c->new_cy;
    double co = cos(c->angle), si = sin(c->angle);
    /* Inverse rotation maps destination coordinates back to source.
       With image y increasing downward, positive degrees appear counter-clockwise. */
    *sx = co * dx - si * dy + c->old_cx;
    *sy = si * dx + co * dy + c->old_cy;
    return 1.0;
}

static GrayImage *rotate_image(const GrayImage *src, double degrees)
{
    double a = degrees * PI / 180.0, co = cos(a), si = sin(a);
    int out_w = (int)ceil(fabs(src->width * co) + fabs(src->height * si));
    int out_h = (int)ceil(fabs(src->height * co) + fabs(src->width * si));
    RotateContext c = {a, (src->width-1)/2.0, (src->height-1)/2.0,
                       (out_w-1)/2.0, (out_h-1)/2.0};
    return warp_inverse_map(src, out_w, out_h, map_rotate, &c);
}

typedef struct { double sx, sy, old_cx, old_cy, new_cx, new_cy; } ScaleContext;
static double map_scale(double x, double y, double *sx, double *sy, void *ctx)
{
    ScaleContext *c = (ScaleContext *)ctx;
    *sx = (x - c->new_cx) / c->sx + c->old_cx;
    *sy = (y - c->new_cy) / c->sy + c->old_cy;
    return 1.0;
}

static GrayImage *scale_image(const GrayImage *src, double scale_x, double scale_y)
{
    if (scale_x <= 0.0 || scale_y <= 0.0 || scale_x > 20.0 || scale_y > 20.0) return NULL;
    int out_w = (int)lround(src->width * scale_x);
    int out_h = (int)lround(src->height * scale_y);
    if (out_w < 1) out_w = 1;
    if (out_h < 1) out_h = 1;
    ScaleContext c = {scale_x, scale_y, (src->width-1)/2.0, (src->height-1)/2.0,
                      (out_w-1)/2.0, (out_h-1)/2.0};
    return warp_inverse_map(src, out_w, out_h, map_scale, &c);
}

/* Zoom keeps output size fixed and samples around the image center. */
typedef struct { double factor, cx, cy; } ZoomContext;
static double map_zoom(double x, double y, double *sx, double *sy, void *ctx)
{
    ZoomContext *c = (ZoomContext *)ctx;
    *sx = (x - c->cx) / c->factor + c->cx;
    *sy = (y - c->cy) / c->factor + c->cy;
    return 1.0;
}
static GrayImage *zoom_image(const GrayImage *src, double factor)
{
    if (factor <= 0.0 || factor > 20.0) return NULL;
    ZoomContext c = {factor, (src->width-1)/2.0, (src->height-1)/2.0};
    return warp_inverse_map(src, src->width, src->height, map_zoom, &c);
}

static GrayImage *shear_image(const GrayImage *src, double shx, double shy)
{
    if (fabs(1.0 - shx * shy) < 1e-6 || fabs(shx) > 5.0 || fabs(shy) > 5.0) return NULL;
    /* Compute expanded canvas from forward-transformed image corners. */
    double xs[4] = {0, src->width-1.0, src->width-1.0, 0};
    double ys[4] = {0, 0, src->height-1.0, src->height-1.0};
    double minx = HUGE_VAL, miny = HUGE_VAL, maxx = -HUGE_VAL, maxy = -HUGE_VAL;
    for (int i = 0; i < 4; ++i) {
        double X = xs[i] + shx * ys[i], Y = ys[i] + shy * xs[i];
        if (X < minx) minx = X;
        if (X > maxx) maxx = X;
        if (Y < miny) miny = Y;
        if (Y > maxy) maxy = Y;
    }
    int out_w = (int)ceil(maxx - minx + 1.0);
    int out_h = (int)ceil(maxy - miny + 1.0);
    GrayImage *out = create_gray_image(out_w, out_h);
    if (!out) return NULL;
    double det = 1.0 - shx * shy;
    for (int y = 0; y < out_h; ++y) for (int x = 0; x < out_w; ++x) {
        double X = x + minx, Y = y + miny;
        double sx = (X - shx * Y) / det;
        double sy = (Y - shy * X) / det;
        if (sx >= 0 && sy >= 0 && sx <= src->width-1 && sy <= src->height-1)
            out->data[gi(out, x, y)] = bilinear_sample(src, sx, sy);
    }
    return out;
}

typedef struct { double strength, cx, cy, radius; int bulge; } RadialContext;
static double map_radial(double x, double y, double *sx, double *sy, void *ctx)
{
    RadialContext *c = (RadialContext *)ctx;
    double dx = x - c->cx, dy = y - c->cy;
    double r = hypot(dx, dy);
    if (r < 1e-12) { *sx = c->cx; *sy = c->cy; return 1.0; }
    double rn = r / c->radius;
    if (rn >= 1.0) { *sx = x; *sy = y; return 1.0; }
    double influence = (1.0 - rn) * (1.0 - rn);
    /* bulge: source radius is reduced; squeeze: source radius is increased. */
    double source_r = c->bulge ? r * (1.0 - c->strength * influence)
                               : r * (1.0 + c->strength * influence);
    double factor = source_r / r;
    *sx = c->cx + dx * factor;
    *sy = c->cy + dy * factor;
    return 1.0;
}

static GrayImage *radial_warp(const GrayImage *src, double strength, int bulge)
{
    if (strength < 0.0 || strength > 0.9) return NULL;
    RadialContext c = {strength, (src->width-1)/2.0, (src->height-1)/2.0,
                       hypot(src->width, src->height) / 2.0, bulge};
    return warp_inverse_map(src, src->width, src->height, map_radial, &c);
}

static GrayImage *reflect_image(const GrayImage *src, int horizontal)
{
    GrayImage *out = create_gray_image(src->width, src->height);
    if (!out) return NULL;
    for (int y = 0; y < src->height; ++y)
        for (int x = 0; x < src->width; ++x) {
            int sx = horizontal ? src->width - 1 - x : x;
            int sy = horizontal ? y : src->height - 1 - y;
            out->data[gi(out, x, y)] = src->data[gi(src, sx, sy)];
        }
    return out;
}

/* Solve an 8x8 linear system for a projective homography. */
static int solve_linear_8(double a[8][9], double solution[8])
{
    for (int col = 0; col < 8; ++col) {
        int pivot = col;
        for (int row = col + 1; row < 8; ++row)
            if (fabs(a[row][col]) > fabs(a[pivot][col])) pivot = row;
        if (fabs(a[pivot][col]) < 1e-10) return 0;
        if (pivot != col) for (int j = col; j < 9; ++j) {
            double t = a[col][j]; a[col][j] = a[pivot][j]; a[pivot][j] = t;
        }
        double divisor = a[col][col];
        for (int j = col; j < 9; ++j) a[col][j] /= divisor;
        for (int row = 0; row < 8; ++row) if (row != col) {
            double factor = a[row][col];
            for (int j = col; j < 9; ++j) a[row][j] -= factor * a[col][j];
        }
    }
    for (int i = 0; i < 8; ++i) solution[i] = a[i][8];
    return 1;
}

static int invert_3x3(const double h[9], double inv[9])
{
    double det = h[0]*(h[4]*h[8]-h[5]*h[7]) - h[1]*(h[3]*h[8]-h[5]*h[6])
               + h[2]*(h[3]*h[7]-h[4]*h[6]);
    if (fabs(det) < 1e-12) return 0;
    inv[0]=(h[4]*h[8]-h[5]*h[7])/det; inv[1]=(h[2]*h[7]-h[1]*h[8])/det; inv[2]=(h[1]*h[5]-h[2]*h[4])/det;
    inv[3]=(h[5]*h[6]-h[3]*h[8])/det; inv[4]=(h[0]*h[8]-h[2]*h[6])/det; inv[5]=(h[2]*h[3]-h[0]*h[5])/det;
    inv[6]=(h[3]*h[7]-h[4]*h[6])/det; inv[7]=(h[1]*h[6]-h[0]*h[7])/det; inv[8]=(h[0]*h[4]-h[1]*h[3])/det;
    return 1;
}

static GrayImage *perspective_warp(const GrayImage *src, const double dst_x[4], const double dst_y[4])
{
    /* Four source corners correspond to four user-specified destination corners. */
    double sx[4] = {0, src->width-1.0, src->width-1.0, 0};
    double sy[4] = {0, 0, src->height-1.0, src->height-1.0};
    double a[8][9] = {{0}};
    for (int i = 0; i < 4; ++i) {
        double x = sx[i], y = sy[i], X = dst_x[i], Y = dst_y[i];
        a[2*i][0]=x; a[2*i][1]=y; a[2*i][2]=1; a[2*i][6]=-X*x; a[2*i][7]=-X*y; a[2*i][8]=X;
        a[2*i+1][3]=x; a[2*i+1][4]=y; a[2*i+1][5]=1; a[2*i+1][6]=-Y*x; a[2*i+1][7]=-Y*y; a[2*i+1][8]=Y;
    }
    double v[8];
    if (!solve_linear_8(a, v)) return NULL;
    double h[9] = {v[0],v[1],v[2],v[3],v[4],v[5],v[6],v[7],1.0};
    double inv[9];
    if (!invert_3x3(h, inv)) return NULL;

    /* This version retains the original canvas dimensions; pixels outside map to black. */
    GrayImage *out = create_gray_image(src->width, src->height);
    if (!out) return NULL;
    for (int y = 0; y < out->height; ++y) for (int x = 0; x < out->width; ++x) {
        double den = inv[6]*x + inv[7]*y + inv[8];
        if (fabs(den) < 1e-12) continue;
        double X = (inv[0]*x + inv[1]*y + inv[2]) / den;
        double Y = (inv[3]*x + inv[4]*y + inv[5]) / den;
        if (X >= 0 && Y >= 0 && X <= src->width-1 && Y <= src->height-1)
            out->data[gi(out, x, y)] = bilinear_sample(src, X, Y);
    }
    return out;
}

/* ================================ PADDING =================================== */

static GrayImage *pad_image(const GrayImage *src, int pad, int mode, double constant)
{
    if (pad < 0 || pad > 1000) return NULL;
    GrayImage *out = create_gray_image(src->width + 2*pad, src->height + 2*pad);
    if (!out) return NULL;
    for (int y = 0; y < out->height; ++y) for (int x = 0; x < out->width; ++x) {
        int sx = x - pad, sy = y - pad;
        double v;
        if (sx >= 0 && sx < src->width && sy >= 0 && sy < src->height)
            v = src->data[gi(src, sx, sy)];
        else if (mode == 1)
            v = get_pixel_replicate(src, sx, sy);
        else if (mode == 2)
            v = get_pixel_reflect(src, sx, sy);
        else
            v = constant;
        out->data[gi(out, x, y)] = v;
    }
    return out;
}

/* ============================ ADDITIONAL EDGE OPS =========================== */

static GrayImage *prewitt_x(const GrayImage *src)
{
    const double k[9] = { -1,0,1, -1,0,1, -1,0,1 };
    return convolve_2d(src, k, 3);
}

static GrayImage *prewitt_y(const GrayImage *src)
{
    const double k[9] = { -1,-1,-1, 0,0,0, 1,1,1 };
    return convolve_2d(src, k, 3);
}

static GrayImage *roberts_x(const GrayImage *src)
{
    GrayImage *out = create_gray_image(src->width, src->height);
    if (!out) return NULL;
    for (int y = 0; y < src->height; ++y)
        for (int x = 0; x < src->width; ++x)
            out->data[gi(out, x, y)] = get_pixel_replicate(src, x, y)
                                      - get_pixel_replicate(src, x + 1, y + 1);
    return out;
}

static GrayImage *roberts_y(const GrayImage *src)
{
    GrayImage *out = create_gray_image(src->width, src->height);
    if (!out) return NULL;
    for (int y = 0; y < src->height; ++y)
        for (int x = 0; x < src->width; ++x)
            out->data[gi(out, x, y)] = get_pixel_replicate(src, x + 1, y)
                                      - get_pixel_replicate(src, x, y + 1);
    return out;
}

/* Canny edge detector: Gaussian smoothing -> Sobel -> NMS -> hysteresis. */
static GrayImage *canny_edge_detector(const GrayImage *src, int blur_size,
                                      double sigma, double low_threshold,
                                      double high_threshold)
{
    if (low_threshold < 0.0 || high_threshold <= low_threshold) return NULL;
    GrayImage *smooth = gaussian_filter(src, blur_size, sigma);
    GrayImage *gx = NULL, *gy = NULL;
    GrayImage *mag = NULL;
    GrayImage *nms = NULL;
    GrayImage *edges = NULL;
    if (!smooth) return NULL;
    mag = gradient_magnitude(smooth, &gx, &gy);
    if (!mag) { free_gray_image(smooth); return NULL; }
    nms = create_gray_image(src->width, src->height);
    edges = create_gray_image(src->width, src->height);
    unsigned char *state = (unsigned char *)calloc((size_t)src->width * src->height, 1);
    int *queue = (int *)malloc((size_t)src->width * src->height * sizeof(int));
    if (!nms || !edges || !state || !queue) {
        free_gray_image(smooth); free_gray_image(gx); free_gray_image(gy);
        free_gray_image(mag); free_gray_image(nms); free_gray_image(edges);
        free(state); free(queue); return NULL;
    }

    /* Non-maximum suppression: keep a pixel only if it is a local maximum
       along its gradient direction. */
    for (int y = 1; y < src->height - 1; ++y) {
        for (int x = 1; x < src->width - 1; ++x) {
            int p = gi(src, x, y);
            double angle = atan2(gy->data[p], gx->data[p]) * 180.0 / PI;
            if (angle < 0.0) angle += 180.0;
            double m1, m2;
            if (angle < 22.5 || angle >= 157.5) {
                m1 = mag->data[gi(src, x - 1, y)];
                m2 = mag->data[gi(src, x + 1, y)];
            } else if (angle < 67.5) {
                m1 = mag->data[gi(src, x + 1, y - 1)];
                m2 = mag->data[gi(src, x - 1, y + 1)];
            } else if (angle < 112.5) {
                m1 = mag->data[gi(src, x, y - 1)];
                m2 = mag->data[gi(src, x, y + 1)];
            } else {
                m1 = mag->data[gi(src, x - 1, y - 1)];
                m2 = mag->data[gi(src, x + 1, y + 1)];
            }
            if (mag->data[p] >= m1 && mag->data[p] >= m2)
                nms->data[p] = mag->data[p];
        }
    }

    /* Double threshold classification: 2=strong, 1=weak, 0=not an edge. */
    int head = 0, tail = 0;
    int total = src->width * src->height;
    for (int i = 0; i < total; ++i) {
        if (nms->data[i] >= high_threshold) {
            state[i] = 2;
            queue[tail++] = i;
        } else if (nms->data[i] >= low_threshold) {
            state[i] = 1;
        }
    }

    /* Hysteresis: a weak edge survives only if connected to a strong edge
       through an 8-neighborhood. The queue ensures each strong pixel is visited once. */
    while (head < tail) {
        int p = queue[head++];
        int x = p % src->width;
        int y = p / src->width;
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                int nx = x + dx, ny = y + dy;
                if (nx < 0 || nx >= src->width || ny < 0 || ny >= src->height) continue;
                int q = gi(src, nx, ny);
                if (state[q] == 1) {
                    state[q] = 2;
                    queue[tail++] = q;
                }
            }
        }
    }

    for (int i = 0; i < total; ++i) edges->data[i] = state[i] == 2 ? 255.0 : 0.0;
    free_gray_image(smooth); free_gray_image(gx); free_gray_image(gy);
    free_gray_image(mag); free_gray_image(nms); free(state); free(queue);
    return edges;
}

/* ============================ IMAGE ENHANCEMENT ============================= */

static GrayImage *unsharp_mask(const GrayImage *src, int blur_size,
                               double sigma, double amount)
{
    GrayImage *blur = gaussian_filter(src, blur_size, sigma);
    GrayImage *out = create_gray_image(src->width, src->height);
    if (!blur || !out) { free_gray_image(blur); free_gray_image(out); return NULL; }
    for (int i = 0; i < src->width * src->height; ++i) {
        /* Unsharp mask: sharpened = original + amount*(original - blurred). */
        out->data[i] = src->data[i] + amount * (src->data[i] - blur->data[i]);
        out->data[i] = fmin(255.0, fmax(0.0, out->data[i]));
    }
    free_gray_image(blur);
    return out;
}

static GrayImage *contrast_stretch(const GrayImage *src)
{
    double minv = HUGE_VAL, maxv = -HUGE_VAL;
    for (int i = 0; i < src->width * src->height; ++i) {
        if (src->data[i] < minv) minv = src->data[i];
        if (src->data[i] > maxv) maxv = src->data[i];
    }
    GrayImage *out = create_gray_image(src->width, src->height);
    if (!out) return NULL;
    if (maxv - minv < 1e-12) {
        for (int i = 0; i < src->width * src->height; ++i) out->data[i] = src->data[i];
        return out;
    }
    for (int i = 0; i < src->width * src->height; ++i)
        out->data[i] = 255.0 * (src->data[i] - minv) / (maxv - minv);
    return out;
}

static GrayImage *histogram_equalization(const GrayImage *src)
{
    unsigned long long hist[256] = {0};
    unsigned long long total = (unsigned long long)src->width * src->height;
    for (int i = 0; i < (int)total; ++i)
        hist[clamp_int((int)lround(src->data[i]), 0, 255)]++;

    unsigned long long cdf[256] = {0};
    cdf[0] = hist[0];
    for (int i = 1; i < 256; ++i) cdf[i] = cdf[i - 1] + hist[i];
    unsigned long long cdf_min = 0;
    for (int i = 0; i < 256; ++i) if (cdf[i] != 0) { cdf_min = cdf[i]; break; }

    GrayImage *out = create_gray_image(src->width, src->height);
    if (!out) return NULL;
    if (total == cdf_min) {
        for (int i = 0; i < (int)total; ++i) out->data[i] = src->data[i];
        return out;
    }

    /* Mapping uses the normalized cumulative distribution function (CDF). */
    double map[256];
    for (int i = 0; i < 256; ++i)
        map[i] = 255.0 * ((double)cdf[i] - cdf_min) / ((double)total - cdf_min);
    for (int i = 0; i < (int)total; ++i) {
        int v = clamp_int((int)lround(src->data[i]), 0, 255);
        out->data[i] = map[v];
    }
    return out;
}

static GrayImage *gamma_correction(const GrayImage *src, double gamma)
{
    if (gamma <= 0.0) return NULL;
    GrayImage *out = create_gray_image(src->width, src->height);
    if (!out) return NULL;
    for (int i = 0; i < src->width * src->height; ++i) {
        double normalized = fmin(1.0, fmax(0.0, src->data[i] / 255.0));
        out->data[i] = 255.0 * pow(normalized, gamma);
    }
    return out;
}

/* =========================== MORPHOLOGICAL OPS ============================== */

static GrayImage *morphology_primitive(const GrayImage *src, int ksize, int dilate)
{
    if (!valid_odd_kernel(ksize)) return NULL;
    GrayImage *out = create_gray_image(src->width, src->height);
    if (!out) return NULL;
    int radius = ksize / 2;
    for (int y = 0; y < src->height; ++y) {
        for (int x = 0; x < src->width; ++x) {
            int result = dilate ? 0 : 1;
            for (int dy = -radius; dy <= radius; ++dy) {
                for (int dx = -radius; dx <= radius; ++dx) {
                    int nx = x + dx, ny = y + dy;
                    /* Outside is black: it can prevent erosion but not add dilation pixels. */
                    int white = (nx >= 0 && nx < src->width && ny >= 0 && ny < src->height &&
                                 src->data[gi(src, nx, ny)] >= 127.5);
                    if (dilate && white) result = 1;
                    if (!dilate && !white) result = 0;
                }
            }
            out->data[gi(out, x, y)] = result ? 255.0 : 0.0;
        }
    }
    return out;
}

static GrayImage *morphological_erosion(const GrayImage *src, int ksize)
{
    return morphology_primitive(src, ksize, 0);
}

static GrayImage *morphological_dilation(const GrayImage *src, int ksize)
{
    return morphology_primitive(src, ksize, 1);
}

static GrayImage *morphological_opening(const GrayImage *src, int ksize)
{
    GrayImage *eroded = morphological_erosion(src, ksize);
    if (!eroded) return NULL;
    GrayImage *out = morphological_dilation(eroded, ksize);
    free_gray_image(eroded);
    return out;
}

static GrayImage *morphological_closing(const GrayImage *src, int ksize)
{
    GrayImage *dilated = morphological_dilation(src, ksize);
    if (!dilated) return NULL;
    GrayImage *out = morphological_erosion(dilated, ksize);
    free_gray_image(dilated);
    return out;
}

static GrayImage *morphological_gradient(const GrayImage *src, int ksize)
{
    GrayImage *dilated = morphological_dilation(src, ksize);
    GrayImage *eroded = morphological_erosion(src, ksize);
    GrayImage *out = create_gray_image(src->width, src->height);
    if (!dilated || !eroded || !out) {
        free_gray_image(dilated); free_gray_image(eroded); free_gray_image(out); return NULL;
    }
    for (int i = 0; i < src->width * src->height; ++i)
        out->data[i] = dilated->data[i] - eroded->data[i];
    free_gray_image(dilated); free_gray_image(eroded);
    return out;
}

static GrayImage *morphological_boundary(const GrayImage *src, int ksize)
{
    GrayImage *eroded = morphological_erosion(src, ksize);
    GrayImage *out = create_gray_image(src->width, src->height);
    if (!eroded || !out) { free_gray_image(eroded); free_gray_image(out); return NULL; }
    /* Boundary of a binary foreground object: A - erosion(A). */
    for (int i = 0; i < src->width * src->height; ++i)
        out->data[i] = (src->data[i] >= 127.5 && eroded->data[i] < 127.5) ? 255.0 : 0.0;
    free_gray_image(eroded);
    return out;
}

/* =========================== FOURIER / FREQUENCY OPS ======================== */

typedef struct {
    double re;
    double im;
} ComplexNumber;

static int next_power_of_two(int value)
{
    if (value <= 0 || value > 2048) return 0;
    int n = 1;
    while (n < value && n <= 2048) n <<= 1;
    return n >= value ? n : 0;
}

static void fft_1d(ComplexNumber *a, int n, int inverse)
{
    /* Iterative radix-2 Cooley-Tukey FFT. n must be a power of two.
       The 2D transform computes the separable form of
       F(u,v)=sum_y sum_x I(x,y) exp[-j*2*pi*(u*x/W + v*y/H)]. */
    int j = 0;
    for (int i = 1; i < n; ++i) {
        int bit = n >> 1;
        while (j & bit) { j ^= bit; bit >>= 1; }
        j ^= bit;
        if (i < j) {
            ComplexNumber temp = a[i]; a[i] = a[j]; a[j] = temp;
        }
    }

    for (int length = 2; length <= n; length <<= 1) {
        double angle = (inverse ? 2.0 : -2.0) * PI / length;
        ComplexNumber root = {cos(angle), sin(angle)};
        for (int start = 0; start < n; start += length) {
            ComplexNumber w = {1.0, 0.0};
            for (int k = 0; k < length / 2; ++k) {
                ComplexNumber u = a[start + k];
                ComplexNumber v0 = a[start + k + length / 2];
                ComplexNumber v = {v0.re * w.re - v0.im * w.im,
                                   v0.re * w.im + v0.im * w.re};
                a[start + k].re = u.re + v.re;
                a[start + k].im = u.im + v.im;
                a[start + k + length / 2].re = u.re - v.re;
                a[start + k + length / 2].im = u.im - v.im;
                ComplexNumber next_w = {w.re * root.re - w.im * root.im,
                                        w.re * root.im + w.im * root.re};
                w = next_w;
            }
        }
    }

    if (inverse) {
        for (int i = 0; i < n; ++i) { a[i].re /= n; a[i].im /= n; }
    }
}

static int fft_2d(ComplexNumber *data, int width, int height, int inverse)
{
    int maximum = width > height ? width : height;
    ComplexNumber *line = (ComplexNumber *)malloc((size_t)maximum * sizeof(ComplexNumber));
    if (!line) return 0;

    /* Transform each row. */
    for (int y = 0; y < height; ++y)
        fft_1d(&data[y * width], width, inverse);

    /* Gather each column, transform it, and put it back. */
    for (int x = 0; x < width; ++x) {
        for (int y = 0; y < height; ++y) line[y] = data[y * width + x];
        fft_1d(line, height, inverse);
        for (int y = 0; y < height; ++y) data[y * width + x] = line[y];
    }

    free(line);
    return 1;
}

static ComplexNumber *image_fft(const GrayImage *src, int *fft_width, int *fft_height)
{
    int w = next_power_of_two(src->width);
    int h = next_power_of_two(src->height);
    if (w == 0 || h == 0) {
        printf("FFT supports images up to 2048 x 2048 in this educational implementation. Resize the image first.\n");
        return NULL;
    }
    size_t count = (size_t)w * h;
    ComplexNumber *spectrum = (ComplexNumber *)calloc(count, sizeof(ComplexNumber));
    if (!spectrum) return NULL;

    /* Zero-pad image to power-of-two dimensions for radix-2 FFT. */
    for (int y = 0; y < src->height; ++y)
        for (int x = 0; x < src->width; ++x)
            spectrum[y * w + x].re = src->data[gi(src, x, y)];

    if (!fft_2d(spectrum, w, h, 0)) { free(spectrum); return NULL; }
    *fft_width = w;
    *fft_height = h;
    return spectrum;
}

static GrayImage *fft_magnitude_spectrum(const GrayImage *src)
{
    int w, h;
    ComplexNumber *f = image_fft(src, &w, &h);
    if (!f) return NULL;
    GrayImage *out = create_gray_image(w, h);
    if (!out) { free(f); return NULL; }

    double maximum = 0.0;
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
        int sx = (x + w / 2) % w;
        int sy = (y + h / 2) % h;
        ComplexNumber z = f[sy * w + sx];
        double value = log(1.0 + hypot(z.re, z.im));
        out->data[gi(out, x, y)] = value;
        if (value > maximum) maximum = value;
    }
    if (maximum > 0.0)
        for (int i = 0; i < w * h; ++i) out->data[i] = 255.0 * out->data[i] / maximum;
    free(f);
    return out;
}

static GrayImage *fft_phase_spectrum(const GrayImage *src)
{
    int w, h;
    ComplexNumber *f = image_fft(src, &w, &h);
    if (!f) return NULL;
    GrayImage *out = create_gray_image(w, h);
    if (!out) { free(f); return NULL; }
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
        int sx = (x + w / 2) % w;
        int sy = (y + h / 2) % h;
        ComplexNumber z = f[sy * w + sx];
        double phase = atan2(z.im, z.re);
        out->data[gi(out, x, y)] = (phase + PI) * 255.0 / (2.0 * PI);
    }
    free(f);
    return out;
}

typedef enum {
    FREQ_IDEAL_LOW = 0,
    FREQ_IDEAL_HIGH = 1,
    FREQ_GAUSSIAN_LOW = 2,
    FREQ_GAUSSIAN_HIGH = 3
} FrequencyFilterType;

static GrayImage *frequency_domain_filter(const GrayImage *src, double cutoff,
                                          FrequencyFilterType type)
{
    if (cutoff <= 0.0) return NULL;
    int w, h;
    ComplexNumber *f = image_fft(src, &w, &h);
    if (!f) return NULL;

    /* The unshifted FFT has DC at (0,0); use wrapped frequency coordinates. */
    for (int y = 0; y < h; ++y) {
        int fy = y <= h / 2 ? y : y - h;
        for (int x = 0; x < w; ++x) {
            int fx = x <= w / 2 ? x : x - w;
            double distance = hypot((double)fx, (double)fy);
            double low_mask;
            if (type == FREQ_IDEAL_LOW || type == FREQ_IDEAL_HIGH)
                low_mask = distance <= cutoff ? 1.0 : 0.0;
            else
                low_mask = exp(-(distance * distance) / (2.0 * cutoff * cutoff));
            double mask = (type == FREQ_IDEAL_HIGH || type == FREQ_GAUSSIAN_HIGH)
                        ? 1.0 - low_mask : low_mask;
            int p = y * w + x;
            f[p].re *= mask;
            f[p].im *= mask;
        }
    }

    if (!fft_2d(f, w, h, 1)) { free(f); return NULL; }
    GrayImage *out = create_gray_image(src->width, src->height);
    if (!out) { free(f); return NULL; }
    for (int y = 0; y < src->height; ++y)
        for (int x = 0; x < src->width; ++x)
            out->data[gi(out, x, y)] = f[y * w + x].re;
    free(f);
    return out;
}

/* =========================== GENERAL USER INTERFACE ========================= */

static int read_int_prompt(const char *prompt, int *value)
{
    printf("%s", prompt);
    return scanf("%d", value) == 1;
}

static int read_double_prompt(const char *prompt, double *value)
{
    printf("%s", prompt);
    return scanf("%lf", value) == 1;
}

static void save_and_free_result(GrayImage *result, const char *name, int signed_mode)
{
    if (!result) { printf("Operation failed or invalid parameters.\n"); return; }
    save_gray(result, name, signed_mode);
    free_gray_image(result);
}

static void show_menu(void)
{
    printf("\n================ IMAGE PROCESSING LAB ================\n");
    printf(" 1  Grayscale conversion\n");
    printf("--- Smoothing / filtering ---\n");
    printf(" 2  Gaussian filter\n");
    printf(" 3  Mean (averaging) filter\n");
    printf(" 4  Median filter\n");
    printf(" 5  Bilateral filter\n");
    printf("--- Derivatives / edge information ---\n");
    printf(" 6  First derivative X\n");
    printf(" 7  First derivative Y\n");
    printf(" 8  Second derivative X\n");
    printf(" 9  Second derivative Y\n");
    printf("10  Laplacian (4-neighbor)\n");
    printf("11  Laplacian (8-neighbor)\n");
    printf("12  Laplacian of Gaussian (LoG)\n");
    printf("13  Sobel horizontal gradient Gx\n");
    printf("14  Sobel vertical gradient Gy\n");
    printf("15  Gradient magnitude\n");
    printf("16  Gradient direction theta\n");
    printf("17  Gradient direction vectors (CSV)\n");
    printf("18  Gradient direction arrow overlay (PPM)\n");
    printf("19  Prewitt horizontal derivative\n");
    printf("20  Prewitt vertical derivative\n");
    printf("21  Roberts diagonal derivative X\n");
    printf("22  Roberts diagonal derivative Y\n");
    printf("23  Canny edge detector\n");
    printf("--- Thresholding ---\n");
    printf("24  Binary threshold\n");
    printf("25  Inverse binary threshold\n");
    printf("26  Adaptive mean threshold\n");
    printf("27  Otsu automatic threshold\n");
    printf("28  Multilevel Otsu threshold\n");
    printf("--- Image enhancement ---\n");
    printf("29  Unsharp-mask sharpening\n");
    printf("30  Contrast stretching\n");
    printf("31  Histogram equalization\n");
    printf("32  Gamma correction\n");
    printf("--- Morphological operations (binary images) ---\n");
    printf("33  Erosion\n");
    printf("34  Dilation\n");
    printf("35  Opening\n");
    printf("36  Closing\n");
    printf("37  Morphological gradient\n");
    printf("38  Boundary extraction\n");
    printf("--- Geometric transformations ---\n");
    printf("39  Translation / shifting\n");
    printf("40  Rotation (expanded canvas)\n");
    printf("41  Scaling (resize)\n");
    printf("42  Zoom in / out (fixed canvas)\n");
    printf("43  Shear / skew\n");
    printf("44  Bulge (radial warp)\n");
    printf("45  Squeeze (radial warp)\n");
    printf("46  Perspective warp (4 destination corners)\n");
    printf("47  Reflection horizontal\n");
    printf("48  Reflection vertical\n");
    printf("49  Padding (constant / replicate / reflect)\n");
    printf("--- Frequency domain (2D FFT) ---\n");
    printf("50  FFT log-magnitude spectrum\n");
    printf("51  FFT phase spectrum\n");
    printf("52  Ideal low-pass frequency filter\n");
    printf("53  Ideal high-pass frequency filter\n");
    printf("54  Gaussian low-pass frequency filter\n");
    printf("55  Gaussian high-pass frequency filter\n");
    printf(" 0  Exit\n");
    printf("=======================================================\n");
}

int main(int argc, char **argv)
{
    char filename[MAX_PATH_LEN];
    if (argc >= 2) {
        snprintf(filename, sizeof(filename), "%s", argv[1]);
    } else {
        printf("Enter PPM image path: ");
        if (!fgets(filename, sizeof(filename), stdin)) return 1;
        filename[strcspn(filename, "\r\n")] = '\0';
    }

    ColorImage *original_color = load_ppm(filename);
    if (!original_color) {
        fprintf(stderr, "Could not load input image.\n");
        return 1;
    }
    GrayImage *original = convert_to_grayscale(original_color);
    if (!original) { free_color_image(original_color); return 1; }

    printf("\nImage loaded successfully: %s\n", filename);
    printf("Dimensions: %d x %d pixels (RGB input; grayscale processing).\n",
           original_color->width, original_color->height);
    printf("Each operation starts from the original image.\n");

    int choice = -1;
    while (choice != 0) {
        show_menu();
        if (!read_int_prompt("Choose operation: ", &choice)) {
            printf("Invalid input. Exiting.\n");
            break;
        }

        int ksize, classes, spacing, pad, mode;
        double sigma, value, value2, C;
        GrayImage *result = NULL;

        switch (choice) {
            case 1:
                /* Save the luminance conversion, not the original RGB image. */
                save_gray(original, "output_grayscale.ppm", 0);
                break;
            case 2:
                if (read_int_prompt("Odd Gaussian kernel size (e.g. 3, 5, 7): ", &ksize) &&
                    read_double_prompt("Sigma (e.g. 1.0): ", &sigma))
                    save_and_free_result(gaussian_filter(original, ksize, sigma), "output_gaussian.ppm", 0);
                break;
            case 3:
                if (read_int_prompt("Odd mean-filter kernel size: ", &ksize))
                    save_and_free_result(mean_filter(original, ksize), "output_mean.ppm", 0);
                break;
            case 4:
                if (read_int_prompt("Odd median-filter kernel size: ", &ksize))
                    save_and_free_result(median_filter(original, ksize), "output_median.ppm", 0);
                break;
            case 5:
                if (read_int_prompt("Odd bilateral kernel size: ", &ksize) &&
                    read_double_prompt("Spatial sigma (sigma_s, e.g. 3): ", &sigma) &&
                    read_double_prompt("Intensity sigma (sigma_r, e.g. 25): ", &value))
                    save_and_free_result(bilateral_filter(original, ksize, sigma, value), "output_bilateral.ppm", 0);
                break;
            case 6: save_and_free_result(first_derivative_x(original), "output_first_dx.ppm", 1); break;
            case 7: save_and_free_result(first_derivative_y(original), "output_first_dy.ppm", 1); break;
            case 8: save_and_free_result(second_derivative_x(original), "output_second_dxx.ppm", 1); break;
            case 9: save_and_free_result(second_derivative_y(original), "output_second_dyy.ppm", 1); break;
            case 10: save_and_free_result(laplacian_filter(original, 0), "output_laplacian_4.ppm", 1); break;
            case 11: save_and_free_result(laplacian_filter(original, 1), "output_laplacian_8.ppm", 1); break;
            case 12:
                if (read_int_prompt("Odd Gaussian kernel size: ", &ksize) &&
                    read_double_prompt("Sigma: ", &sigma))
                    save_and_free_result(laplacian_of_gaussian(original, ksize, sigma), "output_log.ppm", 1);
                break;
            case 13: save_and_free_result(sobel_x(original), "output_sobel_gx.ppm", 1); break;
            case 14: save_and_free_result(sobel_y(original), "output_sobel_gy.ppm", 1); break;
            case 15: save_and_free_result(gradient_magnitude(original, NULL, NULL), "output_gradient_magnitude.ppm", 2); break;
            case 16: save_and_free_result(gradient_direction_theta(original), "output_gradient_theta.ppm", 0); break;
            case 17:
                if (read_int_prompt("Vector sampling spacing in pixels (e.g. 8): ", &spacing))
                    save_gradient_vectors_csv(original, "output_gradient_vectors.csv", spacing);
                break;
            case 18:
                if (read_int_prompt("Arrow spacing in pixels (e.g. 12): ", &spacing) &&
                    read_double_prompt("Arrow display length in pixels (e.g. 6): ", &value))
                    save_gradient_vector_overlay(original, "output_gradient_vectors.ppm", spacing, value);
                break;
            case 19: save_and_free_result(prewitt_x(original), "output_prewitt_gx.ppm", 1); break;
            case 20: save_and_free_result(prewitt_y(original), "output_prewitt_gy.ppm", 1); break;
            case 21: save_and_free_result(roberts_x(original), "output_roberts_gx.ppm", 1); break;
            case 22: save_and_free_result(roberts_y(original), "output_roberts_gy.ppm", 1); break;
            case 23:
                if (read_int_prompt("Gaussian blur kernel size (odd, e.g. 5): ", &ksize) &&
                    read_double_prompt("Gaussian sigma (e.g. 1.4): ", &sigma) &&
                    read_double_prompt("Low edge threshold: ", &value) &&
                    read_double_prompt("High edge threshold (larger than low): ", &value2))
                    save_and_free_result(canny_edge_detector(original, ksize, sigma, value, value2),
                                         "output_canny.ppm", 0);
                break;
            case 24:
                if (read_double_prompt("Threshold (0..255): ", &value))
                    save_and_free_result(binary_threshold(original, value), "output_binary_threshold.ppm", 0);
                break;
            case 25:
                if (read_double_prompt("Threshold (0..255): ", &value))
                    save_and_free_result(inverse_binary_threshold(original, value), "output_inverse_threshold.ppm", 0);
                break;
            case 26:
                if (read_int_prompt("Odd local block size (e.g. 15): ", &ksize) &&
                    read_double_prompt("C offset (e.g. 5): ", &C))
                    save_and_free_result(adaptive_mean_threshold(original, ksize, C), "output_adaptive_mean.ppm", 0);
                break;
            case 27: {
                int used = 0;
                result = otsu_threshold(original, &used);
                printf("Otsu selected threshold = %d\n", used);
                save_and_free_result(result, "output_otsu.ppm", 0);
                break;
            }
            case 28:
                if (read_int_prompt("Number of classes (2..8): ", &classes))
                    save_and_free_result(multilevel_otsu_threshold(original, classes), "output_multilevel_otsu.ppm", 0);
                break;
            case 29:
                if (read_int_prompt("Odd Gaussian blur kernel size: ", &ksize) &&
                    read_double_prompt("Gaussian sigma: ", &sigma) &&
                    read_double_prompt("Sharpening amount (e.g. 1.0): ", &value))
                    save_and_free_result(unsharp_mask(original, ksize, sigma, value), "output_sharpened.ppm", 0);
                break;
            case 30: save_and_free_result(contrast_stretch(original), "output_contrast_stretch.ppm", 0); break;
            case 31: save_and_free_result(histogram_equalization(original), "output_histogram_equalization.ppm", 0); break;
            case 32:
                if (read_double_prompt("Gamma value (>0; <1 brightens, >1 darkens): ", &value))
                    save_and_free_result(gamma_correction(original, value), "output_gamma.ppm", 0);
                break;
            case 33:
            case 34:
            case 35:
            case 36:
            case 37:
            case 38:
                if (read_int_prompt("Odd structuring-element size (e.g. 3): ", &ksize)) {
                    if (choice == 33) result = morphological_erosion(original, ksize);
                    if (choice == 34) result = morphological_dilation(original, ksize);
                    if (choice == 35) result = morphological_opening(original, ksize);
                    if (choice == 36) result = morphological_closing(original, ksize);
                    if (choice == 37) result = morphological_gradient(original, ksize);
                    if (choice == 38) result = morphological_boundary(original, ksize);
                    const char *output_name = NULL;
                    if (choice == 33) output_name = "output_erosion.ppm";
                    if (choice == 34) output_name = "output_dilation.ppm";
                    if (choice == 35) output_name = "output_opening.ppm";
                    if (choice == 36) output_name = "output_closing.ppm";
                    if (choice == 37) output_name = "output_morph_gradient.ppm";
                    if (choice == 38) output_name = "output_boundary.ppm";
                    save_and_free_result(result, output_name, 0);
                }
                break;
            case 39:
                if (read_double_prompt("Horizontal shift dx (pixels, + right): ", &value) &&
                    read_double_prompt("Vertical shift dy (pixels, + down): ", &value2))
                    save_and_free_result(translate_image(original, value, value2), "output_translation.ppm", 0);
                break;
            case 40:
                if (read_double_prompt("Rotation angle in degrees (+ counter-clockwise): ", &value))
                    save_and_free_result(rotate_image(original, value), "output_rotation.ppm", 0);
                break;
            case 41:
                if (read_double_prompt("Horizontal scale factor (e.g. 1.5): ", &value) &&
                    read_double_prompt("Vertical scale factor (e.g. 1.5): ", &value2))
                    save_and_free_result(scale_image(original, value, value2), "output_scaling.ppm", 0);
                break;
            case 42:
                if (read_double_prompt("Zoom factor (>1 zoom in, between 0 and 1 zoom out): ", &value))
                    save_and_free_result(zoom_image(original, value), "output_zoom.ppm", 0);
                break;
            case 43:
                if (read_double_prompt("Horizontal shear shx (e.g. 0.3): ", &value) &&
                    read_double_prompt("Vertical shear shy (e.g. 0): ", &value2))
                    save_and_free_result(shear_image(original, value, value2), "output_shear.ppm", 0);
                break;
            case 44:
            case 45:
                if (read_double_prompt("Warp strength (0..0.9, e.g. 0.5): ", &value))
                    save_and_free_result(radial_warp(original, value, choice == 44),
                                         choice == 44 ? "output_bulge.ppm" : "output_squeeze.ppm", 0);
                break;
            case 46: {
                double dx[4], dy[4];
                const char *labels[4] = {"Top-left", "Top-right", "Bottom-right", "Bottom-left"};
                int ok = 1;
                printf("Enter destination corner coordinates in the output canvas (0-based pixels).\n");
                printf("Corner order: top-left, top-right, bottom-right, bottom-left.\n");
                for (int i = 0; i < 4 && ok; ++i) {
                    char prompt[128];
                    snprintf(prompt, sizeof(prompt), "%s destination x: ", labels[i]);
                    ok = read_double_prompt(prompt, &dx[i]);
                    snprintf(prompt, sizeof(prompt), "%s destination y: ", labels[i]);
                    if (ok) ok = read_double_prompt(prompt, &dy[i]);
                }
                if (ok) save_and_free_result(perspective_warp(original, dx, dy), "output_perspective.ppm", 0);
                break;
            }
            case 47: save_and_free_result(reflect_image(original, 1), "output_reflect_horizontal.ppm", 0); break;
            case 48: save_and_free_result(reflect_image(original, 0), "output_reflect_vertical.ppm", 0); break;
            case 49:
                if (read_int_prompt("Padding width in pixels: ", &pad) &&
                    read_int_prompt("Mode (0=constant, 1=replicate, 2=reflect): ", &mode)) {
                    double constant = 0.0;
                    if (mode == 0) read_double_prompt("Constant pixel value (0..255): ", &constant);
                    if (mode < 0 || mode > 2) printf("Invalid padding mode.\n");
                    else save_and_free_result(pad_image(original, pad, mode, constant), "output_padded.ppm", 0);
                }
                break;
            case 50: save_and_free_result(fft_magnitude_spectrum(original), "output_fft_magnitude.ppm", 0); break;
            case 51: save_and_free_result(fft_phase_spectrum(original), "output_fft_phase.ppm", 0); break;
            case 52:
            case 53:
            case 54:
            case 55:
                if (read_double_prompt("Frequency cut-off radius in pixels (e.g. 20): ", &value)) {
                    FrequencyFilterType type = FREQ_IDEAL_LOW;
                    if (choice == 53) type = FREQ_IDEAL_HIGH;
                    if (choice == 54) type = FREQ_GAUSSIAN_LOW;
                    if (choice == 55) type = FREQ_GAUSSIAN_HIGH;
                    result = frequency_domain_filter(original, value, type);
                    const char *output_name = choice == 52 ? "output_fft_ideal_lowpass.ppm" :
                                              choice == 53 ? "output_fft_ideal_highpass.ppm" :
                                              choice == 54 ? "output_fft_gaussian_lowpass.ppm" :
                                                             "output_fft_gaussian_highpass.ppm";
                    /* High-pass reconstruction can be signed, so visualize around middle gray. */
                    save_and_free_result(result, output_name,
                                         (choice == 53 || choice == 55) ? 1 : 0);
                }
                break;
            case 0: printf("Exiting.\n"); break;
            default: printf("Unknown menu option.\n"); break;
        }
    }

    free_gray_image(original);
    free_color_image(original_color);
    return 0;
}
