#include <stdio.h>
#include <stdlib.h>
#include "cJSON.h"

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

int main() {
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
    printf("File contents:\n%s\n", contents);
    printf("Parsed successfully!\n");

    cJSON *templates = cJSON_GetObjectItem(root, "templates");
    if (templates == NULL) {
        printf("Error: no 'templates' key found\n");
        return 1;
    }

    int template_count = cJSON_GetArraySize(templates);
    printf("Found %d templates\n", template_count);

    cJSON *first_template = cJSON_GetArrayItem(templates, 0);
    printf("First template: %s\n", first_template->valuestring);

    cJSON *words = cJSON_GetObjectItem(root, "words");
    if (words == NULL) {
        printf("Error: no 'words' found \n");
        return 1;
    }

    cJSON *nouns = cJSON_GetObjectItem(words, "noun");
    if (nouns == NULL) {
        printf("Error: no 'noun' key found\n");
        return 1;
    }

    int nouns_count = cJSON_GetArraySize(nouns);
    cJSON *first_noun = cJSON_GetArrayItem(nouns, 0);

    printf("Found %d nouns\n", nouns_count);
    printf("First noun: %s\n", first_noun->valuestring);



    free(contents);   // we malloc'd it, so we must free it — otherwise it's a memory leak
    return 0;


}