Lab 3 for Danila Chenchik (chenchik) and Greer Glenn (greer112)

used about 65 late hours (Danusha used about 3 on his Lab 1, Greer hasn't used any up until now)

Drop_One_Node :
	traverses the tree on the left hand side to find the first full string it can find and calls the delete() method on that string
	

Coarse Grained Mutex Trie:
	keep one pthread_mutex for the entire trie- lock it before each method that modifies the data structure (insert, delete, check_max_nodes) and we also locked search()
	Unlock the mutex right before returning in each method.



Delete Thread:
	Created a condition variable that signaled in insert method when node_count >= max_nodes. In check_max_nodes, the same condition waited until it was signaled, and then check_max_nodes was called until node_count got back down below our threshold variable, max_nodes


RW Lock Trie:
  We added a pthread_rwlock in addition to the mutex we already had from our coarse grained mutex tree (since we still needed a mutex in order to signal the delete thread). We locked the regular mutex in all functions that modified the node count (insert, delete) and then locked the rwlock (using pthread_rwlock_wrlock() method in order to allow multiple readers but only one writer at a time) and rw_lock was also used on search. We had to make our condition wait before locking the rwlock in check_max_nodes, otherwise we would sometimes get deadlocked or a core dump. And then once the condition had been signaled in check_max_nodes, we would let the delete thread run. We unlocked the rwlock before the regular mutex before returning.

Fine-Trie:
	Added a mutex to our node data structure, which gave each node it's own mutex. Added a global mutex that was associated with the root (called mutex) and a mutex associated with the node_count (called node_count_mutex).
	
	In general our thread ordering/calling structure is as follows: Global root mutex, then individual node mutexes, then node_count_mutex. Typically, the global root mutex would be unlocked in the first or second iteration of a recursion of a function. But generally, we saw that functions would only recur once or twice, if that was the case, the unlocking order would follow this pattern: node_count_mutex, individual node mutexes, global root mutex.

	On a few rare occasions, we did get some instances of corruption and segmentation fault/core aborted behavior (on about 1 out of every 20 attempts.)

	We've noticed that the more threads we attempt to run it with, the more we found errors.

TESTING IN MAIN:
    2 test functions created in main.c: delete_tests_2() and delete_tests_3(). They are called delete tests because they are testing the delete threads ability to catch up.
    Both of these insert various numbers of nodes. 
    delete_tests_2() inserts 1000 random strings and sees if the delete thread can catch up. 
    delete_tets_3() inserts 100 random strings and sees if the delete thread can catch up in a smaller time interval.



