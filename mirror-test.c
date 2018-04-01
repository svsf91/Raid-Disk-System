#include "blkdev.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>

/* Write some data to an area of memory */
void write_data(char *data, int length)
{
    for (int i = 0; i < length; i++)
    {
        data[i] = (char)i;
    }
}

/* Create a new file ready to be used as an image. Every byte of the file will be zero. */
struct blkdev *create_new_image(char *path, int blocks)
{
    if (blocks < 1)
    {
        printf("create_new_image: error - blocks must be at least 1: %d\n", blocks);
        return NULL;
    }
    FILE *image = fopen(path, "w");
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
void dump(char *buffer, int length, char *path)
{
    FILE *output = fopen(path, "w");
    fwrite(buffer, 1, length, output);
    fclose(output);
}

int main()
{
    struct blkdev *mirror_drives[2];
    /* Create two images for the mirror */
    mirror_drives[0] = create_new_image("mirror1", 2);
    mirror_drives[1] = create_new_image("mirror2", 2);
    /* Create the raid mirror */
    struct blkdev *mirror = mirror_create(mirror_drives);

    /* Write some data to the mirror, then read the data back and check that the
     * two buffers contain the same bytes.
     */
    char write_buffer[BLOCK_SIZE];
    write_data(write_buffer, BLOCK_SIZE);
    if (blkdev_write(mirror, 0, 1, write_buffer) != SUCCESS)
    {
        printf("Write failed!\n");
        exit(0);
    }

    char read_buffer[BLOCK_SIZE];
    /* Zero out the buffer to make sure blkdev_read() actually does something */
    bzero(read_buffer, BLOCK_SIZE);

    if (blkdev_read(mirror, 0, 1, read_buffer) != SUCCESS)
    {
        printf("Read failed!\n");
        exit(0);
    }

    /* For debugging, you can analyze these files manually */
    dump(write_buffer, BLOCK_SIZE, "write-buffer");
    dump(read_buffer, BLOCK_SIZE, "read-buffer");

    if (memcmp(write_buffer, read_buffer, BLOCK_SIZE) != 0)
    {
        printf("Read doesn't match write!\n");
    }
    else
    {
        printf("Mirror test passed\n");
    }

    /* Your tests here */
    char buffer0[BLOCK_SIZE * 2];
    char buffer1[BLOCK_SIZE * 2];
    srand(time(NULL));
    write_data(buffer1, 2 * BLOCK_SIZE);
    assert(blkdev_write(mirror, 0, 2, buffer1) == SUCCESS);
    assert(mirror != NULL);
    int num_blocks = mirror->ops->num_blocks(mirror);
    assert(num_blocks == mirror_drives[0]->ops->num_blocks(mirror_drives[0]));
    char backup[BLOCK_SIZE * num_blocks];
    char copy[BLOCK_SIZE * num_blocks];
    bzero(backup, BLOCK_SIZE * num_blocks);
    bzero(copy, BLOCK_SIZE * num_blocks);
    //overwrite mirror with 0
    assert(blkdev_write(mirror, 0, num_blocks, backup) == SUCCESS);
    for (int i = 0; i < 7 * num_blocks; i++)
    {
        int start = rand() % num_blocks;
        assert(blkdev_write(mirror, start, 1, write_buffer) == SUCCESS);
        memcpy(backup + start * BLOCK_SIZE, write_buffer, BLOCK_SIZE);
        assert(blkdev_read(mirror, start, 1, read_buffer) == SUCCESS);
        assert(memcmp(write_buffer, read_buffer, BLOCK_SIZE) == 0);
        bzero(read_buffer, BLOCK_SIZE);
    }
    assert(blkdev_read(mirror, 0, num_blocks, copy) == SUCCESS);
    dump(copy, BLOCK_SIZE * num_blocks, "copy");
    dump(backup, BLOCK_SIZE * num_blocks, "backup");
    assert(memcmp(backup, copy, BLOCK_SIZE * num_blocks) == 0);

    //when one of the disks fail
    image_fail(mirror_drives[0]);
    assert(blkdev_read(mirror_drives[0], 0, 2, buffer0) != SUCCESS);
    assert(blkdev_read(mirror, 0, 2, buffer0) == SUCCESS);
    assert(memcmp(buffer0, buffer1, BLOCK_SIZE * 2) == 0);

    //replace
    struct blkdev *new_disk = create_new_image("new_disk", 2);
    assert(mirror_replace(mirror, 0, new_disk) == SUCCESS);
    assert(blkdev_read(new_disk, 0, 2, buffer0) == SUCCESS);
    assert(blkdev_read(mirror_drives[1], 0, 2, buffer1) == SUCCESS);
    assert(memcmp(buffer0, buffer1, BLOCK_SIZE * 2) == 0);

    printf("Mirror test passed\n\n");
}
