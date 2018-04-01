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

/* Write a buffer to a file for debugging purposes */
void dump(char* buffer, int length, char* path){
    FILE * output = fopen(path, "w");
    fwrite(buffer, 1, length, output);
    fclose(output);
}

int main() {
    // Passes all other tests with different strip sizes (e.g. 2, 4, 7, and 32 sectors) 
    // and different numbers of disks.
    int units[] = {2, 4, 7, 32};
    int ndisks[] = {5, 7, 11};
    char *img_names[11] = {
        "test1","test2","test3","test4","test5","test6","test7","test8","test9","test10","test11"
    };
    struct blkdev *raid4;
    srand(time(NULL));
    int unit, ndisk, num_blocks;

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 3; j++) {
            unit = units[i];
            ndisk = ndisks[j];

            // create disks
            struct blkdev *disks[ndisk];
            for (int k = 0; k < ndisk; k++) {
                disks[k] = create_new_image(img_names[k], 32);
            }

            // create raid4
            raid4 = raid4_create(ndisk, disks, unit);
            num_blocks = blkdev_num_blocks(raid4);

            // reports the correct size
            assert(raid4 != NULL);
            assert(num_blocks == blkdev_num_blocks(disks[0]) / unit * unit * (ndisk - 1));

           // create buffer
            char read_buf[BLOCK_SIZE * 2];
            char write_buf[BLOCK_SIZE * 2];
            char backup[BLOCK_SIZE * num_blocks];
            char copy[BLOCK_SIZE * num_blocks];
            bzero(backup, BLOCK_SIZE * num_blocks);
            bzero(copy, BLOCK_SIZE * num_blocks);

            write_data(write_buf, BLOCK_SIZE);
            //overwrite raid0 with 0
            assert(blkdev_write(raid4, 0, num_blocks, backup) == SUCCESS);
            assert(blkdev_read(raid4, 0, num_blocks, copy) == SUCCESS);
            // reads data from the right disks and locations. 
            // overwrites the correct locations. 
            for(int i = 0; i < 7 * num_blocks; i++) {
                int start = rand() % num_blocks;
                assert(blkdev_write(raid4, start, 1, write_buf) == SUCCESS);
                memcpy(backup + start * BLOCK_SIZE, write_buf, BLOCK_SIZE);
                assert(memcmp(backup + start * BLOCK_SIZE, write_buf, BLOCK_SIZE) == 0);
                assert(blkdev_read(raid4, start, 1, read_buf) == SUCCESS);
                assert(memcmp(write_buf, read_buf, BLOCK_SIZE) == 0);
                bzero(read_buf, BLOCK_SIZE);
            }
            assert(blkdev_read(raid4, 0, num_blocks, copy) == SUCCESS);
            assert(memcmp(backup, copy, BLOCK_SIZE * num_blocks) == 0);

            // fail a disk and verify that the volume doesn't fail.
            image_fail(disks[0]);

            bzero(backup, BLOCK_SIZE * num_blocks);
            bzero(copy, BLOCK_SIZE * num_blocks);
            assert(blkdev_write(raid4, 0, num_blocks, backup) == SUCCESS);
            for(int i = 0; i < num_blocks; i++) {
                int start = rand() % num_blocks;
                assert(blkdev_write(raid4, start, 1, write_buf) == SUCCESS);
                memcpy(backup + start * BLOCK_SIZE, write_buf, BLOCK_SIZE);
                assert(memcmp(backup + start * BLOCK_SIZE, write_buf, BLOCK_SIZE) == 0);
                assert(blkdev_read(raid4, start, 1, read_buf) == SUCCESS);
                assert(memcmp(write_buf, read_buf, BLOCK_SIZE) == 0);
                bzero(read_buf, BLOCK_SIZE);
            }
            assert(blkdev_read(raid4, 0, num_blocks, copy) == SUCCESS);            
            assert(memcmp(backup, copy, BLOCK_SIZE * num_blocks) == 0);

            assert(blkdev_read(raid4, 0, num_blocks, copy) == SUCCESS);
            assert(memcmp(backup, copy, BLOCK_SIZE * num_blocks) == 0);

            assert(blkdev_write(raid4, 0, 2, write_buf) == SUCCESS);
            assert(blkdev_read(raid4, 0, 2, read_buf) == SUCCESS);
            dump(read_buf, BLOCK_SIZE, "read-buf");
            dump(write_buf, BLOCK_SIZE, "write-buf");
            assert(memcmp(write_buf, read_buf, BLOCK_SIZE) == 0);
            bzero(read_buf, BLOCK_SIZE);

            //test replace
            struct blkdev *newdisk = create_new_image("new disk", 32);
            assert(raid4_replace(raid4, 0, newdisk) == SUCCESS);
            bzero(backup, BLOCK_SIZE * num_blocks);
            bzero(copy, BLOCK_SIZE * num_blocks);
            assert(blkdev_write(raid4, 0, num_blocks, backup) == SUCCESS);
            for(int i = 0; i < num_blocks; i++) {
                int start = rand() % num_blocks;
                assert(blkdev_write(raid4, start, 1, write_buf) == SUCCESS);
                memcpy(backup + start * BLOCK_SIZE, write_buf, BLOCK_SIZE);
                assert(memcmp(backup + start * BLOCK_SIZE, write_buf, BLOCK_SIZE) == 0);
                assert(blkdev_read(raid4, start, 1, read_buf) == SUCCESS);
                assert(memcmp(write_buf, read_buf, BLOCK_SIZE) == 0);
                bzero(read_buf, BLOCK_SIZE);
            }
            assert(blkdev_read(raid4, 0, num_blocks, copy) == SUCCESS);            
            assert(memcmp(backup, copy, BLOCK_SIZE * num_blocks) == 0);

            // fail another disk
            image_fail(disks[1]);
            assert(blkdev_write(raid4, unit - 1, 2, write_buf) != SUCCESS);
            assert(blkdev_read(raid4, unit - 1, 2, read_buf) != SUCCESS);

            // close
            blkdev_close(raid4);
            printf("Raid4 test stripe size: %d, disk number: %d passed.\n", unit, ndisk);
        }
    }

    printf("raid4 test passed\n");
}
