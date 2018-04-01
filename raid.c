/*
 * file:        homework.c
 * description: skeleton code for CS 5600 Homework 2
 *
 * Peter Desnoyers, Northeastern Computer Science, 2011
 * $Id: homework.c 410 2011-11-07 18:42:45Z pjd $
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "blkdev.h"

/********** MIRRORING ***************/

/* example state for mirror device. See mirror_create for how to
 * initialize a struct blkdev with this.
 */
struct mirror_dev
{
    struct blkdev *disks[2]; /* flag bad disk by setting to NULL */
    int nblks;
};

static int mirror_num_blocks(struct blkdev *dev)
{
    struct mirror_dev *mirror = (struct mirror_dev *)dev->private;
    /* your code here */
    return mirror->nblks;
}

/* read from one of the sides of the mirror. (if one side has failed,
 * it had better be the other one...) If both sides have failed,
 * return an error.
 * Note that a read operation may return an error to indicate that the
 * underlying device has failed, in which case you should close the
 * device and flag it (e.g. as a null pointer) so you won't try to use
 * it again. 
 */
static int mirror_read(struct blkdev *dev, int first_blk,
                       int num_blks, void *buf)
{
    /* your code here*/
    struct mirror_dev *mdev = dev->private;
    struct blkdev *side[2];
    side[0] = mdev->disks[0];
    side[1] = mdev->disks[1];
    if (side[0] != NULL)
    {
        int val0 = side[0]->ops->read(side[0], first_blk, num_blks, buf);
        if (val0 == E_UNAVAIL)
        {
            side[0]->ops->close(side[0]);
            mdev->disks[0] = NULL;
        }
        else
        {
            return val0;
        }
    }
    if (side[1] != NULL)
    {
        int val1 = side[1]->ops->read(side[1], first_blk, num_blks, buf);
        if (val1 == E_UNAVAIL)
        {
            side[1]->ops->close(side[1]);
            mdev->disks[1] = NULL;
        }
        else
        {
            return val1;
        }
    }
    return E_UNAVAIL;
}

/* write to both sides of the mirror, or the remaining side if one has
 * failed. If both sides have failed, return an error.
 * Note that a write operation may indicate that the underlying device
 * has failed, in which case you should close the device and flag it
 * (e.g. as a null pointer) so you won't try to use it again.
 */
static int mirror_write(struct blkdev *dev, int first_blk,
                        int num_blks, void *buf)
{
    /* your code here */
    struct mirror_dev *mdev = dev->private;
    struct blkdev *side[2];
    side[0] = mdev->disks[0];
    side[1] = mdev->disks[1];
    int val0 = 0, val1 = 0;
    if (side[0] != NULL)
    {
        val0 = side[0]->ops->write(side[0], first_blk, num_blks, buf);
        if (val0 == E_UNAVAIL)
        {
            side[0]->ops->close(side[0]);
            mdev->disks[0] = NULL;
        }
    }
    if (side[1] != NULL)
    {
        val1 = side[1]->ops->write(side[1], first_blk, num_blks, buf);
        if (val1 == E_UNAVAIL)
        {
            side[1]->ops->close(side[1]);
            mdev->disks[1] = NULL;
        }
    }
    if (val0 != SUCCESS && val1 != SUCCESS)
    {
        return val0;
    }
    else
    {
        return SUCCESS;
    }
}

/* clean up, including: close any open (i.e. non-failed) devices, and
 * free any data structures you allocated in mirror_create.
 */
static void mirror_close(struct blkdev *dev)
{
    /* your code here */
    struct mirror_dev *mdev = dev->private;
    struct blkdev *side[2];
    side[0] = mdev->disks[0];
    side[1] = mdev->disks[1];
    if (side[0] != NULL)
    {
        side[0]->ops->close(side[0]);
        mdev->disks[0] = NULL;
    }
    if (side[1] != NULL)
    {
        side[1]->ops->close(side[1]);
        mdev->disks[1] = NULL;
    }
    free(mdev);
    free(dev);
}

struct blkdev_ops mirror_ops = {
    .num_blocks = mirror_num_blocks,
    .read = mirror_read,
    .write = mirror_write,
    .close = mirror_close};

/* create a mirrored volume from two disks. Do not write to the disks
 * in this function - you should assume that they contain identical
 * contents. 
 */
struct blkdev *mirror_create(struct blkdev *disks[2])
{
    /* your code here */
    int size0 = disks[0]->ops->num_blocks(disks[0]);
    int size1 = disks[1]->ops->num_blocks(disks[1]);
    if (size0 != size1)
    {
        printf("Different size\n");
        return NULL;
    }
    struct blkdev *dev = malloc(sizeof(*dev));
    struct mirror_dev *mdev = malloc(sizeof(*mdev));
    //point mirror_dev to disks
    mdev->disks[0] = disks[0];
    mdev->disks[1] = disks[1];
    mdev->nblks = size0;
    dev->private = mdev;
    dev->ops = &mirror_ops;

    return dev;
}

/* replace failed device 'i' (0 or 1) in a mirror. Note that we assume
 * the upper layer knows which device failed. You will need to
 * replicate content from the other underlying device before returning
 * from this call.
 */
int mirror_replace(struct blkdev *volume, int i, struct blkdev *newdisk)
{
    /* your code here */

    struct mirror_dev *mdev = volume->private;
    struct blkdev *mirror = mdev->disks[1 - i];
    if (mdev->nblks != newdisk->ops->num_blocks(newdisk))
    {
        return E_SIZE;
    }

    char buf[BLOCK_SIZE];
    int j;
    for (j = 0; j < mdev->nblks; j++)
    {
        int val = mirror->ops->read(mirror, j, 1, buf);

        // if read fails, return error message
        if (val == E_UNAVAIL)
        {
            mirror->ops->close(mirror);
            mdev->disks[1 - i] = NULL;
        }
        if (val != SUCCESS)
        {
            return val;
        }
        newdisk->ops->write(newdisk, j, 1, buf);
    }
    mdev->disks[i] = newdisk;
    return SUCCESS;
}

/**********  RAID0 ***************/
struct raid0_dev
{
    struct blkdev **disks;
    int nblks;
    int ndisks;
    int unit;
};

int raid0_num_blocks(struct blkdev *dev)
{
    struct raid0_dev *rdev = dev->private;
    return rdev->nblks;
}

/* read blocks from a striped volume. 
 * Note that a read operation may return an error to indicate that the
 * underlying device has failed, in which case you should (a) close the
 * device and (b) return an error on this and all subsequent read or
 * write operations. 
 */
static int raid0_read(struct blkdev *dev, int first_blk,
                      int num_blks, void *buf)
{
    struct raid0_dev *rdev = dev->private;
    int ndisks = rdev->ndisks;
    int unit = rdev->unit;
    int stripe_index, disk_index, blk_offset_in_stripe, stripe_offset_on_disk, blk_offset_on_disk;
    int val = 0;
    struct blkdev *des_disk;
    int i;
    for (i = first_blk; i < first_blk + num_blks; i++)
    {
        stripe_index = i / unit; //number of stripe the block is in
        disk_index = stripe_index % ndisks;
        blk_offset_in_stripe = i % unit;
        stripe_offset_on_disk = stripe_index / ndisks;
        blk_offset_on_disk = stripe_offset_on_disk * unit + blk_offset_in_stripe;
        des_disk = rdev->disks[disk_index];
        if (des_disk == NULL)
        {
            return E_UNAVAIL;
        }
        val = des_disk->ops->read(des_disk, blk_offset_on_disk, 1, (char *)buf + (i - first_blk) * BLOCK_SIZE);
        if (val == E_UNAVAIL)
        {
            blkdev_close(rdev->disks[disk_index]);
            rdev->disks[disk_index] = NULL;
            return E_UNAVAIL;
        }
        else if (val != SUCCESS)
        {
            return val;
        }
    }
    return val;
}

/* write blocks to a striped volume.
 * Again if an underlying device fails you should close it and return
 * an error for this and all subsequent read or write operations.
 */
static int raid0_write(struct blkdev *dev, int first_blk,
                       int num_blks, void *buf)
{
    struct raid0_dev *rdev = dev->private;
    int ndisks = rdev->ndisks;
    int unit = rdev->unit;
    int stripe_index, disk_index, blk_offset_in_stripe, stripe_offset_on_disk, blk_offset_on_disk;
    int val = 0;
    struct blkdev *des_disk;
    int i;
    for (i = first_blk; i < first_blk + num_blks; i++)
    {
        stripe_index = i / unit; //number of stripe the block is in
        disk_index = stripe_index % ndisks;
        blk_offset_in_stripe = i % unit;
        stripe_offset_on_disk = stripe_index / ndisks;
        blk_offset_on_disk = stripe_offset_on_disk * unit + blk_offset_in_stripe;
        des_disk = rdev->disks[disk_index];
        if (des_disk == NULL)
        {
            return E_UNAVAIL;
        }
        val = des_disk->ops->write(des_disk, blk_offset_on_disk, 1, (char *)buf + (i - first_blk) * BLOCK_SIZE);
        if (val == E_UNAVAIL)
        {
            blkdev_close(rdev->disks[disk_index]);
            rdev->disks[disk_index] = NULL;
            return E_UNAVAIL;
        }
        else if (val != SUCCESS)
        {
            return val;
        }
    }
    return val;
}

/* clean up, including: close all devices and free any data structures
 * you allocated in stripe_create. 
 */
static void raid0_close(struct blkdev *dev)
{
    struct raid0_dev *rdev = dev->private;
    int i;
    for (i = 0; i < rdev->ndisks; i++)
    {
        if (rdev->disks[i] == NULL)
        {
            continue;
        }
        rdev->disks[i]->ops->close(rdev->disks[i]);
        rdev->disks[i] = NULL;
    }
    free(rdev->disks);
    free(rdev);
    free(dev);
}

struct blkdev_ops raid0_ops = {
    .num_blocks = raid0_num_blocks,
    .read = raid0_read,
    .write = raid0_write,
    .close = raid0_close};
/* create a striped volume across N disks, with a stripe size of
 * 'unit'. (i.e. if 'unit' is 4, then blocks 0..3 will be on disks[0],
 * 4..7 on disks[1], etc.)
 * Check the size of the disks to compute the final volume size, and
 * fail (return NULL) if they aren't all the same.
 * Do not write to the disks in this function.
 */
struct blkdev *raid0_create(int N, struct blkdev *disks[], int unit)
{
    int i = 0;
    int nblocks = disks[0]->ops->num_blocks(disks[i]);
    for (i = 1; i < N; i++)
    {
        if (nblocks != disks[i]->ops->num_blocks(disks[i]))
        {
            printf("ERROR: size of disks not same");
            return NULL;
        }
    }
    struct blkdev *dev = malloc(sizeof(*dev));
    struct raid0_dev *rdev = malloc(sizeof(*rdev));
    rdev->disks = malloc(N * sizeof(*dev));
    for (i = 0; i < N; i++)
    {
        rdev->disks[i] = disks[i];
    }
    rdev->nblks = N * (nblocks / unit) * unit;
    rdev->ndisks = N;
    rdev->unit = unit;

    dev->private = rdev;
    dev->ops = &raid0_ops;
    return dev;
}

/**********   RAID 4  ***************/

/* helper function - compute parity function across two blocks of
 * 'len' bytes and put it in a third block. Note that 'dst' can be the
 * same as either 'src1' or 'src2', so to compute parity across N
 * blocks you can do: 
 *
 *     void **block[i] - array of pointers to blocks
 *     dst = <zeros[len]>
 *     for (i = 0; i < N; i++)
 *        parity(block[i], dst, dst);
 *
 * Yes, it could be faster. Don't worry about it.
 */
struct raid4_dev
{
    struct blkdev **disks;
    int nblks;
    int ndisks;
    int unit;
    int failed;
};

int raid4_num_blocks(struct blkdev *dev)
{
    struct raid4_dev *r4dev = dev->private;
    return r4dev->nblks;
}

void parity(int len, void *src1, void *src2, void *dst)
{
    unsigned char *s1 = src1, *s2 = src2, *d = dst;
    int i;
    for (i = 0; i < len; i++)
        d[i] = s1[i] ^ s2[i];
}

/* read blocks from a RAID 4 volume.
 * If the volume is in a degraded state you may need to reconstruct
 * data from the other stripes of the stripe set plus parity.
 * If a drive fails during a read and all other drives are
 * operational, close that drive and continue in degraded state.
 * If a drive fails and the volume is already in a degraded state,
 * close the drive and return an error.
 */

static int raid4_read_in_degraded_state(struct blkdev *dev, int failed, void *buf, int blk_offset_on_disk)
{
    struct raid4_dev *r4dev = dev->private;
    int ndisks = r4dev->ndisks;
    int i = 0;
    int val;

    char res[BLOCK_SIZE];
    char tmp[BLOCK_SIZE];
    int isEmpty = 1;
    for (i = 0; i < ndisks; i++)
    {
        if (r4dev->disks[i] == NULL)
        {
            continue;
        }
        val = raid4_read_helper(dev, tmp, blk_offset_on_disk, i);
        if (val != SUCCESS)
        {
            return val;
        }
        if(isEmpty) {
            memcpy(res, tmp, BLOCK_SIZE);
            isEmpty = 0;
        } else {
            parity(BLOCK_SIZE, tmp, res, res);
        }
    }
    memcpy(buf, res, BLOCK_SIZE);
    return val;
}

static int raid4_read(struct blkdev *dev, int first_blk, int num_blks, void *buf)
{
    struct raid4_dev *r4dev = dev->private;
    int ndisks = r4dev->ndisks;
    int unit = r4dev->unit;
    int stripe_index, disk_index, blk_offset_in_stripe, stripe_offset_on_disk, blk_offset_on_disk;
    struct blkdev *des_disk;
    int val = 0;
    int i;
    for (i = first_blk; i < first_blk + num_blks; i++)
    {
        stripe_index = i / unit; //number of stripe the block is in
        disk_index = stripe_index % (ndisks - 1);
        blk_offset_in_stripe = i % unit;
        stripe_offset_on_disk = stripe_index / (ndisks - 1);
        blk_offset_on_disk = stripe_offset_on_disk * unit + blk_offset_in_stripe;
        val = raid4_read_helper(dev, (char *)buf + (i - first_blk) * BLOCK_SIZE, blk_offset_on_disk, disk_index);
    }
    return val;
}

/* write blocks to a RAID 4 volume.
 * Note that you must handle short writes - i.e. less than a full
 * stripe set. You may either use the optimized algorithm (for N>3
 * read old data, parity, write new data, new parity) or you can read
 * the entire stripe set, modify it, and re-write it. Your code will
 * be graded on correctness, not speed.
 * If an underlying device fails you should close it and complete the
 * write in the degraded state. If a drive fails in the degraded
 * state, close it and return an error.
 * In the degraded state perform all writes to non-failed drives, and
 * forget about the failed one. (parity will handle it)
 */
static int raid4_write(struct blkdev *dev, int first_blk, int num_blks, void *buf)
{
    struct raid4_dev *r4dev = dev->private;
    int ndisks = r4dev->ndisks;
    int unit = r4dev->unit;
    int stripe_index, disk_index, blk_offset_in_stripe, stripe_offset_on_disk, blk_offset_on_disk;
    int val = 0;
    char old_data[BLOCK_SIZE];
    char old_parity[BLOCK_SIZE];
    struct blkdev *parity_disk = r4dev->disks[ndisks - 1];
    struct blkdev *des_disk;
    int i;
    for (i = first_blk; i < first_blk + num_blks; i++)
    {
        stripe_index = i / unit; //number of stripe the block is in
        disk_index = stripe_index % (ndisks - 1);
        blk_offset_in_stripe = i % unit;
        stripe_offset_on_disk = stripe_index / (ndisks - 1);
        blk_offset_on_disk = stripe_offset_on_disk * unit + blk_offset_in_stripe;
        des_disk = r4dev->disks[disk_index];
        //read old data
        val = raid4_read_helper(dev, old_data, blk_offset_on_disk, disk_index);
        if (val != SUCCESS)
        {
            printf("write failed");
            return val;
        }
        //write new data
        val = raid4_write_helper(dev, (char *)buf + (i - first_blk) * BLOCK_SIZE, blk_offset_on_disk, disk_index);
        if (val != SUCCESS)
        {
            printf("write failed");
            return val;
        }
        //read parity
        val = raid4_read_helper(dev, old_parity, blk_offset_on_disk, ndisks - 1);
        if (val != SUCCESS)
        {
            printf("write failed");
            return val;
        }
        //calculate parity
        parity(BLOCK_SIZE, old_data, old_parity, old_parity);
        parity(BLOCK_SIZE, old_parity, (char *)buf + (i - first_blk) * BLOCK_SIZE, old_parity);

        //write new parity
        val = raid4_write_helper(dev, old_parity, blk_offset_on_disk, ndisks - 1);
        if (val != SUCCESS)
        {
            printf("write failed");
            return val;
        }
    }
    return val;
}

int raid4_read_helper(struct blkdev *dev, char *buffer, int blk_offset_on_disk, int disk_index)
{
    struct raid4_dev *r4dev = dev->private;
    struct blkdev *des_disk = r4dev->disks[disk_index];
    int val = SUCCESS;
    if (des_disk != NULL)
    {
        val = des_disk->ops->read(des_disk, blk_offset_on_disk, 1, buffer);
        if (val == E_UNAVAIL)
        {
            printf("closed");
            if (r4dev->failed != -1 && r4dev->failed != disk_index)
            {
                printf("read failed");
                des_disk->ops->close(des_disk);
                r4dev->disks[disk_index] = NULL;
                return val;
            }
            else
            {
                r4dev->failed = disk_index;
                des_disk->ops->close(des_disk);
                r4dev->disks[disk_index] = NULL;
                val = raid4_read_in_degraded_state(dev, disk_index, buffer, blk_offset_on_disk);
            }
        }
    } else {
        if(r4dev->failed != -1 && r4dev->failed != disk_index) {
            return E_UNAVAIL;
        }
        val = raid4_read_in_degraded_state(dev, disk_index, buffer, blk_offset_on_disk);
    }
    return val;
}

int raid4_write_helper(struct blkdev *dev, char *buffer, int blk_offset_on_disk, int disk_index)
{
    struct raid4_dev *r4dev = dev->private;
    struct blkdev *des_disk = r4dev->disks[disk_index];
    int val = SUCCESS;
    if (des_disk != NULL)
    {
        val = des_disk->ops->write(des_disk, blk_offset_on_disk, 1, buffer);
        if (val == E_UNAVAIL)
        {
            printf("closed");
            if (r4dev->failed != -1 && r4dev->failed != disk_index)
            {
                printf("write failed");
                des_disk->ops->close(des_disk);
                r4dev->disks[disk_index] = NULL;
                return val;
            }
            else
            {
                r4dev->failed = disk_index;
                des_disk->ops->close(des_disk);
                r4dev->disks[disk_index] = NULL;
            }
        }
    } else {
        if(r4dev->failed != -1 && r4dev->failed != disk_index) {
            return E_UNAVAIL;
        }
    }
    return val;
}

/* clean up, including: close all devices and free any data structures
 * you allocated in raid4_create. 
 */
static void raid4_close(struct blkdev *dev)
{
    struct raid4_dev *r4dev = dev->private;
    int i;
    for (i = 0; i < r4dev->ndisks; i++)
    {
        if (r4dev->disks[i] == NULL)
        {
            continue;
        }
        r4dev->disks[i]->ops->close(r4dev->disks[i]);
        r4dev->disks[i] = NULL;
    }
    free(r4dev->disks);
    free(r4dev);
    free(dev);
}

/* Initialize a RAID 4 volume with strip size 'unit', using
 * disks[N-1] as the parity drive. Do not write to the disks - assume
 * that they are properly initialized with correct parity. (warning -
 * some of the grading scripts may fail if you modify data on the
 * drives in this function)
 */

struct blkdev_ops raid4_ops = {
    .num_blocks = raid4_num_blocks,
    .read = raid4_read,
    .write = raid4_write,
    .close = raid4_close};

struct blkdev *raid4_create(int N, struct blkdev *disks[], int unit)
{
    int i = 0;
    int nblocks = disks[0]->ops->num_blocks(disks[i]);
    for (i = 1; i < N; i++)
    {
        if (nblocks != disks[i]->ops->num_blocks(disks[i]))
        {
            printf("ERROR: size of disks not same");
            return NULL;
        }
    }
    struct blkdev *dev = malloc(sizeof(*dev));
    struct raid4_dev *r4dev = malloc(sizeof(*r4dev));
    r4dev->disks = malloc(N * sizeof(*dev));

    //copy all disks to raid4_dev
    for (i = 0; i < N; i++)
    {
        r4dev->disks[i] = disks[i];
    }
    r4dev->nblks = (N - 1) * (nblocks / unit) * unit; //one disk for parity
    r4dev->unit = unit;
    r4dev->ndisks = N;
    r4dev->failed = -1;
    dev->private = r4dev;
    dev->ops = &raid4_ops;
    return dev;
}

/* replace failed device 'i' in a RAID 4. Note that we assume
 * the upper layer knows which device failed. You will need to
 * reconstruct content from data and parity before returning
 * from this call.
 */
int raid4_replace(struct blkdev *volume, int i, struct blkdev *newdisk)
{
    struct raid4_dev *r4dev = volume->private;
    int ndisks = r4dev->ndisks;
    int nblks = r4dev->nblks;
    int unit = r4dev->unit;
    int nblks_on_disk = nblks / (ndisks - 1);
    int val, flag;
    int j, k;
    char buf[BLOCK_SIZE];
    for (j = 0; j < nblks_on_disk; j++)
    {
        val = raid4_read_in_degraded_state(volume, i, buf, j);
        if (val != SUCCESS)
        {
            return val;
        }
        val = newdisk->ops->write(newdisk, j, 1, buf);
        if (val != SUCCESS)
        {
            return val;
        }
    }
    r4dev->disks[i] = newdisk;
    return val;
}
