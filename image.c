#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<math.h>
//
pragma pack (push,1)

typedef struct {
    uint16_t file_type;     // Always 'BM'
    uint32_t file_size;     // Size of file
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t offset_data;   // Start of pixel data
} BMPFileHeader;

typedef struct {
    uint32_t size;          // Header size
    int32_t width;          // Image width
    int32_t height;         // Image height
    uint16_t planes;        // Must be 1
    uint16_t bit_count;     // 24 for RGB
    uint32_t compression;   // 0 for uncompressed
    uint32_t size_image;
    int32_t x_pixels_per_m;
    int32_t y_pixels_per_m;
    uint32_t colors_used;
    uint32_t colors_important;
} BMPInfoHeader;
