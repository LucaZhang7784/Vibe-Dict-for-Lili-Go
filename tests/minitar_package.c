/* Native minitar smoke test; output directory must be disposable. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <minitar.h>
int main(int argc, char **argv) {
    assert(argc == 3);
    struct minitar archive;
    struct minitar_entry entry;
    assert(minitar_open(argv[1], &archive) == 0);
    unsigned files = 0;
    while (minitar_read_entry(&archive, &entry) == 0) {
        assert(entry.metadata.path[0] != '/');
        assert(strstr(entry.metadata.path, "..") == NULL);
        if (entry.metadata.type == MTAR_DIRECTORY) continue;
        assert(entry.metadata.type == MTAR_REGULAR);
        char path[1024];
        int n = snprintf(path, sizeof(path), "%s/%s", argv[2], entry.metadata.path);
        assert(n > 0 && (size_t)n < sizeof(path));
        assert(minitar_read_contents_to_file(&archive, &entry, path));
        files++;
    }
    assert(minitar_close(&archive) == 0);
    assert(files == 9);
    puts("Native minitar: all 9 install files extracted successfully");
    return 0;
}
