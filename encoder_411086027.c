#define _USE_MATH_DEFINES
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define PI 3.14159265358979323846

typedef struct _bmp {
    int Hpixels;
    int Vpixels;
    unsigned char HeaderInfo[54];
    unsigned long int Hbytes;
    unsigned char **data;
    unsigned char **R;
    unsigned char **G;
    unsigned char **B;
    float **Y;   // YCbCr components
    float **Cb;
    float **Cr;
} bmp;

// 標準JPEG量化表
const int Qt_Y_standard[8][8] = {
    {16, 11, 10, 16, 24, 40, 51, 61},
    {12, 12, 14, 19, 26, 58, 60, 55},
    {14, 13, 16, 24, 40, 57, 69, 56},
    {14, 17, 22, 29, 51, 87, 80, 62},
    {18, 22, 37, 56, 68, 109, 103, 77},
    {24, 35, 55, 64, 81, 104, 113, 92},
    {49, 64, 78, 87, 103, 121, 120, 101},
    {72, 92, 95, 98, 112, 100, 103, 99}
};

const int Qt_CbCr_standard[8][8] = {
    {17, 18, 24, 47, 99, 99, 99, 99},
    {18, 21, 26, 66, 99, 99, 99, 99},
    {24, 26, 56, 99, 99, 99, 99, 99},
    {47, 66, 99, 99, 99, 99, 99, 99},
    {99, 99, 99, 99, 99, 99, 99, 99},
    {99, 99, 99, 99, 99, 99, 99, 99},
    {99, 99, 99, 99, 99, 99, 99, 99},
    {99, 99, 99, 99, 99, 99, 99, 99}
};

// 創建DCT矩陣
void create_dct_matrix(float dct[8][8]) {
    float alpha;
    for(int i = 0; i < 8; i++) {
        alpha = (i == 0) ? 1.0/sqrt(2) : 1.0;
        for(int j = 0; j < 8; j++) {
            dct[i][j] = alpha * cos((2*j + 1) * i * PI / 16.0) / 2.0;
        }
    }
}

// RGB到YCbCr轉換
void rgb_to_ycbcr(unsigned char r, unsigned char g, unsigned char b, float *y, float *cb, float *cr) {
    *y = 0.299*r + 0.587*g + 0.114*b;
    *cb = -0.1687*r - 0.3313*g + 0.5*b + 128;
    *cr = 0.5*r - 0.4187*g - 0.0813*b + 128;
}

// 2D DCT轉換
void dct_transform(float input[8][8], float output[8][8], const float dct[8][8]) {
    float temp[8][8] = {0};
    
    // 第一次轉換：input × DCT
    for(int i = 0; i < 8; i++) {
        for(int j = 0; j < 8; j++) {
            float sum = 0;
            for(int k = 0; k < 8; k++) {
                sum += input[i][k] * dct[j][k];
            }
            temp[i][j] = sum;
        }
    }
    
    // 第二次轉換：DCT^T × temp
    for(int i = 0; i < 8; i++) {
        for(int j = 0; j < 8; j++) {
            float sum = 0;
            for(int k = 0; k < 8; k++) {
                sum += dct[i][k] * temp[k][j];
            }
            output[i][j] = sum;
        }
    }
}

// 改進量化表儲存函數
void save_quantization_table(const char *filename, const int qt[8][8]) {
    FILE *f = fopen(filename, "w");
    if (f == NULL) {
        fprintf(stderr, "無法開啟檔案 %s，錯誤碼：%d\n", filename, errno);
        perror("錯誤詳情");
        return;
    }
    
    for(int i = 0; i < 8; i++) {
        for(int j = 0; j < 8; j++) {
            if(fprintf(f, "%d%c", qt[i][j], j == 7 ? '\n' : ' ') < 0) {
                fprintf(stderr, "寫入檔案 %s 時發生錯誤\n", filename);
                fclose(f);
                return;
            }
        }
    }
    fclose(f);
}

// 儲存通道數據到文字檔
void save_channel_to_file(char *filename, unsigned char **channel, int width, int height) {
    FILE *f = fopen(filename, "w");
    if (f == NULL) {
        fprintf(stderr, "無法開啟檔案 %s\n", filename);
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

// 儲存圖片尺寸
void save_dimensions_to_file(char *filename, int width, int height) {
    FILE *f = fopen(filename, "w");
    if (f == NULL) {
        fprintf(stderr, "無法開啟檔案 %s\n", filename);
        return;
    }
    fprintf(f, "%d %d\n", width, height);
    fclose(f);
}

// DCT和量化處理的結構體
typedef struct {
    double power[64];
    double mse[64];
} DctQuantResult;

// 處理單個頻道的DCT和量化
void process_channel_dct_quant(float **channel, int height, int width, 
                             const int qt[8][8], const float dct_matrix[8][8],
                             const char *qf_file, const char *ef_file, const char *channel_name) {
    FILE *qf = fopen(qf_file, "wb");
    if (!qf) {
        fprintf(stderr, "無法開啟檔案 %s，錯誤碼：%d\n", qf_file, errno);
        perror("錯誤詳情");
        return;
    }

    FILE *ef = fopen(ef_file, "wb");
    if (!ef) {
        fprintf(stderr, "無法開啟檔案 %s，錯誤碼：%d\n", ef_file, errno);
        perror("錯誤詳情");
        fclose(qf);
        return;
    }

    // 確保高度和寬度是8的倍數
    int adjusted_height = (height / 8) * 8;
    int adjusted_width = (width / 8) * 8;

    float block[8][8], dct_coeffs[8][8];
    DctQuantResult result = {0};
    
    for(int by = 0; by < adjusted_height/8; by++) {
        for(int bx = 0; bx < adjusted_width/8; bx++) {
            // 提取8x8區塊
            for(int i = 0; i < 8; i++) {
                for(int j = 0; j < 8; j++) {
                    block[i][j] = channel[by*8 + i][bx*8 + j];
                }
            }
            
            // DCT轉換
            dct_transform(block, dct_coeffs, dct_matrix);
            
            // 量化和誤差計算
            for(int i = 0; i < 8; i++) {
                for(int j = 0; j < 8; j++) {
                    int idx = i*8 + j;
                    short quantized = (short)round(dct_coeffs[i][j] / qt[i][j]);
                    float error = dct_coeffs[i][j] - quantized * qt[i][j];
                    
                    // 檢查寫入是否成功
                    if(fwrite(&quantized, sizeof(short), 1, qf) != 1) {
                        fprintf(stderr, "寫入量化數據到 %s 時發生錯誤\n", qf_file);
                        goto cleanup;
                    }
                    if(fwrite(&error, sizeof(float), 1, ef) != 1) {
                        fprintf(stderr, "寫入誤差數據到 %s 時發生錯誤\n", ef_file);
                        goto cleanup;
                    }
                    
                    result.power[idx] += dct_coeffs[i][j] * dct_coeffs[i][j];
                    result.mse[idx] += error * error;
                }
            }
        }
    }

    // 輸出SQNR值
    printf("\n%s channel SQNR (dB):\n", channel_name);
    for(int i = 0; i < 64; i++) {
        if(result.mse[i] > 0) {
            double sqnr = 10 * log10(result.power[i] / result.mse[i]);
            printf("%.2f ", sqnr);
        } else {
            printf("inf ");
        }
        if((i+1) % 8 == 0) printf("\n");
    }

cleanup:
    fclose(qf);
    fclose(ef);
}

// 初始化YCbCr空間並進行轉換
void init_ycbcr(bmp *p_bmp) {
    p_bmp->Y = (float**)malloc(p_bmp->Vpixels * sizeof(float*));
    p_bmp->Cb = (float**)malloc(p_bmp->Vpixels * sizeof(float*));
    p_bmp->Cr = (float**)malloc(p_bmp->Vpixels * sizeof(float*));
    
    for(int i = 0; i < p_bmp->Vpixels; i++) {
        p_bmp->Y[i] = (float*)malloc(p_bmp->Hpixels * sizeof(float));
        p_bmp->Cb[i] = (float*)malloc(p_bmp->Hpixels * sizeof(float));
        p_bmp->Cr[i] = (float*)malloc(p_bmp->Hpixels * sizeof(float));
    }

    for(int i = 0; i < p_bmp->Vpixels; i++) {
        for(int j = 0; j < p_bmp->Hpixels; j++) {
            rgb_to_ycbcr(p_bmp->R[i][j], p_bmp->G[i][j], p_bmp->B[i][j],
                        &p_bmp->Y[i][j], &p_bmp->Cb[i][j], &p_bmp->Cr[i][j]);
            p_bmp->Y[i][j] -= 128;
            p_bmp->Cb[i][j] -= 128;
            p_bmp->Cr[i][j] -= 128;
        }
    }
}

// 釋放YCbCr內存
void free_ycbcr(bmp *p_bmp) {
    for(int i = 0; i < p_bmp->Vpixels; i++) {
        free(p_bmp->Y[i]);
        free(p_bmp->Cb[i]);
        free(p_bmp->Cr[i]);
    }
    free(p_bmp->Y);
    free(p_bmp->Cb);
    free(p_bmp->Cr);
}

// 讀取BMP檔案
int bmp_load_fn(char *filename, bmp *p_bmp) {
    FILE *f = fopen(filename, "rb");
    if (f == NULL) {
        printf("\n\n%s NOT FOUND\n\n", filename);
        return 0;
    }

    size_t bytesRead = fread(p_bmp->HeaderInfo, sizeof(unsigned char), 54, f);
    if (bytesRead != 54) {
        printf("Error reading header\n");
        fclose(f);
        return 0;
    }

    if (p_bmp->HeaderInfo[0] != 'B' || p_bmp->HeaderInfo[1] != 'M') {
        printf("Not a valid BMP file\n");
        fclose(f);
        return 0;
    }

    p_bmp->Hpixels = *(int*)&p_bmp->HeaderInfo[18];
    p_bmp->Vpixels = *(int*)&p_bmp->HeaderInfo[22];
    p_bmp->Hbytes = ((p_bmp->Hpixels * 3 + 3) & (~3));

    p_bmp->data = (unsigned char**)malloc(p_bmp->Vpixels * sizeof(unsigned char*));
    p_bmp->R = (unsigned char**)malloc(p_bmp->Vpixels * sizeof(unsigned char*));
    p_bmp->G = (unsigned char**)malloc(p_bmp->Vpixels * sizeof(unsigned char*));
    p_bmp->B = (unsigned char**)malloc(p_bmp->Vpixels * sizeof(unsigned char*));

    if (!p_bmp->data || !p_bmp->R || !p_bmp->G || !p_bmp->B) {
        printf("Memory allocation failed\n");
        fclose(f);
        return 0;
    }

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

        if (fread(p_bmp->data[i], 1, p_bmp->Hbytes, f) != p_bmp->Hbytes) {
            printf("Error reading pixel data at row %d\n", i);
            fclose(f);
            return 0;
        }

        for (int j = 0; j < p_bmp->Hpixels; j++) {
            p_bmp->B[i][j] = p_bmp->data[i][j * 3];
            p_bmp->G[i][j] = p_bmp->data[i][j * 3 + 1];
            p_bmp->R[i][j] = p_bmp->data[i][j * 3 + 2];
        }
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
    if(argc != 7 && argc != 12) {
        return 1;
    }

    int mode = atoi(argv[1]);
    if(mode != 0 && mode != 1) {
        fprintf(stderr, "Unsupported mode: %d\n", mode);
        return 1;
    }

    bmp image = {0};
    if(!bmp_load_fn(argv[2], &image)) {
        fprintf(stderr, "Failed to load BMP file\n");
        return 1;
    }

    // Save dimensions for both modes
    save_dimensions_to_file(mode == 0 ? argv[6] : argv[7], image.Hpixels, image.Vpixels);

    if(mode == 0) {
        // Original mode 0 processing
        save_channel_to_file(argv[3], image.R, image.Hpixels, image.Vpixels);
        save_channel_to_file(argv[4], image.G, image.Hpixels, image.Vpixels);
        save_channel_to_file(argv[5], image.B, image.Hpixels, image.Vpixels);
    } 
    if(mode == 1) {
        // 檢查圖片尺寸是否為8的倍數
        if(image.Hpixels % 8 != 0 || image.Vpixels % 8 != 0) {
            
        }
                
        // 儲存量化表
        save_quantization_table(argv[3], Qt_Y_standard);
        save_quantization_table(argv[4], Qt_CbCr_standard);
        save_quantization_table(argv[5], Qt_CbCr_standard);
        
        save_dimensions_to_file(argv[6], image.Hpixels, image.Vpixels);
        
        init_ycbcr(&image);
        
        float dct_matrix[8][8];
        create_dct_matrix(dct_matrix);
        
        process_channel_dct_quant(image.Y, image.Vpixels, image.Hpixels, 
                                Qt_Y_standard, dct_matrix,
                                argv[7], argv[10], "Y");
        
        process_channel_dct_quant(image.Cb, image.Vpixels, image.Hpixels,
                                Qt_CbCr_standard, dct_matrix,
                                argv[8], argv[11], "Cb");
        
        process_channel_dct_quant(image.Cr, image.Vpixels, image.Hpixels,
                                Qt_CbCr_standard, dct_matrix,
                                argv[9], argv[12], "Cr");
        
        free_ycbcr(&image);
    }
    
}
