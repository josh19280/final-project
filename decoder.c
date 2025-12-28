#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define PI 3.141592653589793

void YCbCr2RGB(double Y, double Cb, double Cr,
               unsigned char *R, unsigned char *G, unsigned char *B)
{
    double r = Y + 1.402 * (Cr - 128.0);
    double g = Y - 0.344136 * (Cb - 128.0) - 0.714136 * (Cr - 128.0);
    double b = Y + 1.772 * (Cb - 128.0);

    *R = (r < 0) ? 0 : ((r > 255) ? 255 : (unsigned char)round(r));
    *G = (g < 0) ? 0 : ((g > 255) ? 255 : (unsigned char)round(g));
    *B = (b < 0) ? 0 : ((b > 255) ? 255 : (unsigned char)round(b));
}

void IDCT8x8(double F[8][8], double block[8][8])
{
    for (int x = 0; x < 8; x++) {
        for (int y = 0; y < 8; y++) {
            double sum = 0.0;
            for (int u = 0; u < 8; u++) {
                for (int v = 0; v < 8; v++) {
                    double cu = (u == 0) ? 1.0/sqrt(2.0) : 1.0;
                    double cv = (v == 0) ? 1.0/sqrt(2.0) : 1.0;
                    sum += cu * cv * F[u][v] *
                           cos((2*x+1)*u*PI/16.0) *
                           cos((2*y+1)*v*PI/16.0);
                }
            }
            block[x][y] = 0.25 * sum + 128.0;  // 記得加回 128
        }
    }
}

void write_bmp(const char *filename, unsigned char **image, int width, int height)
{
    FILE *fp = fopen(filename, "wb");
    if (!fp) return;

    int rowSize = (width * 3 + 3) & ~3;
    int imageSize = rowSize * height;
    int fileSize = 54 + imageSize;

    unsigned char header[54] = {
        'B','M',
        fileSize, fileSize>>8, fileSize>>16, fileSize>>24,
        0,0,0,0,
        54,0,0,0,
        40,0,0,0,
        width, width>>8, width>>16, width>>24,
        height, height>>8, height>>16, height>>24,
        1,0, 24,0,
        0,0,0,0,
        imageSize, imageSize>>8, imageSize>>16, imageSize>>24,
        0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0
    };

    fwrite(header, 1, 54, fp);

    unsigned char *row = calloc(rowSize, 1);
    for (int y = height - 1; y >= 0; y--) {
        memcpy(row, image[y], width * 3);
        fwrite(row, 1, rowSize, fp);
    }

    free(row);
    fclose(fp);
}

int main(int argc, char *argv[])
{
    if (argc < 2) return 1;
    int method = atoi(argv[1]);

    /* ================= method 0 ================= */
    if (method == 0) {
        if (argc != 7) return 1;

        FILE *fd = fopen(argv[6], "r");
        if (!fd) return 1;

        int width, height;
        fscanf(fd, "%d %d", &width, &height);
        fclose(fd);

        FILE *fr = fopen(argv[3], "r");
        FILE *fg = fopen(argv[4], "r");
        FILE *fb = fopen(argv[5], "r");
        if (!fr || !fg || !fb) return 1;

        unsigned char **image = malloc(height * sizeof(unsigned char*));
        for (int i = 0; i < height; i++)
            image[i] = malloc(width * 3);

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                int r, g, b;
                fscanf(fr, "%d", &r);
                fscanf(fg, "%d", &g);
                fscanf(fb, "%d", &b);
                image[y][x*3+0] = b;
                image[y][x*3+1] = g;
                image[y][x*3+2] = r;
            }
        }

        fclose(fr); fclose(fg); fclose(fb);
        write_bmp(argv[2], image, width, height);

        for (int i = 0; i < height; i++) free(image[i]);
        free(image);
    }

    /* ================= method 1 ================= */
    else if (method == 1) {

        /* ---------- method 1(a) ---------- */
        if (argc == 11) {
            unsigned char QtY[8][8], QtCb[8][8], QtCr[8][8];
            FILE *fQtY = fopen(argv[4], "r");
            FILE *fQtCb = fopen(argv[5], "r");
            FILE *fQtCr = fopen(argv[6], "r");

            for (int i=0;i<8;i++)
                for (int j=0;j<8;j++) {
                    int v;
                    fscanf(fQtY,"%d",&v); QtY[i][j]=v;
                    fscanf(fQtCb,"%d",&v); QtCb[i][j]=v;
                    fscanf(fQtCr,"%d",&v); QtCr[i][j]=v;
                }

            fclose(fQtY); fclose(fQtCb); fclose(fQtCr);

            FILE *fdim = fopen(argv[7], "r");
            int width, height;
            fscanf(fdim, "%d %d", &width, &height);
            fclose(fdim);

            FILE *fqY  = fopen(argv[8], "rb");
            FILE *fqCb = fopen(argv[9], "rb");
            FILE *fqCr = fopen(argv[10], "rb");

            unsigned char **image = malloc(height * sizeof(unsigned char*));
            for (int i=0;i<height;i++) image[i]=malloc(width*3);

            int bh = (height+7)/8, bw=(width+7)/8;

            for (int by=0;by<bh;by++)
                for (int bx=0;bx<bw;bx++) {
                    double FY[8][8],FCb[8][8],FCr[8][8];
                    double bY[8][8],bCb[8][8],bCr[8][8];

                    for(int i=0;i<8;i++)
                        for(int j=0;j<8;j++){
                            short q;
                            fread(&q,2,1,fqY);  FY[i][j]=q*QtY[i][j];
                            fread(&q,2,1,fqCb); FCb[i][j]=q*QtCb[i][j];
                            fread(&q,2,1,fqCr); FCr[i][j]=q*QtCr[i][j];
                        }

                    IDCT8x8(FY,bY);
                    IDCT8x8(FCb,bCb);
                    IDCT8x8(FCr,bCr);

                    for(int i=0;i<8;i++)
                        for(int j=0;j<8;j++){
                            int py=by*8+i, px=bx*8+j;
                            if(py<height && px<width){
                                unsigned char R,G,B;
                                YCbCr2RGB(bY[i][j],bCb[i][j],bCr[i][j],&R,&G,&B);
                                image[py][px*3+0]=B;
                                image[py][px*3+1]=G;
                                image[py][px*3+2]=R;
                            }
                        }
                }

            fclose(fqY); fclose(fqCb); fclose(fqCr);
            write_bmp(argv[2], image, width, height);

            for(int i=0;i<height;i++) free(image[i]);
            free(image);
        }

        /* ---------- method 1(b) ---------- */
        else if (argc == 13) {
            unsigned char QtY[8][8], QtCb[8][8], QtCr[8][8];
            FILE *fQtY=fopen(argv[3],"r");
            FILE *fQtCb=fopen(argv[4],"r");
            FILE *fQtCr=fopen(argv[5],"r");

            for(int i=0;i<8;i++)
                for(int j=0;j<8;j++){
                    int v;
                    fscanf(fQtY,"%d",&v); QtY[i][j]=v;
                    fscanf(fQtCb,"%d",&v); QtCb[i][j]=v;
                    fscanf(fQtCr,"%d",&v); QtCr[i][j]=v;
                }

            fclose(fQtY); fclose(fQtCb); fclose(fQtCr);

            FILE *fdim=fopen(argv[6],"r");
            int width,height;
            fscanf(fdim,"%d %d",&width,&height);
            fclose(fdim);

            FILE *fqY=fopen(argv[7],"rb");
            FILE *fqCb=fopen(argv[8],"rb");
            FILE *fqCr=fopen(argv[9],"rb");
            FILE *feY=fopen(argv[10],"rb");
            FILE *feCb=fopen(argv[11],"rb");
            FILE *feCr=fopen(argv[12],"rb");

            unsigned char **image=malloc(height*sizeof(unsigned char*));
            for(int i=0;i<height;i++) image[i]=malloc(width*3);

            int bh=(height+7)/8,bw=(width+7)/8;

            for(int by=0;by<bh;by++)
                for(int bx=0;bx<bw;bx++){
                    double FY[8][8],FCb[8][8],FCr[8][8];
                    double bY[8][8],bCb[8][8],bCr[8][8];

                    for(int i=0;i<8;i++)
                        for(int j=0;j<8;j++){
                            short q; float e;
                            fread(&q,2,1,fqY); fread(&e,4,1,feY);
                            FY[i][j]=q*QtY[i][j]+e;
                            fread(&q,2,1,fqCb); fread(&e,4,1,feCb);
                            FCb[i][j]=q*QtCb[i][j]+e;
                            fread(&q,2,1,fqCr); fread(&e,4,1,feCr);
                            FCr[i][j]=q*QtCr[i][j]+e;
                        }

                    IDCT8x8(FY,bY);
                    IDCT8x8(FCb,bCb);
                    IDCT8x8(FCr,bCr);

                    for(int i=0;i<8;i++)
                        for(int j=0;j<8;j++){
                            int py=by*8+i,px=bx*8+j;
                            if(py<height && px<width){
                                unsigned char R,G,B;
                                YCbCr2RGB(bY[i][j],bCb[i][j],bCr[i][j],&R,&G,&B);
                                image[py][px*3+0]=B;
                                image[py][px*3+1]=G;
                                image[py][px*3+2]=R;
                            }
                        }
                }

            fclose(fqY);fclose(fqCb);fclose(fqCr);
            fclose(feY);fclose(feCb);fclose(feCr);

            write_bmp(argv[2], image, width, height);
            for(int i=0;i<height;i++) free(image[i]);
            free(image);
        }
    }
/* ================= method 2 ================= */
    else if (method == 2) {
        int is_ascii = (strcmp(argv[3], "ascii") == 0);

        if (is_ascii) {
            // Method 2(a): ASCII format
            FILE *fin = fopen(argv[4], "r");
            if (!fin) return 1;

            int width, height;
            fscanf(fin, "%d %d", &width, &height);

            int num_blocks_h = (height + 7)/8;
            int num_blocks_w = (width  + 7)/8;

            // Zigzag table
            int zigzag[64] = {
                0,  1,  5,  6, 14, 15, 27, 28,
                2,  4,  7, 13, 16, 26, 29, 42,
                3,  8, 12, 17, 25, 30, 41, 43,
                9, 11, 18, 24, 31, 40, 44, 53,
               10, 19, 23, 32, 39, 45, 52, 54,
               20, 22, 33, 38, 46, 51, 55, 60,
               21, 34, 37, 47, 50, 56, 59, 61,
               35, 36, 48, 49, 57, 58, 62, 63
            };

            short ***qF_Y  = malloc(num_blocks_h * sizeof(short**));
            short ***qF_Cb = malloc(num_blocks_h * sizeof(short**));
            short ***qF_Cr = malloc(num_blocks_h * sizeof(short**));

            for (int m=0; m<num_blocks_h; m++) {
                qF_Y[m]  = malloc(num_blocks_w * sizeof(short*));
                qF_Cb[m] = malloc(num_blocks_w * sizeof(short*));
                qF_Cr[m] = malloc(num_blocks_w * sizeof(short*));
                for (int n=0; n<num_blocks_w; n++) {
                    qF_Y[m][n]  = calloc(64, sizeof(short));
                    qF_Cb[m][n] = calloc(64, sizeof(short));
                    qF_Cr[m][n] = calloc(64, sizeof(short));
                }
            }

            // Read RLE data
            for (int by=0; by<num_blocks_h; by++) {
                for (int bx=0; bx<num_blocks_w; bx++) {
                    int m, n;
                    char ch;

                    // Y channel
                    fscanf(fin, " (%d,%d, %c)", &m, &n, &ch);
                    int pos = 0;
                    while (1) {
                        int skip, value;
                        fscanf(fin, "%d %d", &skip, &value);
                        if (skip == 0 && value == 0) break;
                        pos += skip;
                        if (pos < 64) qF_Y[by][bx][pos++] = value;
                    }

                    // Cb channel
                    fscanf(fin, " (%d,%d, %c%c)", &m, &n, &ch, &ch);
                    pos = 0;
                    while (1) {
                        int skip, value;
                        fscanf(fin, "%d %d", &skip, &value);
                        if (skip == 0 && value == 0) break;
                        pos += skip;
                        if (pos < 64) qF_Cb[by][bx][pos++] = value;
                    }

                    // Cr channel
                    fscanf(fin, " (%d,%d, %c%c)", &m, &n, &ch, &ch);
                    pos = 0;
                    while (1) {
                        int skip, value;
                        fscanf(fin, "%d %d", &skip, &value);
                        if (skip == 0 && value == 0) break;
                        pos += skip;
                        if (pos < 64) qF_Cr[by][bx][pos++] = value;
                    }
                }
            }
            fclose(fin);

            // Un-DPCM
            for (int by=1; by<num_blocks_h; by++) {
                qF_Y[by][0][0]  += qF_Y[by-1][0][0];
                qF_Cb[by][0][0] += qF_Cb[by-1][0][0];
                qF_Cr[by][0][0] += qF_Cr[by-1][0][0];
            }
            for (int by=0; by<num_blocks_h; by++) {
                for (int bx=1; bx<num_blocks_w; bx++) {
                    qF_Y[by][bx][0]  += qF_Y[by][bx-1][0];
                    qF_Cb[by][bx][0] += qF_Cb[by][bx-1][0];
                    qF_Cr[by][bx][0] += qF_Cr[by][bx-1][0];
                }
            }

            // Load quantization tables
            unsigned char QtY[8][8] = {
                {16,11,10,16,24,40,51,61},
                {12,12,14,19,26,58,60,55},
                {14,13,16,24,40,57,69,56},
                {14,17,22,29,51,87,80,62},
                {18,22,37,56,68,109,103,77},
                {24,35,55,64,81,104,113,92},
                {49,64,78,87,103,121,120,101},
                {72,92,95,98,112,100,103,99}
            };

            unsigned char QtC[8][8] = {
                {17,18,24,47,99,99,99,99},
                {18,21,26,66,99,99,99,99},
                {24,26,56,99,99,99,99,99},
                {47,66,99,99,99,99,99,99},
                {99,99,99,99,99,99,99,99},
                {99,99,99,99,99,99,99,99},
                {99,99,99,99,99,99,99,99},
                {99,99,99,99,99,99,99,99}
            };

            unsigned char **image = malloc(height * sizeof(unsigned char*));
            for (int i=0; i<height; i++) image[i] = malloc(width * 3);

                    // Decode each block
            for (int by=0; by<num_blocks_h; by++) {
                for (int bx=0; bx<num_blocks_w; bx++) {
                    double FY[8][8], FCb[8][8], FCr[8][8];
                    double bY[8][8], bCb[8][8], bCr[8][8];

                    // 先把 zigzag 順序的資料轉回 row-major 順序
                    short tempY[64], tempCb[64], tempCr[64];
                    
                    // Un-zigzag: qF_Y[by][bx][k] 是 zigzag 順序的第 k 個值
                    // 應該放回 zigzag[k] 這個位置（row-major index）
                    for (int k=0; k<64; k++) {
                        int pos = zigzag[k];  // 這是在 64 個元素中的位置
                        tempY[pos]  = qF_Y[by][bx][k];
                        tempCb[pos] = qF_Cb[by][bx][k];
                        tempCr[pos] = qF_Cr[by][bx][k];
                    }

                    // 現在把 row-major 的資料轉成 2D 並反量化
                    for (int i=0; i<8; i++) {
                        for (int j=0; j<8; j++) {
                            FY[i][j]  = (double)tempY[i*8+j] * QtY[i][j];
                            FCb[i][j] = (double)tempCb[i*8+j] * QtC[i][j];
                            FCr[i][j] = (double)tempCr[i*8+j] * QtC[i][j];
                        }
                    }

                    IDCT8x8(FY, bY);
                    IDCT8x8(FCb, bCb);
                    IDCT8x8(FCr, bCr);

                    for (int i=0; i<8; i++) {
                        for (int j=0; j<8; j++) {
                            int py = by*8+i, px = bx*8+j;
                            if (py<height && px<width) {
                                unsigned char R, G, B;
                                YCbCr2RGB(bY[i][j], bCb[i][j], bCr[i][j], &R, &G, &B);
                                image[py][px*3+0] = B;
                                image[py][px*3+1] = G;
                                image[py][px*3+2] = R;
                            }
                        }
                    }
                }
            }

            write_bmp(argv[2], image, width, height);

            for (int i=0; i<height; i++) free(image[i]);
            free(image);

            for (int m=0; m<num_blocks_h; m++) {
                for (int n=0; n<num_blocks_w; n++) {
                    free(qF_Y[m][n]);
                    free(qF_Cb[m][n]);
                    free(qF_Cr[m][n]);
                }
                free(qF_Y[m]);
                free(qF_Cb[m]);
                free(qF_Cr[m]);
            }
            free(qF_Y);
            free(qF_Cb);
            free(qF_Cr);

        } else {
            // Method 2(b): Binary format
            FILE *fin = fopen(argv[4], "rb");
            if (!fin) return 1;

            int width, height;
            fread(&width, sizeof(int), 1, fin);
            fread(&height, sizeof(int), 1, fin);

            int num_blocks_h = (height + 7)/8;
            int num_blocks_w = (width  + 7)/8;

            int zigzag[64] = {
                0,  1,  5,  6, 14, 15, 27, 28,
                2,  4,  7, 13, 16, 26, 29, 42,
                3,  8, 12, 17, 25, 30, 41, 43,
                9, 11, 18, 24, 31, 40, 44, 53,
               10, 19, 23, 32, 39, 45, 52, 54,
               20, 22, 33, 38, 46, 51, 55, 60,
               21, 34, 37, 47, 50, 56, 59, 61,
               35, 36, 48, 49, 57, 58, 62, 63
            };

            short ***qF_Y  = malloc(num_blocks_h * sizeof(short**));
            short ***qF_Cb = malloc(num_blocks_h * sizeof(short**));
            short ***qF_Cr = malloc(num_blocks_h * sizeof(short**));

            for (int m=0; m<num_blocks_h; m++) {
                qF_Y[m]  = malloc(num_blocks_w * sizeof(short*));
                qF_Cb[m] = malloc(num_blocks_w * sizeof(short*));
                qF_Cr[m] = malloc(num_blocks_w * sizeof(short*));
                for (int n=0; n<num_blocks_w; n++) {
                    qF_Y[m][n]  = calloc(64, sizeof(short));
                    qF_Cb[m][n] = calloc(64, sizeof(short));
                    qF_Cr[m][n] = calloc(64, sizeof(short));
                }
            }

            // Read RLE binary data
            for (int by=0; by<num_blocks_h; by++) {
                for (int bx=0; bx<num_blocks_w; bx++) {
                    // Y channel
                    int pos = 0;
                    while (1) {
                        short skip, value;
                        fread(&skip, sizeof(short), 1, fin);
                        fread(&value, sizeof(short), 1, fin);
                        if (skip == 0 && value == 0) break;
                        pos += skip;
                        if (pos < 64) {
                            qF_Y[by][bx][pos] = value;
                            pos++;
                        }
                    }

                    // Cb channel
                    pos = 0;
                    while (1) {
                        short skip, value;
                        fread(&skip, sizeof(short), 1, fin);
                        fread(&value, sizeof(short), 1, fin);
                        if (skip == 0 && value == 0) break;
                        pos += skip;
                        if (pos < 64) {
                            qF_Cb[by][bx][pos] = value;
                            pos++;
                        }
                    }

                    // Cr channel
                    pos = 0;
                    while (1) {
                        short skip, value;
                        fread(&skip, sizeof(short), 1, fin);
                        fread(&value, sizeof(short), 1, fin);
                        if (skip == 0 && value == 0) break;
                        pos += skip;
                        if (pos < 64) {
                            qF_Cr[by][bx][pos] = value;
                            pos++;
                        }
                    }
                }
            }
            fclose(fin);

            // Un-DPCM
            for (int by=1; by<num_blocks_h; by++) {
                qF_Y[by][0][0]  += qF_Y[by-1][0][0];
                qF_Cb[by][0][0] += qF_Cb[by-1][0][0];
                qF_Cr[by][0][0] += qF_Cr[by-1][0][0];
            }
            for (int by=0; by<num_blocks_h; by++) {
                for (int bx=1; bx<num_blocks_w; bx++) {
                    qF_Y[by][bx][0]  += qF_Y[by][bx-1][0];
                    qF_Cb[by][bx][0] += qF_Cb[by][bx-1][0];
                    qF_Cr[by][bx][0] += qF_Cr[by][bx-1][0];
                }
            }

            unsigned char QtY[8][8] = {
                {16,11,10,16,24,40,51,61},
                {12,12,14,19,26,58,60,55},
                {14,13,16,24,40,57,69,56},
                {14,17,22,29,51,87,80,62},
                {18,22,37,56,68,109,103,77},
                {24,35,55,64,81,104,113,92},
                {49,64,78,87,103,121,120,101},
                {72,92,95,98,112,100,103,99}
            };

            unsigned char QtC[8][8] = {
                {17,18,24,47,99,99,99,99},
                {18,21,26,66,99,99,99,99},
                {24,26,56,99,99,99,99,99},
                {47,66,99,99,99,99,99,99},
                {99,99,99,99,99,99,99,99},
                {99,99,99,99,99,99,99,99},
                {99,99,99,99,99,99,99,99},
                {99,99,99,99,99,99,99,99}
            };

            unsigned char **image = malloc(height * sizeof(unsigned char*));
            for (int i=0; i<height; i++) image[i] = malloc(width * 3);

                    // Decode each block
            for (int by=0; by<num_blocks_h; by++) {
                for (int bx=0; bx<num_blocks_w; bx++) {
                    double FY[8][8], FCb[8][8], FCr[8][8];
                    double bY[8][8], bCb[8][8], bCr[8][8];

                    // 先把 zigzag 順序的資料轉回 row-major 順序
                    short tempY[64], tempCb[64], tempCr[64];
                    
                    // Un-zigzag: qF_Y[by][bx][k] 是 zigzag 順序的第 k 個值
                    // 應該放回 zigzag[k] 這個位置（row-major index）
                    for (int k=0; k<64; k++) {
                        int pos = zigzag[k];  // 這是在 64 個元素中的位置
                        tempY[pos]  = qF_Y[by][bx][k];
                        tempCb[pos] = qF_Cb[by][bx][k];
                        tempCr[pos] = qF_Cr[by][bx][k];
                    }

                    // 現在把 row-major 的資料轉成 2D 並反量化
                    for (int i=0; i<8; i++) {
                        for (int j=0; j<8; j++) {
                            FY[i][j]  = (double)tempY[i*8+j] * QtY[i][j];
                            FCb[i][j] = (double)tempCb[i*8+j] * QtC[i][j];
                            FCr[i][j] = (double)tempCr[i*8+j] * QtC[i][j];
                        }
                    }

                    IDCT8x8(FY, bY);
                    IDCT8x8(FCb, bCb);
                    IDCT8x8(FCr, bCr);

                    for (int i=0; i<8; i++) {
                        for (int j=0; j<8; j++) {
                            int py = by*8+i, px = bx*8+j;
                            if (py<height && px<width) {
                                unsigned char R, G, B;
                                YCbCr2RGB(bY[i][j], bCb[i][j], bCr[i][j], &R, &G, &B);
                                image[py][px*3+0] = B;
                                image[py][px*3+1] = G;
                                image[py][px*3+2] = R;
                            }
                        }
                    }
                }
            }

            write_bmp(argv[2], image, width, height);

            for (int i=0; i<height; i++) free(image[i]);
            free(image);

            for (int m=0; m<num_blocks_h; m++) {
                for (int n=0; n<num_blocks_w; n++) {
                    free(qF_Y[m][n]);
                    free(qF_Cb[m][n]);
                    free(qF_Cr[m][n]);
                }
                free(qF_Y[m]);
                free(qF_Cb[m]);
                free(qF_Cr[m]);
            }
            free(qF_Y);
            free(qF_Cb);
            free(qF_Cr);
        }
    }

    return 0;
}