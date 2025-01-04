#include <stdio.h>
#include <stdlib.h>

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

// 讀取BMP檔案
int bmp_load_fn(char *filename, bmp *p_bmp) {
    FILE *f = fopen(filename, "rb");  // 使用二進制讀取模式
    if (f == NULL) {
        printf("\n\n%s NOT FOUND\n\n", filename);
        return 0;
    }

    // 讀取BMP Header
    size_t bytesRead = fread(p_bmp->HeaderInfo, sizeof(unsigned char), 54, f);
    if (bytesRead != 54) {
        printf("Error reading header\n");
        fclose(f);
        return 0;
    }

    // 檢查BMP檔案標識（'BM'）
    if (p_bmp->HeaderInfo[0] != 'B' || p_bmp->HeaderInfo[1] != 'M') {
        printf("Not a valid BMP file\n");
        fclose(f);
        return 0;
    }

    // 取得圖片寬高
    p_bmp->Hpixels = *(int*)&p_bmp->HeaderInfo[18];
    p_bmp->Vpixels = *(int*)&p_bmp->HeaderInfo[22];
    
    // 計算每行字節數
    p_bmp->Hbytes = ((p_bmp->Hpixels * 3 + 3) & (~3));

    // 分配記憶體
    p_bmp->data = (unsigned char**)malloc(p_bmp->Vpixels * sizeof(unsigned char*));
    p_bmp->R = (unsigned char**)malloc(p_bmp->Vpixels * sizeof(unsigned char*));
    p_bmp->G = (unsigned char**)malloc(p_bmp->Vpixels * sizeof(unsigned char*));
    p_bmp->B = (unsigned char**)malloc(p_bmp->Vpixels * sizeof(unsigned char*));

    if (!p_bmp->data || !p_bmp->R || !p_bmp->G || !p_bmp->B) {
        printf("Memory allocation failed\n");
        fclose(f);
        return 0;
    }

    // 讀取圖片數據
    for (int i = 0; i < p_bmp->Vpixels; i++) {
        p_bmp->data[i] = (unsigned char*)malloc(p_bmp->Hbytes);
        p_bmp->R[i] = (unsigned char*)malloc(p_bmp->Hpixels);
        p_bmp->G[i] = (unsigned char*)malloc(p_bmp->Hpixels);
        p_bmp->B[i] = (unsigned char*)malloc(p_bmp->Hpixels);

        if (!p_bmp->data[i] || !p_bmp->R[i] || !p_bmp->G[i] || !p_bmp->B[i]) {
            printf("Memory allocation failed for row %d\n", i);
            fclose(f);
            return 0;
        }

        // 讀取一行數據
        if (fread(p_bmp->data[i], 1, p_bmp->Hbytes, f) != p_bmp->Hbytes) {
            printf("Error reading pixel data at row %d\n", i);
            fclose(f);
            return 0;
        }

        // 分離RGB通道
        for (int j = 0; j < p_bmp->Hpixels; j++) {
            p_bmp->B[i][j] = p_bmp->data[i][j * 3];
            p_bmp->G[i][j] = p_bmp->data[i][j * 3 + 1];
            p_bmp->R[i][j] = p_bmp->data[i][j * 3 + 2];
        }
    }

    fclose(f);
    return 1;
}

// 將通道數據保存到txt檔
void save_channel_to_file(char *filename, unsigned char **channel, int width, int height) {
    FILE *f = fopen(filename, "w");
    if (f == NULL) {
        fprintf(stderr, "Cannot open file %s for writing\n", filename);
        return;
    }

    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            fprintf(f, "%d", channel[i][j]);
            if (j < width - 1) {
                fprintf(f, " ");
            }
        }
        fprintf(f, "\n");
    }

    fclose(f);
}

// 保存圖片尺寸
void save_dimensions_to_file(char *filename, int width, int height) {
    FILE *f = fopen(filename, "w");
    if (f == NULL) {
        fprintf(stderr, "Cannot open file %s for writing\n", filename);
        return;
    }
    fprintf(f, "%d %d\n", width, height);
    fclose(f);
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

    bmp image = {0};  // 初始化結構
    if (!bmp_load_fn(argv[2], &image)) {
        fprintf(stderr, "Failed to load BMP file\n");
        return 1;
    }

    save_channel_to_file(argv[3], image.R, image.Hpixels, image.Vpixels);
    save_channel_to_file(argv[4], image.G, image.Hpixels, image.Vpixels);
    save_channel_to_file(argv[5], image.B, image.Hpixels, image.Vpixels);
    save_dimensions_to_file(argv[6], image.Hpixels, image.Vpixels);

    bmp_free(&image);
    return 0;
}