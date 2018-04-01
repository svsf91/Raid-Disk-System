#include "blkdev.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>

/* Write some data to an area of memory */
void write_data(char* data, int length){
    for (int i = 0; i < length; i++){
        data[i] = (char)i;
    }
}

/* Create a new file ready to be used as an image. Every byte of the file will be zero. */
struct blkdev *create_new_image(char * path, int blocks){
    if (blocks < 1){
        printf("create_new_image: error - blocks must be at least 1: %d\n", blocks);
        return NULL;
    }
    FILE * image = fopen(path, "w");
    /* This is a trick: instead of writing every byte from 0 to N we can instead move the file cursor
     * directly to N-1 and then write 1 byte. The filesystem will fill in the rest of the bytes with
     * zero for us.
     */
    fseek(image, blocks * BLOCK_SIZE - 1, SEEK_SET);
    char c = 0;
    fwrite(&c, 1, 1, image);
    fclose(image);

    return image_create(path);
}

void dump(char* buffer, int length, char* path){
    FILE * output = fopen(path, "w");
    fwrite(buffer, 1, length, output);
    fclose(output);
}

int main() {
    // Passes all other tests with different strip sizes (e.g. 2, 4, 7, and 32 sectors) 
    // and different numbers of disks.
    int units[] = {2, 4, 7, 32};
    int ndisks[] = {3, 7, 11};
    char *img_names[11] = {
        "test1","test2","test3","test4","test5",
        "test6","test7","test8","test9","test10","test11"
    };
    struct blkdev *raid0;

    // create rand()
    srand(time(NULL));

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 3; j++) {
            int unit = units[i];
            int ndisk = ndisks[j];

            // init
            struct blkdev *disks[ndisk];
            for (int k = 0; k < ndisk; k++) {
                disks[k] = create_new_image(img_names[k], 32);
            }
            raid0 = raid0_create(ndisk, disks, unit);

            // test num_blocks
            assert(raid0 != NULL);
            int num_blocks = blkdev_num_blocks(raid0);
            assert(num_blocks == blkdev_num_blocks(disks[0]) / unit * unit * ndisk);

            // create buffer
            char read_buf[BLOCK_SIZE];
            char write_buf[BLOCK_SIZE];
            char backup[BLOCK_SIZE * num_blocks];
            char copy[BLOCK_SIZE * num_blocks];
            
            //overwrite raid0 with 0
            assert(blkdev_write(raid0, 0, num_blocks, backup) == SUCCESS);

            // reads data from the right disks and locations. 
            // overwrites the correct locations. 
            
            for(int i = 0; i < num_blocks; i++) {
                int start = rand() % num_blocks;
                assert(blkdev_write(raid0, start, 1, write_buf) == SUCCESS);
                memcpy(backup + start * BLOCK_SIZE, write_buf, BLOCK_SIZE);
                assert(memcmp(backup + start * BLOCK_SIZE, write_buf, BLOCK_SIZE) == 0);
                assert(blkdev_read(raid0, start, 1, read_buf) == SUCCESS);
                assert(memcmp(write_buf, read_buf, BLOCK_SIZE) == 0);
                bzero(read_buf, BLOCK_SIZE);
            }
            assert(blkdev_read(raid0, 0, num_blocks, copy) == SUCCESS);
            dump(copy, BLOCK_SIZE * num_blocks, "copy");
            dump(backup, BLOCK_SIZE * num_blocks, "backup");
            assert(memcmp(backup, copy, BLOCK_SIZE * num_blocks) == 0);
            // fail a disk and verify that the volume fails.
            image_fail(disks[0]);
            assert(blkdev_write(raid0, 0, 1, write_buf) != SUCCESS);
            assert(blkdev_read(raid0, 0, 1, read_buf) != SUCCESS);

            // close
            blkdev_close(raid0);
            printf("Raid0 test stripe size: %d, disk number: %d passed.\n", unit, ndisk);
        }
    }

    printf("Raid0 test passed\n");
}
