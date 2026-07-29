#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "cJSON.h"

char* read_file(const char* filename) {
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        printf("Error: could not open file\n");
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    rewind(fp);

    char *buffer = malloc(size + 1);
    if (buffer == NULL) {
        printf("Error: memory allocation failed\n");
        fclose(fp);
        return NULL;
    }

    fread(buffer, 1, size, fp);
    buffer[size] = '\0';

    fclose(fp);
    return buffer;
}

int random_index(int array_size) {
    return rand() % array_size;
}

char *get_random_word(const char *category, cJSON *words) {
    cJSON *array = NULL;

    if (strcmp(category, "noun") == 0) {
        array = cJSON_GetObjectItem(words, "noun");
    } else if (strcmp(category, "adjective") == 0) {
        array = cJSON_GetObjectItem(words, "adjective");
    } else if (strcmp(category, "verb") == 0) {
        array = cJSON_GetObjectItem(words, "verb");
    }

    if (array == NULL) {
        return NULL;
    }

    int count = cJSON_GetArraySize(array);
    cJSON *word = cJSON_GetArrayItem(array, random_index(count));
    return word->valuestring;
}

char *generate_poem_line(const char *template_str, cJSON *words) {
    int template_len = strlen(template_str);
    char *output = malloc(512);
    int out_pos = 0;

    char *used_words[20];
    int used_count = 0;

    int i = 0;
    while (i < template_len) {
        if (template_str[i] == '{') {
            i++;

            char category[20];
            int cat_pos = 0;
            while (template_str[i] != '}') {
                category[cat_pos] = template_str[i];
                cat_pos++;
                i++;
            }
            category[cat_pos] = '\0';
            i++;

            char *word = get_random_word(category, words);

            int already_used = 1;
            while (already_used) {
                already_used = 0;
                for (int k = 0; k < used_count; k++) {
                    if (strcmp(word, used_words[k]) == 0) {
                        already_used = 1;
                        word = get_random_word(category, words);
                        break;
                    }
                }
            }

            used_words[used_count] = word;
            used_count++;

            int word_len = strlen(word);
            for (int j = 0; j < word_len; j++) {
                output[out_pos] = word[j];
                out_pos++;
            }

        } else {
            output[out_pos] = template_str[i];
            out_pos++;
            i++;
        }
    }

    output[out_pos] = '\0';
    return output;
}

void generate_and_print_poem(cJSON *templates, cJSON *words, int template_count) {
    printf("\x1b[2J"); // clear the screen
    printf("\x1b[1;1H"); // move cursor to top-left

    for (int i = 0; i < 5; i++) {
        cJSON *template = cJSON_GetArrayItem(templates, random_index(template_count));
        char *line = generate_poem_line(template->valuestring, words);
        printf("%s\n", line);
        free(line);
    }

    printf("\nPress A for a new poem. Press START to exit.");
}

int main(int argc, char **argv) {
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);
    srand(time(NULL));

    char *contents = read_file("sdmc:/3ds/poeticware/poetic_data.json");
    if (contents == NULL) {
        printf("Failed to load poetic_data.json from SD card.\n");
        printf("Press START to exit.");

        while (aptMainLoop()) {
            hidScanInput();
            u32 kDown = hidKeysDown();
            if (kDown & KEY_START) break;
            gfxFlushBuffers();
            gfxSwapBuffers();
            gspWaitForVBlank();
        }
        gfxExit();
        return 1;
    }

    cJSON *root = cJSON_Parse(contents);
    cJSON *templates = cJSON_GetObjectItem(root, "templates");
    cJSON *words = cJSON_GetObjectItem(root, "words");
    int template_count = cJSON_GetArraySize(templates);

    generate_and_print_poem(templates, words, template_count);

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();

        if (kDown & KEY_START) break;

        if (kDown & KEY_A) {
            generate_and_print_poem(templates, words, template_count);
        }

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    free(contents);
    gfxExit();
    return 0;
}