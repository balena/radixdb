radixdb
=======

This library provides a C implementation of the Patricia Trie.  The algorithm
follows the 3 control words (32-bit) overhead -- length (diff bit) and
left/right indexes to next nodes -- plus length of the key and value. There's
no external nodes, so data is located at a node where the parent bit is equal
or greater than the node bit.  The overhead for each key-value pair is exactly
20 bytes (5 words of 32-bits).

It provides the following:
 * O(k) operations. In some cases, this can be faster than a hash table.
 * Largest prefix match.

This implementation does not support deletion, as it's intended for big
read-only databases.

Build
-----

    $ make

This will generate three binary files: radixdbmk, radixdbget and radixdbmatch.

The radixdbmk program
---------------------

In order to generate a radixdb file, do:

    $ ./radixdbmk < f > f.radixdb

A record is encoded for radixdbmk as `+klen,dlen:key->data` followed by a
newline. Here `klen` is the number of bytes in `key` and `dlen` is the number
of bytes in `data`. The end of `data` is indicated by an extra newline. For
example:

     +3,5:one->Hello
     +3,7:two->Goodbye

Keys and data do not have to fit into memory, but `radixdbmk` needs at least 20
bytes of memory per record. A database cannot exceed 4 gigabytes.

`f` is portable across machines.


The cdbget program
------------------

    $ ./radixdbget f.radixdb k

`radixdbget` searches for a record with key `k` in a constant database
`f.radixdb`. The constant database must be readable.

Normally `radixdbget` prints the data with key `k` and exits 0. If there is no
record with the key `k`, `radixdbget` exits 2.

If `radixdbget` encounters a read error, write error, or database format error,
it complains and exits 1.


The cdbmatch program
--------------------

    $ ./radixdbmatch f.radixdb k

`radixdbmatch` searches for the longest prefix available in the constant
database `f.radixdb` matching `k`, in the format `+klen,dlen:key->data`.


Have fun!
