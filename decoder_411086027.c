#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct _bmp {
    int Hpixels;
    int Vpixels;
    unsigned char HeaderInfo[54];
    unsigned long int Hbytes;
    unsigned char **data;
    unsigned char **R;
    unsigned char **G;
    unsigned char **B;
} bmp;

// 讀取維度資訊
int read_dimensions(char *filename, int *width, int *height) {
    FILE *f = fopen(filename, "r");
    if (f == NULL) {
        printf("Cannot open dimension file: %s\n", filename);
        return 0;
    }
    
    if (fscanf(f, "%d %d", width, height) != 2) {
        printf("Error reading dimensions\n");
        fclose(f);
        return 0;
    }
    
    fclose(f);
    return 1;
}

// 讀取通道數據
int read_channel(char *filename, unsigned char **channel, int width, int height) {
    FILE *f = fopen(filename, "r");
    if (f == NULL) {
        printf("Cannot open channel file: %s\n", filename);
        return 0;
    }
    
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            int value;
            if (fscanf(f, "%d", &value) != 1) {
                printf("Error reading channel data at position (%d,%d)\n", i, j);
                fclose(f);
                return 0;
            }
            channel[i][j] = (unsigned char)value;
        }
    }
    
    fclose(f);
    return 1;
}

int initialize_bmp(bmp *p_bmp, int width, int height) {
    p_bmp->Hpixels = width;
    p_bmp->Vpixels = height;
    p_bmp->Hbytes = ((width * 3 + 3) & (~3));

    // 清空Header
    memset(p_bmp->HeaderInfo, 0, 54);
    
    // BMP檔案標識（'BM'）
    p_bmp->HeaderInfo[0] = 'B';
    p_bmp->HeaderInfo[1] = 'M';
    
    // 檔案大小（54位元組的Header + 圖像數據）
    int fileSize = 54 + p_bmp->Hbytes * height;
    memcpy(&p_bmp->HeaderInfo[2], &fileSize, 4);
    
    // 保留欄位（設為0）
    p_bmp->HeaderInfo[6] = 0;
    p_bmp->HeaderInfo[7] = 0;
    p_bmp->HeaderInfo[8] = 0;
    p_bmp->HeaderInfo[9] = 0;
    
    // 數據偏移量（54位元組）
    p_bmp->HeaderInfo[10] = 54;
    p_bmp->HeaderInfo[11] = 0;
    p_bmp->HeaderInfo[12] = 0;
    p_bmp->HeaderInfo[13] = 0;
    
    // BMP Header大小（40位元組）
    p_bmp->HeaderInfo[14] = 40;
    p_bmp->HeaderInfo[15] = 0;
    p_bmp->HeaderInfo[16] = 0;
    p_bmp->HeaderInfo[17] = 0;
    
    // 圖像寬度
    memcpy(&p_bmp->HeaderInfo[18], &width, 4);
    
    // 圖像高度
    memcpy(&p_bmp->HeaderInfo[22], &height, 4);
    
    // 色彩平面數（1）
    p_bmp->HeaderInfo[26] = 1;
    p_bmp->HeaderInfo[27] = 0;
    
    // 每個像素位元數（24）
    p_bmp->HeaderInfo[28] = 24;
    p_bmp->HeaderInfo[29] = 0;
    
    // 壓縮方式（0 = 不壓縮）
    p_bmp->HeaderInfo[30] = 0;
    p_bmp->HeaderInfo[31] = 0;
    p_bmp->HeaderInfo[32] = 0;
    p_bmp->HeaderInfo[33] = 0;
    
    // 圖像數據大小
    int imageSize = p_bmp->Hbytes * height;
    memcpy(&p_bmp->HeaderInfo[34], &imageSize, 4);
    
    // 水平解析度（2835 pixels/meter = 72 DPI）
    int resolution = 2835;
    memcpy(&p_bmp->HeaderInfo[38], &resolution, 4);
    
    // 垂直解析度（2835 pixels/meter = 72 DPI）
    memcpy(&p_bmp->HeaderInfo[42], &resolution, 4);
    
    // 調色盤顏色數（0 = 使用最大可能值）
    memset(&p_bmp->HeaderInfo[46], 0, 4);
    
    // 重要顏色數（0 = 所有顏色都重要）
    memset(&p_bmp->HeaderInfo[50], 0, 4);

    // 分配記憶體（其餘代碼保持不變）
    p_bmp->data = (unsigned char**)malloc(height * sizeof(unsigned char*));
    p_bmp->R = (unsigned char**)malloc(height * sizeof(unsigned char*));
    p_bmp->G = (unsigned char**)malloc(height * sizeof(unsigned char*));
    p_bmp->B = (unsigned char**)malloc(height * sizeof(unsigned char*));

    if (!p_bmp->data || !p_bmp->R || !p_bmp->G || !p_bmp->B) {
        printf("Memory allocation failed\n");
        return 0;
    }

    for (int i = 0; i < height; i++) {
        p_bmp->data[i] = (unsigned char*)malloc(p_bmp->Hbytes);
        p_bmp->R[i] = (unsigned char*)malloc(width);
        p_bmp->G[i] = (unsigned char*)malloc(width);
        p_bmp->B[i] = (unsigned char*)malloc(width);

        if (!p_bmp->data[i] || !p_bmp->R[i] || !p_bmp->G[i] || !p_bmp->B[i]) {
            printf("Memory allocation failed for row %d\n", i);
            return 0;
        }
        
        // 初始化數據為0
        memset(p_bmp->data[i], 0, p_bmp->Hbytes);
        memset(p_bmp->R[i], 0, width);
        memset(p_bmp->G[i], 0, width);
        memset(p_bmp->B[i], 0, width);
    }

    return 1;
}

// 保存BMP檔案
int save_bmp(char *filename, bmp *p_bmp) {
    FILE *f = fopen(filename, "wb");
    if (f == NULL) {
        printf("Cannot create output file: %s\n", filename);
        return 0;
    }

    // 寫入檔頭
    fwrite(p_bmp->HeaderInfo, sizeof(unsigned char), 54, f);

    // 組合RGB通道並寫入
    for (int i = 0; i < p_bmp->Vpixels; i++) {
        for (int j = 0; j < p_bmp->Hpixels; j++) {
            p_bmp->data[i][j * 3] = p_bmp->B[i][j];     // Blue
            p_bmp->data[i][j * 3 + 1] = p_bmp->G[i][j]; // Green
            p_bmp->data[i][j * 3 + 2] = p_bmp->R[i][j]; // Red
        }
        // 寫入一行數據（包含padding）
        fwrite(p_bmp->data[i], sizeof(unsigned char), p_bmp->Hbytes, f);
    }

    fclose(f);
    return 1;
}

// 釋放記憶體
void bmp_free(bmp *p_bmp) {
    if (p_bmp == NULL) return;
    
    for (int i = 0; i < p_bmp->Vpixels; i++) {
        if (p_bmp->data && p_bmp->data[i]) free(p_bmp->data[i]);
        if (p_bmp->R && p_bmp->R[i]) free(p_bmp->R[i]);
        if (p_bmp->G && p_bmp->G[i]) free(p_bmp->G[i]);
        if (p_bmp->B && p_bmp->B[i]) free(p_bmp->B[i]);
    }
    
    if (p_bmp->data) free(p_bmp->data);
    if (p_bmp->R) free(p_bmp->R);
    if (p_bmp->G) free(p_bmp->G);
    if (p_bmp->B) free(p_bmp->B);
}

int main(int argc, char **argv) {
    if (argc != 7) {
        return 1;
    }

    int mode = atoi(argv[1]);
    if (mode != 0) {
        fprintf(stderr, "Unsupported mode: %d\n", mode);
        return 1;
    }

    // 讀取圖片尺寸
    int width, height;
    if (!read_dimensions(argv[6], &width, &height)) {
        return 1;
    }

    // 初始化BMP結構
    bmp image = {0};
    if (!initialize_bmp(&image, width, height)) {
        return 1;
    }

    // 讀取RGB通道
    if (!read_channel(argv[3], image.R, width, height) ||
        !read_channel(argv[4], image.G, width, height) ||
        !read_channel(argv[5], image.B, width, height)) {
        bmp_free(&image);
        return 1;
    }

    // 保存BMP檔案
    if (!save_bmp(argv[2], &image)) {
        bmp_free(&image);
        return 1;
    }

    bmp_free(&image);
    return 0;
}