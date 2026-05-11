/*
 * tarsau.c
 * Sikistirma yapmadan metin dosyalarini .sau arsivine alan ve acan program.
 *
 * Derleme:
 *   gcc -Wall -Wextra -std=c11 -o tarsau tarsau.c
 *
 * Kullanim:
 *   ./tarsau -b t1 t2 t3.txt -o arsiv.sau
 *   ./tarsau -b t1 t2              // cikti: a.sau
 *   ./tarsau -a arsiv.sau
 *   ./tarsau -a arsiv.sau cikis_dizini
 */

#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

#define MAX_FILES 32
#define MAX_TOTAL_SIZE (200LL * 1024LL * 1024LL)
#define HEADER_SIZE_FIELD 10
#define DEFAULT_ARCHIVE "a.sau"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

typedef struct {
    char name[PATH_MAX];
    mode_t permissions;
    long long size;
} FileInfo;

static int ends_with_sau(const char *name) {
    size_t len = strlen(name);
    return len > 4 && strcmp(name + len - 4, ".sau") == 0;
}

static const char *base_name(const char *path) {
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static int is_ascii_text_file(const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) return 0;

    int ch;
    while ((ch = fgetc(fp)) != EOF) {
        unsigned char c = (unsigned char)ch;

        /* ASCII metin: 0-127 arasi karakterler kabul edilir.
           Ikili dosya belirtisi olarak NUL karakterini reddediyoruz. */
        if (c > 127 || c == '\0') {
            fclose(fp);
            return 0;
        }
    }

    fclose(fp);
    return 1;
}

static int make_dirs_if_needed(const char *dir) {
    char temp[PATH_MAX];
    size_t len;

    if (!dir || dir[0] == '\0') return 1;

    snprintf(temp, sizeof(temp), "%s", dir);
    len = strlen(temp);

    if (len > 1 && temp[len - 1] == '/') {
        temp[len - 1] = '\0';
    }

    for (char *p = temp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(temp) != 0 && errno != EEXIST) {
                return 0;
            }
            *p = '/';
        }
    }

    if (mkdir(temp) != 0 && errno != EEXIST) {
        return 0;
    }

    return 1;
}

static int copy_n_bytes(FILE *src, FILE *dst, long long n) {
    char buffer[8192];

    while (n > 0) {
        size_t want = (n > (long long)sizeof(buffer)) ? sizeof(buffer) : (size_t)n;
        size_t read_count = fread(buffer, 1, want, src);

        if (read_count == 0) {
            return 0;
        }

        if (fwrite(buffer, 1, read_count, dst) != read_count) {
            return 0;
        }

        n -= (long long)read_count;
    }

    return 1;
}

static int build_archive(int argc, char *argv[]) {
    FileInfo files[MAX_FILES];
    int file_count = 0;
    const char *output_name = DEFAULT_ARCHIVE;
    long long total_size = 0;

    if (argc < 3) {
        fprintf(stderr, "Kullanim: %s -b dosya1 dosya2 ... [-o arsiv.sau]\n", argv[0]);
        return 1;
    }

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Arsiv dosya adi belirtilmedi!\n");
                return 1;
            }
            output_name = argv[i + 1];
            i++;
            continue;
        }

        if (file_count >= MAX_FILES) {
            fprintf(stderr, "Giris dosyasi sayisi en fazla 32 olabilir!\n");
            return 1;
        }

        struct stat st;
        if (stat(argv[i], &st) != 0 || !S_ISREG(st.st_mode)) {
            printf("%s giris dosyasinin formati uyumsuzdur!\n", argv[i]);
            return 1;
        }

        if (!is_ascii_text_file(argv[i])) {
            printf("%s giris dosyasinin formati uyumsuzdur!\n", argv[i]);
            return 1;
        }

        total_size += (long long)st.st_size;
        if (total_size > MAX_TOTAL_SIZE) {
            fprintf(stderr, "Giris dosyalarinin toplam boyutu 200 MB'i gecemez!\n");
            return 1;
        }

        snprintf(files[file_count].name, sizeof(files[file_count].name), "%s", argv[i]);
        files[file_count].permissions = st.st_mode & 0777;
        files[file_count].size = (long long)st.st_size;
        file_count++;
    }

    if (file_count == 0) {
        fprintf(stderr, "En az bir giris dosyasi verilmelidir!\n");
        return 1;
    }

    if (!ends_with_sau(output_name)) {
        fprintf(stderr, "Arsiv dosyasi .sau uzantili olmalidir!\n");
        return 1;
    }

    char organization[65536];
    organization[0] = '\0';

    for (int i = 0; i < file_count; i++) {
        char record[PATH_MAX + 100];
        snprintf(record, sizeof(record), "|%s,%o,%lld|",
                 base_name(files[i].name),
                 (unsigned int)files[i].permissions,
                 files[i].size);

        if (strlen(organization) + strlen(record) >= sizeof(organization)) {
            fprintf(stderr, "Organizasyon bolumu cok buyuk!\n");
            return 1;
        }

        strcat(organization, record);
    }

    long long org_size = HEADER_SIZE_FIELD + (long long)strlen(organization);

    FILE *archive = fopen(output_name, "wb");
    if (!archive) {
        perror("Arsiv dosyasi olusturulamadi");
        return 1;
    }

    /* Ilk 10 bayt: organizasyon bolumunun toplam boyutu. */
    fprintf(archive, "%010lld", org_size);
    fwrite(organization, 1, strlen(organization), archive);

    for (int i = 0; i < file_count; i++) {
        FILE *in = fopen(files[i].name, "rb");
        if (!in) {
            fclose(archive);
            perror("Giris dosyasi acilamadi");
            return 1;
        }

        if (!copy_n_bytes(in, archive, files[i].size)) {
            fclose(in);
            fclose(archive);
            fprintf(stderr, "Dosya arsive yazilirken hata olustu!\n");
            return 1;
        }

        fclose(in);
    }

    fclose(archive);
    printf("Arsiv olusturuldu: %s\n", output_name);
    return 0;
}

static int parse_archive_header(FILE *archive, FileInfo files[], int *file_count, long long *data_start) {
    char size_field[HEADER_SIZE_FIELD + 1];

    if (fread(size_field, 1, HEADER_SIZE_FIELD, archive) != HEADER_SIZE_FIELD) {
        return 0;
    }

    size_field[HEADER_SIZE_FIELD] = '\0';

    for (int i = 0; i < HEADER_SIZE_FIELD; i++) {
        if (size_field[i] < '0' || size_field[i] > '9') return 0;
    }

    long long org_size = atoll(size_field);
    if (org_size < HEADER_SIZE_FIELD) return 0;

    long long rest_size = org_size - HEADER_SIZE_FIELD;
    char *organization = malloc((size_t)rest_size + 1);
    if (!organization) return 0;

    if (fread(organization, 1, (size_t)rest_size, archive) != (size_t)rest_size) {
        free(organization);
        return 0;
    }
    organization[rest_size] = '\0';

    *file_count = 0;
    char *p = organization;

    while (*p) {
        if (*p != '|') {
            free(organization);
            return 0;
        }
        p++;

        char *end = strchr(p, '|');
        if (!end) {
            free(organization);
            return 0;
        }
        *end = '\0';

        char *name = strtok(p, ",");
        char *perm = strtok(NULL, ",");
        char *size = strtok(NULL, ",");
        char *extra = strtok(NULL, ",");

        if (!name || !perm || !size || extra || *file_count >= MAX_FILES) {
            free(organization);
            return 0;
        }

        snprintf(files[*file_count].name, sizeof(files[*file_count].name), "%s", name);
        files[*file_count].permissions = (mode_t)strtol(perm, NULL, 8);
        files[*file_count].size = atoll(size);

        if (files[*file_count].size < 0) {
            free(organization);
            return 0;
        }

        (*file_count)++;
        p = end + 1;
    }

    *data_start = org_size;
    free(organization);
    return 1;
}

static int extract_archive(int argc, char *argv[]) {
    if (argc < 3 || argc > 4) {
        fprintf(stderr, "Kullanim: %s -a arsiv.sau [dizin]\n", argv[0]);
        return 1;
    }

    const char *archive_name = argv[2];
    const char *output_dir = (argc == 4) ? argv[3] : NULL;

    if (!ends_with_sau(archive_name)) {
        printf("Arsiv dosyasi uygunsuz veya bozuk!\n");
        return 1;
    }

    FILE *archive = fopen(archive_name, "rb");
    if (!archive) {
        printf("Arsiv dosyasi uygunsuz veya bozuk!\n");
        return 1;
    }

    FileInfo files[MAX_FILES];
    int file_count = 0;
    long long data_start = 0;

    if (!parse_archive_header(archive, files, &file_count, &data_start)) {
        fclose(archive);
        printf("Arsiv dosyasi uygunsuz veya bozuk!\n");
        return 1;
    }

    if (output_dir && !make_dirs_if_needed(output_dir)) {
        fclose(archive);
        perror("Dizin olusturulamadi");
        return 1;
    }

    if (fseek(archive, data_start, SEEK_SET) != 0) {
        fclose(archive);
        printf("Arsiv dosyasi uygunsuz veya bozuk!\n");
        return 1;
    }

    for (int i = 0; i < file_count; i++) {
        char output_path[PATH_MAX];

        if (output_dir) {
            snprintf(output_path, sizeof(output_path), "%s/%s", output_dir, files[i].name);
        } else {
            snprintf(output_path, sizeof(output_path), "%s", files[i].name);
        }

        FILE *out = fopen(output_path, "wb");
        if (!out) {
            fclose(archive);
            perror("Cikis dosyasi olusturulamadi");
            return 1;
        }

        if (!copy_n_bytes(archive, out, files[i].size)) {
            fclose(out);
            fclose(archive);
            printf("Arsiv dosyasi uygunsuz veya bozuk!\n");
            return 1;
        }

        fclose(out);

        if (chmod(output_path, files[i].permissions) != 0) {
            perror("Dosya izinleri ayarlanamadi");
            fclose(archive);
            return 1;
        }
    }

    fclose(archive);
    printf("Arsiv basariyla acildi.\n");
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Kullanim:\n");
        fprintf(stderr, "  %s -b dosya1 dosya2 ... [-o arsiv.sau]\n", argv[0]);
        fprintf(stderr, "  %s -a arsiv.sau [dizin]\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "-b") == 0) {
        return build_archive(argc, argv);
    }

    if (strcmp(argv[1], "-a") == 0) {
        return extract_archive(argc, argv);
    }

    fprintf(stderr, "Gecersiz parametre! -b veya -a kullaniniz.\n");
    return 1;
}
