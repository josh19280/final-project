#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define PI 3.141592653589793

// Quantization tables (JPEG standard)
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

// RGB -> YCbCr conversion
void RGB2YCbCr(unsigned char R, unsigned char G, unsigned char B,
               double *Y, double *Cb, double *Cr)
{
    *Y  =  0.299*R + 0.587*G + 0.114*B;
    *Cb = -0.168736*R - 0.331264*G + 0.5*B + 128;
    *Cr =  0.5*R - 0.418688*G - 0.081312*B + 128;
}

// 2D-DCT for 8x8 block
void DCT8x8(double block[8][8], double F[8][8])
{
    for (int u=0; u<8; u++) {
        for (int v=0; v<8; v++) {
            double sum = 0.0;
            for (int r=0; r<8; r++) { 
                for (int c=0; c<8; c++) {      
                    sum += (block[r][c] - 128.0) *  
                           cos((2*r+1)*u*PI/16.0) *
                           cos((2*c+1)*v*PI/16.0);
                }
            }
            double cu = (u==0) ? 1.0/sqrt(2.0) : 1.0;
            double cv = (v==0) ? 1.0/sqrt(2.0) : 1.0;
            F[u][v] = 0.25 * cu * cv * sum;
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc < 2) return 1;

    int method = atoi(argv[1]);

    /* ===================== Method 0 ===================== */
    if (method == 0) {
        if (argc != 7) return 1;

        FILE *fp = fopen(argv[2], "rb");
        if (!fp) return 1;

        unsigned char header[54];
        fread(header, 1, 54, fp);
        int width  = *(int*)&header[18];  // 第 18-21 bytes 是寬度
        int height = *(int*)&header[22];  // 第 22-25 bytes 是高度
        int rowSize = (width * 3 + 3) & ~3;

        FILE *fr = fopen(argv[3], "w");
        FILE *fg = fopen(argv[4], "w");
        FILE *fb = fopen(argv[5], "w");
        FILE *fd = fopen(argv[6], "w");

        fprintf(fd, "%d %d\n", width, height);

        unsigned char *row = malloc(rowSize);

        // BMP bottom-up
        for (int y = height - 1; y >= 0; y--) {
            fseek(fp, 54 + y * rowSize, SEEK_SET);
            fread(row, 1, rowSize, fp);

            for (int x = 0; x < width; x++) {
                fprintf(fb, "%d ", row[x*3 + 0]);
                fprintf(fg, "%d ", row[x*3 + 1]);
                fprintf(fr, "%d ", row[x*3 + 2]);
            }
            fprintf(fr, "\n");
            fprintf(fg, "\n");
            fprintf(fb, "\n");
        }

        free(row);
        fclose(fp); fclose(fr); fclose(fg); fclose(fb); fclose(fd);
    }

    /* ===================== Method 1 ===================== */
    else if (method == 1) {
        if (argc != 13) return 1;

        FILE *fp = fopen(argv[2], "rb");
        if (!fp) return 1;

        unsigned char header[54];
        fread(header, 1, 54, fp);
        int width  = *(int*)&header[18];
        int height = *(int*)&header[22];
        int rowSize = (width * 3 + 3) & ~3;

        FILE *fdim = fopen(argv[6], "w");
        fprintf(fdim, "%d %d\n", width, height);
        fclose(fdim);

        FILE *fQtY  = fopen(argv[3], "w");
        FILE *fQtCb = fopen(argv[4], "w");
        FILE *fQtCr = fopen(argv[5], "w");
        for (int i=0;i<8;i++){
            for (int j=0;j<8;j++){
                fprintf(fQtY,  "%d ", QtY[i][j]);
                fprintf(fQtCb, "%d ", QtC[i][j]);
                fprintf(fQtCr, "%d ", QtC[i][j]);
            }
            fprintf(fQtY,"\n"); fprintf(fQtCb,"\n"); fprintf(fQtCr,"\n");
        }
        fclose(fQtY); fclose(fQtCb); fclose(fQtCr);

        FILE *fqY  = fopen(argv[7],  "wb");
        FILE *fqCb = fopen(argv[8],  "wb");
        FILE *fqCr = fopen(argv[9],  "wb");
        FILE *feY  = fopen(argv[10], "wb");
        FILE *feCb = fopen(argv[11], "wb");
        FILE *feCr = fopen(argv[12], "wb");

        unsigned char **image = malloc(height * sizeof(unsigned char*));
        for (int i=0;i<height;i++)
            image[i] = malloc(width * 3);

        for (int y=0;y<height;y++){
            fseek(fp, 54 + (height-1-y)*rowSize, SEEK_SET);
            fread(image[y],1,width*3,fp);
        }
        fclose(fp);

        int num_blocks_h = (height + 7)/8;
        int num_blocks_w = (width  + 7)/8;

        for (int by=0;by<num_blocks_h;by++){
            for (int bx=0;bx<num_blocks_w;bx++){
                double blockY[8][8] = {0};
                double blockCb[8][8] = {0};
                double blockCr[8][8] = {0};
                double FY[8][8], FCb[8][8], FCr[8][8];

                for (int i=0;i<8;i++){
                    for (int j=0;j<8;j++){
                        int py = by*8+i;
                        int px = bx*8+j;
                        if (py>=height) py=height-1;
                        if (px>=width)  px=width-1;

                        unsigned char B=image[py][px*3+0];
                        unsigned char G=image[py][px*3+1];
                        unsigned char R=image[py][px*3+2];

                        RGB2YCbCr(R,G,B,&blockY[i][j],&blockCb[i][j],&blockCr[i][j]);
                    }
                }

                DCT8x8(blockY,FY);
                DCT8x8(blockCb,FCb);
                DCT8x8(blockCr,FCr);

                for(int i=0;i<8;i++)for(int j=0;j<8;j++){
                    short qY=(short)round(FY[i][j]/QtY[i][j]);
                    short qC=(short)round(FCb[i][j]/QtC[i][j]);
                    short qR=(short)round(FCr[i][j]/QtC[i][j]);
                    fwrite(&qY,sizeof(short),1,fqY);
                    fwrite(&qC,sizeof(short),1,fqCb);
                    fwrite(&qR,sizeof(short),1,fqCr);

                    float eY=(float)(FY[i][j]-qY*QtY[i][j]);
                    float eC=(float)(FCb[i][j]-qC*QtC[i][j]);
                    float eR=(float)(FCr[i][j]-qR*QtC[i][j]);
                    fwrite(&eY,sizeof(float),1,feY);
                    fwrite(&eC,sizeof(float),1,feCb);
                    fwrite(&eR,sizeof(float),1,feCr);
                }
            }
        }

        for(int i=0;i<height;i++) free(image[i]);
        free(image);

        fclose(fqY); fclose(fqCb); fclose(fqCr);
        fclose(feY); fclose(feCb); fclose(feCr);
    }

    return 0;
}