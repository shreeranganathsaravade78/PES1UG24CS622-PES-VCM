#include "tree.h"
#include "index.h"
#include "pes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#define MODE_FILE 0100644
#define MODE_EXEC 0100755
#define MODE_DIR  0040000

int object_write(ObjectType type, const void *data, size_t len, ObjectID *id_out);

uint32_t get_file_mode(const char *path) {
    struct stat st;
    if (lstat(path, &st) != 0) return 0;

    if (S_ISDIR(st.st_mode)) return MODE_DIR;
    if (st.st_mode & S_IXUSR) return MODE_EXEC;
    return MODE_FILE;
}

int tree_parse(const void *data, size_t len, Tree *tree_out) {
    tree_out->count = 0;

    const uint8_t *ptr = data;
    const uint8_t *end = ptr + len;

    while (ptr < end && tree_out->count < MAX_TREE_ENTRIES) {
        TreeEntry *entry = &tree_out->entries[tree_out->count];

        const uint8_t *space = memchr(ptr, ' ', end - ptr);
        if (!space) return -1;

        char mode_str[16] = {0};
        size_t mode_len = space - ptr;
        if (mode_len >= sizeof(mode_str)) return -1;

        memcpy(mode_str, ptr, mode_len);
        entry->mode = strtol(mode_str, NULL, 8);

        ptr = space + 1;

        const uint8_t *null_byte = memchr(ptr, '\0', end - ptr);
        if (!null_byte) return -1;

        size_t name_len = null_byte - ptr;
        if (name_len >= sizeof(entry->name)) return -1;

        memcpy(entry->name, ptr, name_len);
        entry->name[name_len] = '\0';

        ptr = null_byte + 1;

        if (ptr + HASH_SIZE > end) return -1;

        memcpy(entry->hash.hash, ptr, HASH_SIZE);
        ptr += HASH_SIZE;

        tree_out->count++;
    }

    return 0;
}

static int compare_tree_entries(const void *a, const void *b) {
    return strcmp(((TreeEntry *)a)->name, ((TreeEntry *)b)->name);
}

int tree_serialize(const Tree *tree, void **data_out, size_t *len_out) {
    size_t max_size = tree->count * 300;
    uint8_t *buffer = malloc(max_size);
    if (!buffer) return -1;

    Tree sorted = *tree;
    qsort(sorted.entries, sorted.count, sizeof(TreeEntry), compare_tree_entries);

    size_t offset = 0;

    for (int i = 0; i < sorted.count; i++) {
        const TreeEntry *e = &sorted.entries[i];

        int written = sprintf((char *)buffer + offset, "%o %s", e->mode, e->name);
        offset += written;

        buffer[offset++] = '\0';

        memcpy(buffer + offset, e->hash.hash, HASH_SIZE);
        offset += HASH_SIZE;
    }

    *data_out = buffer;
    *len_out = offset;
    return 0;
}

static int write_tree_recursive(IndexEntry *entries, int count, int depth, ObjectID *id_out) {
    Tree tree;
    tree.count = 0;

    for (int i = 0; i < count;) {
        char *path = entries[i].path;
        char *start = path;

        for (int d = 0; d < depth; d++) {
            char *next = strchr(start, '/');
            if (next) start = next + 1;
        }

        char *slash = strchr(start, '/');

        if (slash) {
            size_t len = slash - start;
            char dir[256];

            if (len >= sizeof(dir)) len = sizeof(dir) - 1;

            memcpy(dir, start, len);
            dir[len] = '\0';

            int j = i + 1;

            while (j < count) {
                char *sub = entries[j].path;

                for (int d = 0; d < depth; d++) {
                    char *n = strchr(sub, '/');
                    if (n) sub = n + 1;
                }

                if (strncmp(sub, dir, len) == 0 && sub[len] == '/')
                    j++;
                else
                    break;
            }

            ObjectID sub_id;

            if (write_tree_recursive(&entries[i], j - i, depth + 1, &sub_id) != 0)
                return -1;

            TreeEntry *te = &tree.entries[tree.count++];
            te->mode = MODE_DIR;

            size_t name_len = strlen(dir);
            if (name_len >= sizeof(te->name)) name_len = sizeof(te->name) - 1;
            memcpy(te->name, dir, name_len);
            te->name[name_len] = '\0';

            te->hash = sub_id;

            i = j;

        } else {
            TreeEntry *te = &tree.entries[tree.count++];

            te->mode = entries[i].mode;

            size_t name_len = strlen(start);
            if (name_len >= sizeof(te->name)) name_len = sizeof(te->name) - 1;
            memcpy(te->name, start, name_len);
            te->name[name_len] = '\0';

            te->hash = entries[i].hash;

            i++;
        }
    }

    void *data = NULL;
    size_t len = 0;

    if (tree_serialize(&tree, &data, &len) != 0)
        return -1;

    if (object_write(OBJ_TREE, data, len, id_out) != 0) {
        free(data);
        return -1;
    }

    free(data);
    return 0;
}

int tree_from_index(ObjectID *id_out) {
    Index idx;

    if (index_load(&idx) != 0)
        return -1;

    if (idx.count == 0)
        return -1;

    return write_tree_recursive(idx.entries, idx.count, 0, id_out);
}
