===========
iOS Support
===========

To run qemu on the iOS platform, some modifications were required. Most of the 
modifications are conditioned on the ``CONFIG_IOS`` and ``CONFIG_NO_RWX`` 
configuration variables.

Build support
-------------

For the code to compile, certain changes in the block driver and the slirp 
driver had to be made. There is no ``system()`` call, so code requiring it had 
to be disabled.

``ucontext`` support is broken on iOS. The implementation from ``libucontext`` 
is used instead.

Because ``fork()`` is not allowed on iOS apps, the option to build qemu and the 
utilities as shared libraries is added. Note that because qemu does not perform 
resource cleanup in most cases (open files, allocated memory, etc), it is 
advisable that the user implements a proxy layer for syscalls so resources can 
be kept track by the app that uses qemu as a shared library.

Executable memory locking
-------------------------

The iOS kernel does not permit ``mmap()`` pages with 
``PROT_READ | PROT_WRITE | PROT_EXEC``. However, it does allow allocating pages 
with only ``PROT_READ | PROT_WRITE`` and then later calling ``mprotect()`` with 
``PROT_READ | PROT_EXEC``. A page can never be both writable and executable.

In this document, we will refer to a page that is read-writable as "unlocked" 
and a page that is read-executable as "locked." Because ``mprotect()`` is an 
expensive call, we try to defer calling it until we need to and also try to 
avoid calling it unless it is absolutely needed.

One approach would be to unlock the entire TCG region when a TB translation is 
being done and then lock the entire region when a TB is about to be executed. 
This would require thousands of pages to be locked and unlocked all the time. 
Additionally, it means that different vCPU threads cannot share the same TLB 
cache.

TB allocation changes
---------------------

To improve the performance, we first notice that ``tcg_tb_alloc()`` returns a 
chunk of memory that must be unlocked. A recent change in qemu places the TB 
structure close to the code buffer in order to improve both cache locality and 
reduce code size and memory usage. Unfortunately, we have to regress this 
improvement as any benefit from it is negated with the need to unlock the memory
whenever we need to mutate the TB structure.

We go back to the old method of statically allocating a large buffer for all 
TBs in a region. However a few improvements are made. First, we try to respect 
the locality by making this buffer close to the code. Second, whenever we flush 
the TB cache, we will use the average size of code blocks to divide up the TCG 
region into space for TB structures and space for code blocks.

Locked memory water level
-------------------------

By moving the TB allocation, we made it such that the memory only needs to be 
unlocked in the context of ``tb_gen_code()``. Because the code buffer pointer 
only grows downwards (we do not ever "free" code blocks and have holes), we 
only ever need to unlock at most one page.

We can think of the entire TCG region divided into two sections: the locked 
section and the unlocked section. At the start, the entire region is unlocked. 
As more and more code blocks are generated, the allocation pointer moves 
upwards. We can then lock the memory of any memory below the allocation pointer 
as the code generated is immutable. Therefore we can keep a second pointer to 
the highest page boundary the allocation pointer passes and keep all the memory 
below that pointer (all the way to the start of the region) locked and all the 
memory above it unlocked. This pointer is our locked water level.

That way, assuming all pages are unlocked at the start, we will progressively 
lock more pages as more code is generated. The only page we ever need to unlock 
would be the page pointed to by our locked water level pointer.

In ``tb_gen_code()`` we will call ``mprotect()`` on at most one page in order to
unlock the top of the water level (if it is currently locked). In 
``cpu_tb_exec()`` we will call ``mprotect()`` on all pages below the water 
level that are currently unlocked. This will, in most cases, be one or zero 
pages with the exception being if multiple pages of code were generated without 
being executed.

Multiple threads
----------------

Additional consideration is needed to handle multiple threads. We do not permit 
one vCPU from executing code on another vCPU if the end of the code is located 
at it's TCG region's locked water level. The reason is that without having 
synchronization between threads, we cannot guarantee if the page at the water 
level is locked or unlocked.

There are multiple places this may happen: when a TB is being looked up in the 
main loop, when a TB is being looked up as part of ``goto_tb``, and the TB chain 
caches (where after lookup, we encode a jump so a future call to the first TB 
will immediately jump into the second TB without lookup).

Since adding synchronization is expensive (holding on thread idle while another 
one is generating code defeats the purpose of parallel TCG contexts), we 
implement a lock-less solution. In each TB, we store a pointer to the water 
level pointer. Whenever a TB is looked up, we check that either 1) the TB 
belongs to the current thread and therefore we can ensure the memory is locked 
during execution or 2) the water level of the TCG context that the TB belongs to
is beyond the end of the TB's code block. This does mean that there might be 
redundant code generation done by multiple TCG contexts if multiple vCPUs all 
decide to execute the same block of code at the same time. This should not 
happen too often.

Similarly, for the TB chain cache, we will only chain a TB if either 1) both 
TBs' code buffer end pointer resides in the same page and therefore if the 
memory is locked to execute the first TB, we can jump to the second TB without 
issue, or 2) the second TB's code block fully resides below the locked water 
level of its TCG context. This means in some cases (such as two newly minted 
TBs from two threads happen to be chained), we will not chain the TB when we 
see it initially but will only chain it after a few subsequent executions and 
the locked water level has risen.
