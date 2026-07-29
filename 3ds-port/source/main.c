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

// Walks the text exactly like print_wrapped, but instead of printing,
// records which wrapped ROW and COLUMN each character index lands on.
void compute_wrap_positions(const char *text, int max_width, int *char_to_row, int *char_to_col) {
    int len = strlen(text);
    int col = 0;
    int row = 0;
    int i = 0;

    while (i < len) {
        int word_start = i;
        while (i < len && text[i] != ' ') {
            i++;
        }
        int word_len = i - word_start;

        if (col > 0 && col + 1 + word_len > max_width) {
            row++;
            col = 0;
        } else if (col > 0) {
            col++;  // account for the space before this word
        }

        for (int j = word_start; j < i; j++) {
            char_to_row[j] = row;
            char_to_col[j] = col;
            col++;
        }

        if (i < len && text[i] == ' ') {
            char_to_row[i] = row;
            char_to_col[i] = col;
            i++;
        }
    }
}

// Prints text wrapped by word, with an extra blank line between wrapped rows for spacing
void print_wrapped(const char *text, int max_width, Blank *blanks, int blank_count) {
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
            printf("\n\n");
            col = 0;
        } else if (col > 0) {
            printf(" ");
            col++;
        }

        // check if this word is a blank, and if so, which category
        char *bg_code = NULL;
        for (int b = 0; b < blank_count; b++) {
            if (blanks[b].start_pos == word_start) {
                if (strcmp(blanks[b].category, "noun") == 0) {
                    bg_code = "\x1b[42m";
                } else if (strcmp(blanks[b].category, "adjective") == 0) {
                    bg_code = "\x1b[45m";
                } else if (strcmp(blanks[b].category, "verb") == 0) {
                    bg_code = "\x1b[44m";
                }
                break;
            }
        }

        if (bg_code != NULL) {
            printf("%s", bg_code);
        }

        for (int j = word_start; j < i; j++) {
            printf("%c", text[j]);
        }

        if (bg_code != NULL) {
            printf("\x1b[0m");  // reset back to default
        }

        col += word_len;

        if (i < len && text[i] == ' ') {
            i++;
        }
    }
}

// Maps a raw touch position to a blank index, accounting for multi-row wrapping
// and the extra blank spacer row print_wrapped inserts between wrapped rows.
int find_tapped_blank(touchPosition touch, Blank *blanks, int blank_count, int *char_to_row, int *char_to_col) {
    int tapped_col = touch.px / 8;
    int console_row = touch.py / 8;
    int tapped_wrapped_row = console_row / 2;  // 2 console rows per wrapped row (text + spacer)

    for (int b = 0; b < blank_count; b++) {
        int blank_row = char_to_row[blanks[b].start_pos];
        int blank_col_start = char_to_col[blanks[b].start_pos];
        int blank_col_end = blank_col_start + blanks[b].length;

        if (blank_row == tapped_wrapped_row && tapped_col >= blank_col_start && tapped_col < blank_col_end) {
            return b;
        }
    }

    return -1;
}

void redraw_current_line(const char *line_text, int *char_to_row, int *char_to_col, Blank *blanks, int blank_count) {
    printf("\x1b[2J");
    printf("\x1b[1;1H");
    compute_wrap_positions(line_text, 38, char_to_row, char_to_col);
    print_wrapped(line_text, 38, blanks, blank_count);
    printf("\n\nTap a word to reroll it.\nA: confirm line   START: exit");
}

void start_new_line(cJSON *templates, cJSON *words, int template_count,
                     char **current_line_text, Blank *blanks, int *blank_count,
                     int *char_to_row, int *char_to_col) {
    if (*current_line_text != NULL) {
        free(*current_line_text);
    }

    cJSON *template = cJSON_GetArrayItem(templates, random_index(template_count));
    *current_line_text = generate_poem_line(template->valuestring, words, blanks, blank_count);

    redraw_current_line(*current_line_text, char_to_row, char_to_col, blanks, *blank_count);
}

int main(int argc, char **argv) {
    char *completed_lines[5];
    int current_line = 0;

    char *current_line_text = NULL;
    Blank current_blanks[20];
    int current_blank_count = 0;

    int char_to_row[512];
    int char_to_col[512];

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

    start_new_line(templates, words, template_count, &current_line_text,
                   current_blanks, &current_blank_count, char_to_row, char_to_col);

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();

        touchPosition touch;
        hidTouchRead(&touch);

        if (kDown & KEY_START) break;

        if (!poem_done) {
            if (kDown & KEY_TOUCH) {
                int tapped = find_tapped_blank(touch, current_blanks, current_blank_count, char_to_row, char_to_col);
                if (tapped != -1) {
                    current_line_text = reroll_word(current_line_text, current_blanks, current_blank_count, tapped, words);
                    redraw_current_line(current_line_text, char_to_row, char_to_col, current_blanks, current_blank_count);
                }
            }

            if (kDown & KEY_A) {
                completed_lines[current_line] = current_line_text;
                current_line_text = NULL;

                consoleSelect(&topScreen);
                print_wrapped(completed_lines[current_line], 48, current_blanks, current_blank_count);
                printf("\n\n");
                consoleSelect(&bottomScreen);

                current_line++;

                if (current_line < 5) {
                    start_new_line(templates, words, template_count, &current_line_text,
                                   current_blanks, &current_blank_count, char_to_row, char_to_col);
                } else {
                    poem_done = 1;
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