#include "process.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Node structure for the linked list in each hash table entry
// struct Node
// {
//     char *key;
//     char *value;
//     struct Node *next;
// };

// // Hash table structure
// struct HashMap
// {
//     int size;
//     struct Node **array;
// };

// Hash function
int  calculateHash(char *key, int table_size)
{
    int hash_value = 0;
    for (int i = 0; key[i] != '\0'; ++i)
    {
        hash_value += key[i];
    }
    return hash_value % table_size;
}

// Create a new node
struct Node *createNode(char *key, char *value)
{
    struct Node *newNode = (struct Node *)malloc(sizeof(struct Node));
    if (!newNode)
    {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }

    newNode->key = strdup(key);
    newNode->value = strdup(value);
    newNode->next = NULL;

    return newNode;
}

// Initialize a hash map
struct HashMap *createHashMap(int size)
{
    struct HashMap *map = (struct HashMap *)malloc(sizeof(struct HashMap));
    if (!map)
    {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }

    map->size = size;
    map->array = (struct Node **)malloc(sizeof(struct Node *) * size);
    if (!map->array)
    {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }

    // Initialize each entry in the hash table
    for (int i = 0; i < size; ++i)
    {
        map->array[i] = NULL;
    }

    return map;
}

// Insert a key-value pair into the hash map
void insert(struct HashMap *map, char *key, char *value)
{
    int index =  calculateHash(key, map->size);

    struct Node *newNode = createNode(key, value);
    newNode->next = map->array[index];
    map->array[index] = newNode;
}

// Search for a value based on the key
char *search(struct HashMap *map, char *key)
{
    int index =  calculateHash(key, map->size);

    struct Node *current = map->array[index];
    while (current != NULL)
    {
        if (strcmp(current->key, key) == 0)
        {
            return current->value;
        }
        current = current->next;
    }

    return NULL; // Key not found
}

// Free the memory allocated for the hash map
void destroyHashMap(struct HashMap *map)
{
    for (int i = 0; i < map->size; ++i)
    {
        struct Node *current = map->array[i];
        while (current != NULL)
        {
            struct Node *next = current->next;
            free(current->key);
            free(current->value);
            free(current);
            current = next;
        }
    }

    free(map->array);
    free(map);
}
