#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "cJSON.h"

typedef struct {
    char category[20];
    char word[50];
    int start_pos;
    int length;
} Blank;

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

char *generate_poem_line(const char *template_str, cJSON *words, Blank *blanks, int *blank_count) {
    int template_len = strlen(template_str);
    char *output = malloc(512);
    int out_pos = 0;

    char *used_words[20];
    int used_count = 0;

    *blank_count = 0;

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
            int word_start_pos = out_pos;

            for (int j = 0; j < word_len; j++) {
                output[out_pos] = word[j];
                out_pos++;
            }

            strcpy(blanks[*blank_count].category, category);
            strcpy(blanks[*blank_count].word, word);
            blanks[*blank_count].start_pos = word_start_pos;
            blanks[*blank_count].length = word_len;
            (*blank_count)++;

        } else {
            output[out_pos] = template_str[i];
            out_pos++;
            i++;
        }
    }

    output[out_pos] = '\0';
    return output;
}

char *reroll_word(char *output, Blank *blanks, int blank_count, int reroll_index, cJSON *words) {
    if (reroll_index < 0 || reroll_index >= blank_count) {
        return output;
    }

    char old_category[20];
    strcpy(old_category, blanks[reroll_index].category);
    char old_word[50];
    strcpy(old_word, blanks[reroll_index].word);
    int start_pos = blanks[reroll_index].start_pos;
    int old_length = blanks[reroll_index].length;

    char *new_word = get_random_word(old_category, words);
    while (strcmp(new_word, old_word) == 0) {
        new_word = get_random_word(old_category, words);
    }
    int new_length = strlen(new_word);

    int diff = new_length - old_length;

    int old_output_len = strlen(output);
    int tail_start = start_pos + old_length;
    int tail_length = old_output_len - tail_start;

    memmove(output + tail_start + diff, output + tail_start, tail_length + 1);

    for (int j = 0; j < new_length; j++) {
        output[start_pos + j] = new_word[j];
    }

    strcpy(blanks[reroll_index].word, new_word);
    blanks[reroll_index].length = new_length;

    for (int b = 0; b < blank_count; b++) {
        if (b != reroll_index && blanks[b].start_pos > start_pos) {
            blanks[b].start_pos += diff;
        }
    }

    return output;
}

void print_wrapped(const char *text, int max_width) {
    int len = strlen(text);
    int col = 0;
    int i = 0;

    while (i < len) {
        int word_start = i;
        while (i < len && text[i] != ' ') {
            i++;
        }
        int word_len = i - word_start;

        if (col > 0 && col + 1 + word_len > max_width) {
            printf("\n");
            col = 0;
        } else if (col > 0) {
            printf(" ");
            col++;
        }

        for (int j = word_start; j < i; j++) {
            printf("%c", text[j]);
        }
        col += word_len;

        if (i < len && text[i] == ' ') {
            i++;
        }
    }
}

int find_tapped_blank(touchPosition touch, Blank *blanks, int blank_count) {
    int column = touch.px / 8;

    for (int b = 0; b < blank_count; b++) {
        if (column >= blanks[b].start_pos && column < blanks[b].start_pos + blanks[b].length) {
            return b;
        }
    }

    return -1;
}

void start_new_line(cJSON *templates, cJSON *words, int template_count,
                     char **current_line_text, Blank *blanks, int *blank_count) {
    if (*current_line_text != NULL) {
        free(*current_line_text);
    }

    cJSON *template = cJSON_GetArrayItem(templates, random_index(template_count));
    *current_line_text = generate_poem_line(template->valuestring, words, blanks, blank_count);

    printf("\x1b[2J");
    printf("\x1b[1;1H");
    print_wrapped(*current_line_text, 38);
    printf("\n\nTap a word to reroll it.\nA: confirm line   START: exit");
}

int main(int argc, char **argv) {
    char *completed_lines[5];
    int current_line = 0;

    char *current_line_text = NULL;
    Blank current_blanks[20];
    int current_blank_count = 0;

    PrintConsole topScreen, bottomScreen;

    gfxInitDefault();
    consoleInit(GFX_TOP, &topScreen);
    consoleInit(GFX_BOTTOM, &bottomScreen);
    consoleSelect(&bottomScreen);
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

    int poem_done = 0;

    start_new_line(templates, words, template_count, &current_line_text, current_blanks, &current_blank_count);

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();

        touchPosition touch;
        hidTouchRead(&touch);

        if (kDown & KEY_START) break;

        if (!poem_done) {
            if (kDown & KEY_TOUCH) {
                int tapped = find_tapped_blank(touch, current_blanks, current_blank_count);
                if (tapped != -1) {
                    current_line_text = reroll_word(current_line_text, current_blanks, current_blank_count, tapped, words);
                    printf("\x1b[2J");
                    printf("\x1b[1;1H");
                    print_wrapped(current_line_text, 38);
                    printf("\n\nTap a word to reroll it.\nA: confirm line   START: exit");
                }
            }

            if (kDown & KEY_A) {
                completed_lines[current_line] = current_line_text;
                current_line_text = NULL;

                // print this newly confirmed line on the top screen
                consoleSelect(&topScreen);
                print_wrapped(completed_lines[current_line], 48);
                printf("\n\n");
                consoleSelect(&bottomScreen);  // switch back so bottom-screen editing keeps working

                current_line++;

                if (current_line < 5) {
                    start_new_line(templates, words, template_count, &current_line_text, current_blanks, &current_blank_count);
                } else {
                    poem_done = 1;
                    consoleSelect(&bottomScreen);
                    printf("\x1b[2J");
                    printf("\x1b[1;1H");
                    printf("Poem complete!\nSTART: exit");
                }
            }
        }

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    for (int i = 0; i < current_line; i++) {
        free(completed_lines[i]);
    }
    if (current_line_text != NULL) {
        free(current_line_text);
    }
    free(contents);
    gfxExit();
    return 0;
}