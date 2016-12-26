# dnsServer
Simple multithreaded program built in C

As part of an introduction to multithreaded programs, me and one other person built a simple DNS server that could take ip addresses and associate them with domain names. The domain names were organized in a reverse trie data structure, with nodes holding certain common portions of the domain names. The trie was built around the idea of having a max node count, in our implementation, it was set to 100.

The project is built using linux. To download the porject, simple download the source code from github, upload it to a linux machine, and compile using the command:

<code>make</code>

The simple DNS server, or at least, the data structure that handles the organization of the domain names comes in 4 styles: sequential, coarse grained, read-write, and fine grained. The locks we used were pthread_mutex based.

Sequential: Simply handles one thread, it isn't the fastest thing around, but it will get the job done. Includes a methods called check_max_nodes() and drop_one_node().

check_max_nodes(): checks the node count to see if it is over the maximum allowed amount. Calls drop_one_node() until the node count is below the max.

drop_one_node(): traverses the trie on the left-hand side to find the first full string it can find and calls the delete() method on that string.

Coarse Grained: Handles multiple threads, including a delete thread, but only uses one lock. The delete thread makes sure that if there are over 100 nodes allocated to the trie, it deletes random ones to keep the node count within the threshold. This is a coarse-grained implementation because while there are multiple threads, there is only one lock, and every time the trie calls a method, the thread is locked and unlocked right before the method returns.

Delete Thread: Created a condition variable that signaled in the insert() method when node_count >= max_nodes. In check_max_nodes(), the same condition waited until it was signaled, and then check_max_nodes() was called until node_count got back down below our threshold variable, max_nodes.

RW Lock Trie: We added a pthread_rwlock in addition to the single mutex lock we already had from our coarse-grained mutex trie (since we still needed a mutex in order to signal the delete thread). We locked the regular mutex in all functions that modified the node count (insert, delete) and then locked the rwlock (using pthread_rwlock_wrlock() method in order to allow multiple readers but only one writer at a time) and rw_lock was also used on search. We had to make our condition wait before locking the rwlock in check_max_nodes, otherwise we would sometimes get deadlocked or a core dump. And then once the condition had been signaled in check_max_nodes, we would let the delete thread run. We unlocked the rwlock before the regular mutex before returning.

Fine Grained: Locking for each thread is handled in a per node basis. Each node was given its own mutex lock. We also added a global mutex that was associated with the root (called mutex) and a mutex associated with the node_count (called node_count_mutex). In general our thread ordering/calling structure is as follows: Global root mutex, then individual node mutexes, then node_count_mutex. Typically, the global root mutex would be unlocked in the first or second iteration of a recursion of a function. But generally, we saw that functions would only recure once or twice, if that was the case, the unlocking order would follow this pattern: node_count_mutex, individual node mutexes, global root mutex.

Download the project by downloading the zip from github or by running this command from the command line:

<code>git pull https://github.com/chenchik/dnsServer.git master</code>

This project was built with linux so be sure to upload the project to a linux machine, after doing so:

Compile the project with:

<code>make</code>

run sequential, coarse-grained, read-write, and fine-grained with these commands:

<code>dns-sequential</code>

<code>dns-mutex</code>

<code>dns-rw</code>

<code>dns-fine</code>
