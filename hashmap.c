#include "process.h"

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
    if (map->usedSize == map->size) {
        resizeHashMap(map);
    }
    int index =  calculateHash(key, map->size);
    map->usedSize++;

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

// Resize the hash map
void resizeHashMap(struct HashMap *map)
{
    int new_size = map->size * HASHMAP_RESIZE_FACTOR;
    struct Node **new_array = (struct Node **)malloc(sizeof(struct Node *) * new_size);
    if (!new_array)
    {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }

    // Initialize each entry in the new hash table
    for (int i = 0; i < new_size; ++i)
    {
        new_array[i] = NULL;
    }

    // Rehash all existing elements and insert into the new hash map
    for (int i = 0; i < map->size; ++i)
    {
        struct Node *current = map->array[i];
        while (current != NULL)
        {
            int new_index = calculateHash(current->key, new_size);

            struct Node *new_node = createNode(current->key, current->value);
            new_node->next = new_array[new_index];
            new_array[new_index] = new_node;

            struct Node *temp = current;
            current = current->next;
            free(temp->key);
            free(temp->value);
            free(temp);
        }
    }

    // Free the old array
    free(map->array);

    // Update the hash map with the new array and size
    map->array = new_array;
    map->size = new_size;
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
