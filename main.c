#include <stdio.h>
#include <stdlib.h>
#include "cJSON.h"
#include <time.h>
#include <string.h>
#include <ctype.h>

char* read_file(const char* filename) {
    FILE *fp = fopen(filename, "r");   // open the file for reading ("r" = read mode)
    if (fp == NULL) {                  // fopen returns NULL if it couldn't find/open the file
        printf("Error: could not open file %s\n", filename);
        return NULL;
    }

    fseek(fp, 0, SEEK_END);   // move the "cursor" to the end of the file
    long size = ftell(fp);    // ask "what position is the cursor at?" — that's the file size in bytes
    rewind(fp);                // move the cursor back to the start, ready to actually read

    char *buffer = malloc(size + 1);   // allocate enough memory to hold the file, +1 for a null terminator
    if (buffer == NULL) {
        printf("Error: memory allocation failed\n");
        fclose(fp);
        return NULL;
    }

    fread(buffer, 1, size, fp);   // read `size` bytes from the file into buffer
    buffer[size] = '\0';          // manually add the null terminator so C knows where the string ends

    fclose(fp);   // done with the file, close it
    return buffer;   // hand back the string (caller is responsible for freeing it later)
}

int random_index(int array_size) {
    return rand() % array_size;
}

char *test_copy(const char *input) {
    char *output = malloc(strlen(input) + 1);
    int out_pos = 0;

    for(int i = 0; i < strlen(input); i++) {
        output[out_pos] = input[i];
        out_pos++;
    }

    output[out_pos] = '\0';

    return output;
}

char *get_random_word(const char *category, cJSON *words){
    cJSON *array = NULL;

    if (strcmp(category, "noun") == 0) {
        array = cJSON_GetObjectItem(words, "noun");
    } else if (strcmp(category, "adjective") == 0) {
        array = cJSON_GetObjectItem(words, "adjective");
    } else if (strcmp(category, "verb") == 0) {
        array = cJSON_GetObjectItem(words, "verb");
    }

    if(array == NULL) {
        printf("Error: unknown category '%s'\n", category);
        return NULL;
    }

    int count = cJSON_GetArraySize(array);
    cJSON *word = cJSON_GetArrayItem(array, random_index(count));
    return word->valuestring;

}

typedef struct {
    char category[20];
    char word[50];
    int start_pos;
    int length;
} Blank;

char *generate_poem_line(const char *template_str, cJSON *words, Blank *blanks, int *blank_count) {
    int template_len = strlen(template_str);
    char *output = malloc(512);
    int out_pos = 0;

    char *used_words[20];
    int used_count = 0;

    *blank_count = 0;  // start at zero blanks found

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
            int word_start_pos = out_pos;   // remember where this word starts, BEFORE copying it in

            for (int j = 0; j < word_len; j++) {
                output[out_pos] = word[j];
                out_pos++;
            }

            // record this blank's info
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

int is_vowel(char c) {
    c = tolower(c);
    return (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' || c == 'y');
}

int count_syllables(const char *word) {
    int len = strlen(word);
    int count = 0;
    int prev_was_vowel = 0;  // 0 = false, 1 = true

    for (int i = 0; i < len; i++) {
        int this_is_vowel = is_vowel(word[i]);

        if (this_is_vowel && !prev_was_vowel) {
            count++;  // start of a new vowel group = one syllable
        }

        prev_was_vowel = this_is_vowel;
    }

    // silent 'e' at the end usually doesn't add a syllable (e.g. "like", "gardens" -> "garden" + s doesn't apply here but "like" does)
    if (len > 2 && tolower(word[len - 1]) == 'e' && !is_vowel(word[len - 2])) {
        count--;
    }

    // every real word has at least 1 syllable, so never return 0
    if (count < 1) {
        count = 1;
    }

    return count;
}

char *reroll_word(char *output, Blank *blanks, int blank_count, int reroll_index, cJSON *words) {

    // check for bad index check
    if (reroll_index < 0 || reroll_index >= blank_count) {
        printf("Error: invalid reroll index %d\n", reroll_index);
        return output;  // hand back unchanged, do nothing
    }

    // 1. Grab the blank we're rerolling
    char old_category[20];
    strcpy(old_category, blanks[reroll_index].category);
    char old_word[50];
    strcpy(old_word, blanks[reroll_index].word);
    int start_pos = blanks[reroll_index].start_pos;
    int old_length = blanks[reroll_index].length;

    // 2. Pick a new word, different from the old one
    char *new_word = get_random_word(old_category, words);
    while (strcmp(new_word, old_word) == 0) {
        new_word = get_random_word(old_category, words);
    }
    int new_length = strlen(new_word);

    // 3. Figure out how much everything after this word needs to shift
    int diff = new_length - old_length;

    // 4. Shift everything AFTER the old word, to open/close the right amount of space
    int old_output_len = strlen(output);
    int tail_start = start_pos + old_length;          // where the "rest of the string" currently begins
    int tail_length = old_output_len - tail_start;      // how many characters are in that tail

    memmove(output + tail_start + diff, output + tail_start, tail_length + 1);
    // the "+1" includes the null terminator, so the string stays properly closed

    // 5. Copy the new word into the now-correctly-sized gap
    for (int j = 0; j < new_length; j++) {
        output[start_pos + j] = new_word[j];
    }

    // 6. Update this blank's own record
    strcpy(blanks[reroll_index].word, new_word);
    blanks[reroll_index].length = new_length;

    // 7. Update every LATER blank's start_pos, since the string shifted under them
    for (int b = 0; b < blank_count; b++) {
        if (b != reroll_index && blanks[b].start_pos > start_pos) {
            blanks[b].start_pos += diff;
        }
    }

    return output;
}

int main() {
    srand(time(NULL)); // call this once at the top of main everytime
    // rand() needs to be seeded once at the start of every program
    // or else it will be the same "random" every time you run the program

    char *contents = read_file("poetic_data.json");
    if (contents == NULL) {
        return 1;   // read_file already printed an error, just exit
    }

    cJSON *root = cJSON_Parse(contents);
    if (root == NULL) {
        printf("Error: JSON parse failed\n");
        free(contents);
        return 1;
    }
    //printf("File contents:\n%s\n", contents);
    //printf("Parsed successfully!\n");

    cJSON *templates = cJSON_GetObjectItem(root, "templates");
    if (templates == NULL) {
        printf("Error: no 'templates' key found\n");
        return 1;
    }

    int template_count = cJSON_GetArraySize(templates);
    //printf("Found %d templates\n", template_count);

    cJSON *words = cJSON_GetObjectItem(root, "words");
    if (words == NULL) {
        //printf("Error: no 'words' found \n");
        return 1;
    }
    for(int i = 0; i < 1; i++){
        cJSON *template = cJSON_GetArrayItem(templates, random_index(template_count));

        Blank blanks[20];
        int blank_count = 0;
        char *line = generate_poem_line(template->valuestring, words, blanks, &blank_count);
        printf("Before: %s\n", line);

        // simulate "tapping" blank #0
        line = reroll_word(line, blanks, blank_count, 5, words);
        printf("After rerolling blank 0: %s\n", line);

        free(line);
    }

    // printf("dreams: %d\n", count_syllables("dreams"));
    // printf("gardens: %d\n", count_syllables("gardens"));
    // printf("lightning: %d\n", count_syllables("lightning"));
    // printf("shy: %d\n", count_syllables("shy"));

    //cJSON *first_template = cJSON_GetArrayItem(templates, 0);
    //printf("First template: %s\n", first_template->valuestring);

    //char *copied = test_copy(first_template->valuestring);
    //printf("Copied template: %s\n", copied);
    //free(copied);


    // cJSON *nouns = cJSON_GetObjectItem(words, "noun");
    // if (nouns == NULL) {
    //     printf("Error: no 'noun' key found\n");
    //     return 1;
    // }

    // int nouns_count = cJSON_GetArraySize(nouns);
    // cJSON *first_noun = cJSON_GetArrayItem(nouns, 0);

    // printf("Found %d nouns\n", nouns_count);
    // printf("First noun: %s\n", first_noun->valuestring);

    // for(int i = 0; i < 5; i++){
    //     cJSON *rand_noun = cJSON_GetArrayItem(nouns, random_index(nouns_count));
    //     printf("%s\n", rand_noun->valuestring);
    // }

    // char *w1 = get_random_word("noun", words);
    // char *w2 = get_random_word("adjective", words);
    // char *w3 = get_random_word("verb", words);
    // printf("noun: %s, adjective: %s, verb: %s\n", w1, w2, w3);

    //char *poem_line = generate_poem_line(first_template->valuestring, words);
    //printf("Generate: %s\n", poem_line);
    //free(poem_line);

    free(contents);   // we malloc'd it, so we must free it — otherwise it's a memory leak
    return 0;

}