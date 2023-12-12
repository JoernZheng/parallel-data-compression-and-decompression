// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>

// #define MAX_LINE_LENGTH 256

// // Node structure for the hashmap
// struct Node {
//     char key[MAX_LINE_LENGTH];
//     char value[MAX_LINE_LENGTH];
//     struct Node* next;
// };

// // Function to create a new node
// struct Node* createNode(const char* key, const char* value) {
//     struct Node* newNode = (struct Node*)malloc(sizeof(struct Node));
//     strcpy(newNode->key, key);
//     strcpy(newNode->value, value);
//     newNode->next = NULL;
//     return newNode;
// }

// // Function to insert a node into the hashmap
// void insertNode(struct Node* hashmap[], const char* key, const char* value) {
//     // Hashing function (simple example, can be improved)
//     unsigned int index = 0;
//     for (size_t i = 0; i < strlen(key); ++i) {
//         index += key[i];
//     }
//     index %= MAX_LINE_LENGTH;

//     // Insert the node at the beginning of the linked list
//     struct Node* newNode = createNode(key, value);
//     newNode->next = hashmap[index];
//     hashmap[index] = newNode;
// }

// // Function to print the hashmap
// void printHashmap(struct Node* hashmap[]) {
//     for (int i = 0; i < MAX_LINE_LENGTH; ++i) {
//         struct Node* current = hashmap[i];
//         while (current != NULL) {
//             printf("File: %s, Path: %s\n", current->key, current->value);
//             current = current->next;
//         }
//     }
// }

// // Function to free the memory allocated for the hashmap
// void freeHashmap(struct Node* hashmap[]) {
//     for (int i = 0; i < MAX_LINE_LENGTH; ++i) {
//         struct Node* current = hashmap[i];
//         while (current != NULL) {
//             struct Node* temp = current;
//             current = current->next;
//             free(temp);
//         }
//     }
// }

// int main() {
//     FILE* file = fopen("file_list.txt", "r");
//     if (file == NULL) {
//         fprintf(stderr, "Error opening file.\n");
//         return 1;
//     }

//     // Create a hashmap (array of linked lists)
//     // struct Node* hashmap[MAX_LINE_LENGTH] = {NULL};
//     struct HashMap filePathMap = createHashMap(10);

//     char line[MAX_LINE_LENGTH];
//     while (fgets(line, sizeof(line), file) != NULL) {
//         // Remove trailing newline character
//         line[strcspn(line, "\n")] = '\0';

//         // Split the line into path and file name
//         *(strrchr(line, '(') - 1) = '\0';
//         char* fileName = strrchr(line, '/') + 1;
//         *strrchr(line, '/') = '\0';

//         // Insert the entry into the hashmap
//         // insertNode(hashmap, fileName, line);
//         filePathMap
//         insert(filePathMap, fileName, line);
//         destroyHashMap(filePathMap);
//     }

//     fclose(file);

//     // Print the resulting hashmap
//     printHashmap(hashmap);

//     // Free allocated memory
//     freeHashmap(hashmap);

//     return 0;
// }
