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
#define HIGHLIGHT_PAD_X 2.5f
#define HIGHLIGHT_PAD_Y 0.0f
#define GLYPH_HEIGHT 17.0f

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

// Generic lookup: works for ANY category name that matches a key in your
// JSON's "words" object -- no need to edit this when you add new categories
// like "place", "people", "ingverb", etc. The category string from a
// template blank (e.g. "{things}") is used directly as the JSON key.
char *get_random_word(const char *category, cJSON *words) {
    cJSON *array = cJSON_GetObjectItem(words, category);

    if (array == NULL) {
        return NULL;
    }

    int count = cJSON_GetArraySize(array);
    cJSON *word = cJSON_GetArrayItem(array, random_index(count));
    return word->valuestring;
}

// NOTE: word bank entries can be multi-word phrases by using an
// underscore instead of a space (e.g. "old_house" in poetic_data.json).
// generate_poem_line and reroll_word treat underscores as ordinary
// characters (part of one solid "word"), so all existing wrap/highlight/
// touch logic keeps working unchanged. The underscore is only converted
// back to a real space at the final drawing step, in draw_poem_line.
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
            if (word == NULL) {
                // Unknown category (typo in template, or missing JSON key) --
                // fall back to just showing the category name itself so the
                // app doesn't crash, and the mistake is visible on screen.
                word = category;
            }

            int already_used = 1;
            while (already_used) {
                already_used = 0;
                for (int k = 0; k < used_count; k++) {
                    if (strcmp(word, used_words[k]) == 0) {
                        already_used = 1;
                        char *retry = get_random_word(category, words);
                        if (retry == NULL) break;
                        word = retry;
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
    if (new_word == NULL) {
        return output; // unknown category, nothing to reroll to
    }

    int tries = 0;
    while (strcmp(new_word, old_word) == 0 && tries < 30) {
        new_word = get_random_word(old_category, words);
        tries++;
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

// ==================== CATEGORY HIGHLIGHT COLORS ====================
// This is the ONLY place you need to touch when you add a new word
// category to poetic_data.json. Just add another "if (strcmp(...))"
// line below, matching the category name exactly as it appears in your
// JSON, and pick any C2D_Color32(r, g, b, 255).
u32 category_color(const char *category) {
    if (strcmp(category, "adjective") == 0) return C2D_Color32(160, 40, 160, 255);  // magenta
    if (strcmp(category, "things") == 0)    return C2D_Color32(40, 160, 70, 255);   // green
    if (strcmp(category, "verb") == 0)      return C2D_Color32(40, 70, 200, 255);   // blue
    if (strcmp(category, "place") == 0)     return C2D_Color32(200, 120, 30, 255);  // orange
    if (strcmp(category, "people") == 0)    return C2D_Color32(200, 30, 60, 255);   // red
    if (strcmp(category, "_to") == 0)       return C2D_Color32(30, 160, 160, 255);  // teal
    if (strcmp(category, "ingverb") == 0)   return C2D_Color32(140, 140, 30, 255);  // olive
    if (strcmp(category, "edverb") == 0)    return C2D_Color32(90, 60, 160, 255);   // violet

    // Example for adding a new category:
    // if (strcmp(category, "yourcategory") == 0) return C2D_Color32(r, g, b, 255);

    return C2D_Color32(90, 90, 90, 255); // fallback color for any unrecognized category
}
// =====================================================================

int blank_at_position(Blank *blanks, int blank_count, int pos) {
    for (int b = 0; b < blank_count; b++) {
        if (blanks[b].start_pos == pos) {
            return b;
        }
    }
    return -1;
}

typedef struct {
    int start;
    int len;
} WordSpan;

int split_words(const char *text, WordSpan *out_words, int max_words) {
    int len = strlen(text);
    int i = 0, count = 0;
    while (i < len && count < max_words) {
        int start = i;
        while (i < len && text[i] != ' ') i++;
        out_words[count].start = start;
        out_words[count].len = i - start;
        count++;
        if (i < len && text[i] == ' ') i++;
    }
    return count;
}

// Greedily assigns each word to a row, then rebalances: if the last row
// has fewer than min_last_row_words words, pull the last word of the
// previous row down onto it -- but only if it still fits within
// max_width_chars. Repeats until satisfied or no more moves are possible.
int wrap_words_balanced(WordSpan *words, int word_count, int max_width_chars,
                         int min_last_row_words, int *word_row) {
    int row = 0, col = 0;
    for (int w = 0; w < word_count; w++) {
        int wlen = words[w].len;
        if (col > 0 && col + 1 + wlen > max_width_chars) {
            row++;
            col = 0;
        } else if (col > 0) {
            col++;
        }
        col += wlen;
        word_row[w] = row;
    }
    int total_rows = row + 1;

    if (total_rows <= 1) return total_rows;

    int safety = 0;
    while (safety < 20) {
        safety++;
        int last_row = total_rows - 1;
        int count_last = 0;
        for (int w = 0; w < word_count; w++) if (word_row[w] == last_row) count_last++;
        if (count_last >= min_last_row_words) break;

        int prev_row = last_row - 1;
        if (prev_row < 0) break;

        int candidate = -1;
        for (int w = word_count - 1; w >= 0; w--) {
            if (word_row[w] == prev_row) { candidate = w; break; }
        }
        if (candidate == -1) break;

        int last_row_width = 0;
        int first = 1;
        for (int w = 0; w < word_count; w++) {
            if (word_row[w] == last_row) {
                if (!first) last_row_width += 1;
                last_row_width += words[w].len;
                first = 0;
            }
        }
        int new_width = words[candidate].len + (last_row_width > 0 ? 1 : 0) + last_row_width;
        if (new_width > max_width_chars) break;

        word_row[candidate] = last_row;

        int prev_count = 0;
        for (int w = 0; w < word_count; w++) if (word_row[w] == prev_row) prev_count++;
        if (prev_count == 0) {
            for (int w = 0; w < word_count; w++) {
                if (word_row[w] > prev_row) word_row[w]--;
            }
            total_rows--;
        }
    }

    return total_rows;
}

int count_wrapped_rows(const char *text, int max_width_chars) {
    WordSpan words[60];
    int word_count = split_words(text, words, 60);
    int word_row[60];
    return wrap_words_balanced(words, word_count, max_width_chars, 3, word_row);
}

int draw_poem_line(const char *text, Blank *blanks, int blank_count,
                    C2D_Font fontReg, C2D_Font fontItal, C2D_TextBuf textBuf,
                    int max_width_chars, float screen_width, float left_margin,
                    int centered, float start_y,
                    int *char_to_row, int *char_to_col, float *row_x_offset) {
    WordSpan words[60];
    int word_count = split_words(text, words, 60);

    int word_row[60];
    int total_rows = wrap_words_balanced(words, word_count, max_width_chars, 3, word_row);

    int word_col[60];
    int row_char_count[MAX_ROWS];
    for (int r = 0; r < MAX_ROWS; r++) row_char_count[r] = 0;

    for (int r = 0; r < total_rows; r++) {
        int col = 0;
        int first = 1;
        for (int w = 0; w < word_count; w++) {
            if (word_row[w] == r) {
                if (!first) col++;
                word_col[w] = col;
                col += words[w].len;
                first = 0;
            }
        }
        row_char_count[r] = col;
    }

    for (int r = 0; r < total_rows; r++) {
        float row_width_px = row_char_count[r] * CHAR_WIDTH;
        row_x_offset[r] = centered ? (screen_width - row_width_px) / 2.0f : left_margin;
    }

    for (int w = 0; w < word_count; w++) {
        for (int j = 0; j < words[w].len; j++) {
            char_to_row[words[w].start + j] = word_row[w];
            char_to_col[words[w].start + j] = word_col[w] + j;
        }
    }

    for (int w = 0; w < word_count; w++) {
        int r = word_row[w];
        int c = word_col[w];
        float x = row_x_offset[r] + c * CHAR_WIDTH;
        float y = start_y + r * LINE_HEIGHT;

        char word_buf[60];
        int wlen = words[w].len;
        strncpy(word_buf, text + words[w].start, wlen);
        word_buf[wlen] = '\0';

        // Convert any underscores (used to mark multi-word phrases in the
        // word bank, e.g. "old_house") back into real spaces ONLY here,
        // right before drawing. All wrap/highlight/touch math above this
        // point still sees "old_house" as one solid word of length 9.
        for (int k = 0; k < wlen; k++) {
            if (word_buf[k] == '_') word_buf[k] = ' ';
        }

        int blank_idx = blank_at_position(blanks, blank_count, words[w].start);
        C2D_Font use_font = fontReg;

        if (blank_idx != -1) {
            use_font = fontItal;
            int highlight_len = blanks[blank_idx].length;
            float rect_w = highlight_len * CHAR_WIDTH + HIGHLIGHT_PAD_X * 2.0f;
            float rect_h = GLYPH_HEIGHT + HIGHLIGHT_PAD_Y * 2.0f;
            float rect_y = y + (LINE_HEIGHT - GLYPH_HEIGHT) / 2.0f - HIGHLIGHT_PAD_Y;
            C2D_DrawRectSolid(x - HIGHLIGHT_PAD_X, rect_y, 0.4f, rect_w, rect_h,
                               category_color(blanks[blank_idx].category));
        }

        C2D_Text wordText;
        C2D_TextFontParse(&wordText, use_font, textBuf, word_buf);
        C2D_TextOptimize(&wordText);
        C2D_DrawText(&wordText, C2D_WithColor, x, y, 0.5f, FONT_SCALE, FONT_SCALE,
                     C2D_Color32(255, 255, 255, 255));
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

void draw_instructions(C2D_Font font, C2D_TextBuf textBuf, const char *msg, float y) {
    C2D_Text text;
    C2D_TextFontParse(&text, font, textBuf, msg);
    C2D_TextOptimize(&text);
    float width, height;
    C2D_TextGetDimensions(&text, FONT_SCALE, FONT_SCALE, &width, &height);
    float x = (320.0f - width) / 2.0f;
    C2D_DrawText(&text, C2D_WithColor, x, y, 0.5f, FONT_SCALE, FONT_SCALE,
                 C2D_Color32(160, 160, 160, 255));
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

        if (kDown & KEY_START) {
            for (int l = 0; l < current_line; l++) {
                free(completed_lines[l]);
            }
            if (current_line_text != NULL) {
                free(current_line_text);
            }

            current_line = 0;
            poem_done = 0;

            cJSON *t = cJSON_GetArrayItem(templates, random_index(template_count));
            current_line_text = generate_poem_line(t->valuestring, words, current_blanks, &current_blank_count);
        }

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
            float total_height = 0.0f;
            for (int l = 0; l < current_line; l++) {
                int rows = count_wrapped_rows(completed_lines[l], 52);
                total_height += rows * LINE_HEIGHT;
                if (l < current_line - 1) total_height += LINE_GAP;
            }

            float y = (240.0f - total_height) / 2.0f;
            if (y < 10.0f) y = 10.0f;

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

        draw_instructions(fontReg, textBuf, "A: confirm line   START: new poem", 210.0f);

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