#include <3ds.h>
#include <citro2d.h>
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

#define FONT_SCALE 0.6f
#define CHAR_WIDTH 7.24f
#define LINE_HEIGHT 19.0f
#define LINE_GAP 5.0f
#define MAX_ROWS 8

char* read_file(const char* filename) {
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    rewind(fp);

    char *buffer = malloc(size + 1);
    if (buffer == NULL) {
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

u32 category_color(const char *category) {
    if (strcmp(category, "noun") == 0) return C2D_Color32(40, 160, 70, 255);
    if (strcmp(category, "adjective") == 0) return C2D_Color32(160, 40, 160, 255);
    if (strcmp(category, "verb") == 0) return C2D_Color32(40, 70, 200, 255);
    return C2D_Color32(255, 255, 255, 255);
}

int blank_at_position(Blank *blanks, int blank_count, int pos) {
    for (int b = 0; b < blank_count; b++) {
        if (blanks[b].start_pos == pos) {
            return b;
        }
    }
    return -1;
}

int draw_poem_line(const char *text, Blank *blanks, int blank_count,
                    C2D_Font fontReg, C2D_Font fontItal, C2D_TextBuf textBuf,
                    int max_width_chars, float screen_width, float left_margin,
                    int centered, float start_y,
                    int *char_to_row, int *char_to_col, float *row_x_offset) {
    int len = strlen(text);
    int i = 0;
    int row = 0;
    int col = 0;

    while (i < len) {
        int word_start = i;
        while (i < len && text[i] != ' ') {
            i++;
        }
        int word_len = i - word_start;

        if (col > 0 && col + 1 + word_len > max_width_chars) {
            row++;
            col = 0;
        } else if (col > 0) {
            col++;
        }

        for (int j = word_start; j < i; j++) {
            char_to_row[j] = row;
            char_to_col[j] = col;
            col++;
        }

        if (i < len && text[i] == ' ') {
            i++;
        }
    }

    int total_rows = row + 1;

    int row_char_count[MAX_ROWS];
    for (int r = 0; r < MAX_ROWS; r++) row_char_count[r] = 0;
    for (int j = 0; j < len; j++) {
        int r = char_to_row[j];
        int c = char_to_col[j];
        if (c + 1 > row_char_count[r]) row_char_count[r] = c + 1;
    }

    for (int r = 0; r < total_rows; r++) {
        float row_width_px = row_char_count[r] * CHAR_WIDTH;
        if (centered) {
            row_x_offset[r] = (screen_width - row_width_px) / 2.0f;
        } else {
            row_x_offset[r] = left_margin;
        }
    }

    i = 0;
    while (i < len) {
        int word_start = i;
        while (i < len && text[i] != ' ') {
            i++;
        }

        int r = char_to_row[word_start];
        int c = char_to_col[word_start];
        float x = row_x_offset[r] + c * CHAR_WIDTH;
        float y = start_y + r * LINE_HEIGHT;

        char word_buf[60];
        int wlen = i - word_start;
        strncpy(word_buf, text + word_start, wlen);
        word_buf[wlen] = '\0';

        int blank_idx = blank_at_position(blanks, blank_count, word_start);

        C2D_Font use_font = fontReg;
        u32 use_color = C2D_Color32(255, 255, 255, 255);

        if (blank_idx != -1) {
            use_font = fontItal;
            use_color = category_color(blanks[blank_idx].category);
        }

        C2D_Text wordText;
        C2D_TextFontParse(&wordText, use_font, textBuf, word_buf);
        C2D_TextOptimize(&wordText);
        C2D_DrawText(&wordText, C2D_WithColor, x, y, 0.5f, FONT_SCALE, FONT_SCALE, use_color);

        if (i < len && text[i] == ' ') {
            i++;
        }
    }

    return total_rows;
}

int find_tapped_blank(touchPosition touch, Blank *blanks, int blank_count,
                       int *char_to_row, int *char_to_col, float *row_x_offset,
                       float start_y) {
    int tapped_row = (int)((touch.py - start_y) / LINE_HEIGHT);

    for (int b = 0; b < blank_count; b++) {
        int blank_row = char_to_row[blanks[b].start_pos];
        if (blank_row != tapped_row) continue;

        int blank_col = char_to_col[blanks[b].start_pos];
        float blank_x_start = row_x_offset[blank_row] + blank_col * CHAR_WIDTH;
        float blank_x_end = blank_x_start + blanks[b].length * CHAR_WIDTH;

        if (touch.px >= blank_x_start && touch.px < blank_x_end) {
            return b;
        }
    }

    return -1;
}

int main(int argc, char **argv) {
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    C3D_RenderTarget *top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget *bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    C2D_Font fontReg = C2D_FontLoad("sdmc:/3ds/poeticware/spaceReg.bcfnt");
    C2D_Font fontItal = C2D_FontLoad("sdmc:/3ds/poeticware/spaceItal.bcfnt");

    C2D_TextBuf textBuf = C2D_TextBufNew(4096);

    srand(time(NULL));

    char *contents = read_file("sdmc:/3ds/poeticware/poetic_data.json");
    cJSON *root = cJSON_Parse(contents);
    cJSON *templates = cJSON_GetObjectItem(root, "templates");
    cJSON *words = cJSON_GetObjectItem(root, "words");
    int template_count = cJSON_GetArraySize(templates);

    char *completed_lines[5];
    Blank completed_blanks[5][20];
    int completed_blank_counts[5];
    int current_line = 0;

    char *current_line_text = NULL;
    Blank current_blanks[20];
    int current_blank_count = 0;

    int char_to_row[512];
    int char_to_col[512];
    float row_x_offset[MAX_ROWS];

    int poem_done = 0;

    cJSON *tmpl = cJSON_GetArrayItem(templates, random_index(template_count));
    current_line_text = generate_poem_line(tmpl->valuestring, words, current_blanks, &current_blank_count);

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        touchPosition touch;
        hidTouchRead(&touch);

        if (kDown & KEY_START) break;

        if (!poem_done) {
            if (kDown & KEY_TOUCH) {
                int tapped = find_tapped_blank(touch, current_blanks, current_blank_count,
                                                char_to_row, char_to_col, row_x_offset, 40.0f);
                if (tapped != -1) {
                    current_line_text = reroll_word(current_line_text, current_blanks,
                                                      current_blank_count, tapped, words);
                }
            }

            if (kDown & KEY_A) {
                completed_lines[current_line] = current_line_text;
                memcpy(completed_blanks[current_line], current_blanks, sizeof(current_blanks));
                completed_blank_counts[current_line] = current_blank_count;

                current_line_text = NULL;
                current_line++;

                if (current_line < 5) {
                    cJSON *t = cJSON_GetArrayItem(templates, random_index(template_count));
                    current_line_text = generate_poem_line(t->valuestring, words, current_blanks, &current_blank_count);
                } else {
                    poem_done = 1;
                }
            }
        }

        C2D_TextBufClear(textBuf);
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

        C2D_TargetClear(top, C2D_Color32(0, 0, 0, 255));
        C2D_SceneBegin(top);
        {
            float y = 15.0f;
            for (int l = 0; l < current_line; l++) {
                int rows_used = draw_poem_line(completed_lines[l], completed_blanks[l], completed_blank_counts[l],
                                                fontReg, fontItal, textBuf,
                                                52, 400.0f, 20.0f, 0, y,
                                                char_to_row, char_to_col, row_x_offset);
                y += rows_used * LINE_HEIGHT + LINE_GAP;
            }
        }

        C2D_TargetClear(bottom, C2D_Color32(0, 0, 0, 255));
        C2D_SceneBegin(bottom);
        if (!poem_done && current_line_text != NULL) {
            draw_poem_line(current_line_text, current_blanks, current_blank_count,
                            fontReg, fontItal, textBuf,
                            40, 320.0f, 0.0f, 1, 40.0f,
                            char_to_row, char_to_col, row_x_offset);
        }

        C3D_FrameEnd(0);
    }

    for (int l = 0; l < current_line; l++) {
        free(completed_lines[l]);
    }
    if (current_line_text != NULL) {
        free(current_line_text);
    }
    free(contents);

    C2D_TextBufDelete(textBuf);
    C2D_FontFree(fontReg);
    C2D_FontFree(fontItal);
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    return 0;
}