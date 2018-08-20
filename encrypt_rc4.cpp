#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void swap(char *s1, char *s2)
{
    char temp;
    temp = *s1;
    *s1 = *s2;
    *s2 = temp;
}

void re_S(char *S)
{
    for(int i=0; i<256; i++) S[i]=i;
}

void re_T(char *T, char *key)
{
    int keylen;
    keylen = strlen(key);
    for (int i = 0; i < 256; i++)
    T[i] = key[i%keylen];
}

void re_Sbox(char *S, char *T)
{
    int j = 0;
    for(int i = 0; i < 256; i++)
    {
        j = (j + S[i] + T[i]) % 256;
        swap(&S[i], &S[j]);
    }
}

void rc4(FILE *readfile, FILE *writefile, char *key)
{
    char S[256] = {0};
    char readbuf[1];
    int i, j, t;
    char T[256] = {0};

    re_S(S);
    re_T(T, key);
    re_Sbox(S, T);

    i = j = 0;

    while(fread(readbuf, 1, 1,readfile))
    {
        i = (i + 1) % 256;
        j = (j + S[i]) % 256;
        swap(&S[i], &S[j]);
        t = (S[i] + (S[j] % 256)) % 256;
        readbuf[0] = readbuf[0] ^ S[t];
        fwrite(readbuf, 1, 1, writefile);
        memset(readbuf, 0, 1);
    }
    printf("ok!\n");
}

int readkey(char *keyfile, char *key)
{
    FILE *file1 = fopen(keyfile, "r");
    if (file1 == NULL) return -1;
    fscanf(file1, "%s", key);
    fclose(file1);
    return 0;
}

void encrypt(char *sourcefile, char *destfile, char *key0)
{
    char key[256] = "abcd1234";
    if (key0 && strlen(key0) > 0) {
        strncpy(key, key0, 255);
        key[255] = 0;
    }

    FILE *file1 = fopen(sourcefile, "rb");
    if (file1 == NULL) return;
    FILE *file2 = fopen(destfile, "wb");
    if (file2 == NULL) {
        fclose(file1);
        return;
    }

    rc4(file1, file2, key);

    fclose(file1);
    fclose(file2);
}

