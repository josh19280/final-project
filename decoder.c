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
        else return 1;
    }

    return 0;
}