## This is a implementation of Raid disk system
### It consists of Raid0, Raid1, Raid4 
### Random tests are used to simulate Sequential Read/Write and Random Read/Write
---------------------------------------------
### How to run:
```
sh mirror-test.sh
sh raid0-test.sh
sh raid4-test.sh
```

### What is Raid:
#### Raid0:
![Raid0](https://upload.wikimedia.org/wikipedia/commons/thumb/9/9b/RAID_0.svg/150px-RAID_0.svg.png)

> RAID 0 (also known as a stripe set or striped volume) splits ("stripes") data evenly across two or more disks, without parity information redundancy, or fault tolerance.  
> Since RAID 0 provides no fault tolerance or redundancy, the failure of one drive will cause the entire array to fail; as a result of having data striped across all disks, the failure will result in total data loss. This configuration is typically implemented having speed as the intended goal.  
> RAID 0 is normally used to increase performance, although it can also be used as a way to create a large logical volume out of two or more physical disks  

#### Raid1:

![Raid1](https://upload.wikimedia.org/wikipedia/commons/thumb/b/b7/RAID_1.svg/150px-RAID_1.svg.png)
> RAID 1 consists of an exact copy (or mirror) of a set of data on two or more disks; a classic RAID 1 mirrored pair contains two disks. 
> This configuration offers no parity, striping, or spanning of disk space across multiple disks, since the data is mirrored on all disks belonging to the array, and the array can only be as big as the smallest member disk. 
> This layout is useful when read performance or reliability is more important than write performance or the resulting data storage capacity. The array will continue to operate so long as at least one member drive is operational

### Raid4:

![Raid4](https://upload.wikimedia.org/wikipedia/commons/thumb/a/ad/RAID_4.svg/300px-RAID_4.svg.png)
> RAID 4 consists of block-level striping with a dedicated parity disk. As a result of its layout, RAID 4 provides good performance of random reads, while the performance of random writes is low due to the need to write all parity data to a single disk.
