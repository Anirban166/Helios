<p align = "center">
<img width = "95%" height = "auto" src = "Images/RefurbishedTransparentLogo.png">
</p>

---
<h2 align = "center">
Functionality
</h2>
High-performance parallelized distance matrix computation that utilizes both shared-memory (threads) and distributed memory (processes across cores) parallelism.

---
<h2 align = "center">
Features
</h2>
Core features include tiling the dataset for high cache utilization or improvement in spatial locality (note that the tile size is subject to be set by the user, and that generally small to medium tile sizes are preferable, depending upon the cache size of the user) and a hybrid setup of multiple OpenMP threads and MPI processes to run the distance matrix computations in parallel.

---
<h2 align = "center">
Performance
</h2>
Upon tests on the Million Song dataset with 10<sup>5</sup> lines (or a generated distance matrix of size 10<sup>10</sup>) of floating point data, Helios achieved more than hundred times the performance of the sequential version (without any form of parallelism) with 64 processes each tied to a separate CPU core, with eight threads per core.

> To be updated.
