/*MUTEX TRIE*/

/* A simple, (reverse) trie.  Only for use with 1 thread. */
//Danila Chenchik (chenchik) and Greer Glenn (greer112) we agree to the UNC Honor Code. We did not give nor receive unautorized help on this assignment

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h> //mutex lock
#include "trie.h"

//error code for locking or unlocking nodes
#define NULL_NODE_ERROR 9999
#define MAX_KEY 64

struct trie_node {
  struct trie_node *next;  /* parent list */
  unsigned int strlen; /* Length of the key */
  int32_t ip4_address; /* 4 octets */
  struct trie_node *children; /* Sorted list of children */
  char key[MAX_KEY]; /* Up to 64 chars */
  pthread_mutex_t nodeMutex; /* single lock for each node */
};

static struct trie_node * root = NULL;
static int node_count = 0;
static int max_count = 100;  //Try to stay at no more than 100 nodes
//global mutex for locking down the root
pthread_mutex_t  mutex;
//global mutex for locking down the 
pthread_mutex_t  node_count_mutex;
pthread_cond_t  condition;
extern int separate_delete_thread;
//int for signaling if we are shutting down the delete thread
//int closing_delete_thread = 0;
extern int finished;

//DC METHODS
char* cat(char *str1, char *str2, int len1, int len2);
char* keyString(char *str, int len);

//method for locking node
int lockNode(struct trie_node *node){
  int lockval = NULL_NODE_ERROR;
  if(node != NULL){
    lockval = pthread_mutex_lock(&(node->nodeMutex));
    assert(lockval == 0);
  }
  return lockval;
}

//method for unlocking node
int unlockNode(struct trie_node *node){
  int lockval = NULL_NODE_ERROR;
  if(node != NULL){
    lockval = pthread_mutex_unlock(&(node->nodeMutex));
    assert(lockval == 0);
  }
  return lockval;
}

//method for unlocking two nodes at once
int doubleUnlock(struct trie_node *node1, struct trie_node *node2){
 int lockval = NULL_NODE_ERROR;
 if(node1){
   lockval = unlockNode(node1);
   assert( lockval == 0);
 }else if(node2){
   lockval = unlockNode(node2);
   assert(lockval == 0);
 }
 return lockval;
}

//special print method
int spr(char * str){
  printf("%s\n", str);
  fflush(stdout);
  return 1;
}
//END DC METHODS

struct trie_node * new_leaf (const char *string, size_t strlen, int32_t ip4_address) {
  struct trie_node *new_node = malloc(sizeof(struct trie_node));
  node_count++;
  if (!new_node) {
    printf ("WARNING: Node memory allocation failed.  Results may be bogus.\n");
    return NULL;
  }
  assert(strlen < 64);
  assert(strlen > 0);
  new_node->next = NULL;
  new_node->strlen = strlen;
  strncpy(new_node->key, string, strlen);
  new_node->key[strlen] = '\0';
  new_node->ip4_address = ip4_address;
  new_node->children = NULL;
  //initialize new mutex for leaf
  int nodeMutexCheck = pthread_mutex_init(&(new_node->nodeMutex), NULL);
  assert(nodeMutexCheck == 0);
  return new_node;
}

// Compare strings backward.  Unlike strncmp, we assume
// that we will not stop at a null termination, only after
// n chars (or a difference).  Base code borrowed from musl
int reverse_strncmp(const char *left, const char *right, size_t n)
{
    const unsigned char *l= (const unsigned char *) &left[n-1];
    const unsigned char *r= (const unsigned char *) &right[n-1];
    if (!n--) return 0;
    for (; *l && *r && n && *l == *r ; l--, r--, n--);
    return *l - *r;
}

int compare_keys (const char *string1, int len1, const char *string2, int len2, int *pKeylen) {
    int keylen, offset;
    char scratch[MAX_KEY];
    assert (len1 > 0);
    assert (len2 > 0);
    // Take the max of the two keys, treating the front as if it were 
    // filled with spaces, just to ensure a total order on keys.
    if (len1 < len2) {
        keylen = len2;
        offset = keylen - len1;
        memset(scratch, ' ', offset);
        memcpy(&scratch[offset], string1, len1);
        string1 = scratch;
    } else if (len2 < len1) {
        keylen = len1;
        offset = keylen - len2;
        memset(scratch, ' ', offset);
        memcpy(&scratch[offset], string2, len2);
        string2 = scratch;
    } else
        keylen = len1; // == len2
      
    assert (keylen > 0);
    if (pKeylen)
        *pKeylen = keylen;
    return reverse_strncmp(string1, string2, keylen);
}

int compare_keys_substring (const char *string1, int len1, const char *string2, int len2, int *pKeylen) {
    int keylen, offset1, offset2;
    keylen = len1 < len2 ? len1 : len2;
    offset1 = len1 - keylen;
    offset2 = len2 - keylen;
    assert (keylen > 0);
    if (pKeylen)
        *pKeylen = keylen;
    return reverse_strncmp(&string1[offset1], &string2[offset2], keylen);
}

void init(int numthreads) {
  /* Your code here */

  //how many threads do we want this to have?
  if (numthreads > 3)
    printf("You have %d threads. That's a lot.\n", numthreads);

  root = NULL;

  //make the global root lock
  int res1 = pthread_mutex_init(&mutex, NULL);

  //amke the node count lock
  int res2 = pthread_mutex_init(&node_count_mutex, NULL);

  //make the condition
  int res3 = pthread_cond_init(&condition, NULL);

 assert(res1 == 0);
 assert(res2 == 0);
 assert(res3 == 0);

}

void shutdown_delete_thread() {
  /* Your code here */
  //pthread_cond_signal(&condition);
  //separate_delete_thread = 0;
  check_max_nodes();
  return;
}


/* Recursive helper function.
 * Returns a pointer to the node if found.
 * Stores an optional pointer to the 
 * parent, or what should be the parent if not found.
 * 
 */
struct trie_node * 
_fine_search (struct trie_node *node, const char *string, size_t strlen, struct trie_node *previous) {
  
  int keylen, cmp;
  // First things first, check if we are NULL 
  if (node == NULL){ 
    //unlock previous and mutex just in case
    unlockNode(previous);
    if(previous == NULL){
      pthread_mutex_unlock(&mutex);
    }
    return NULL;
  }

  assert(node->strlen <= MAX_KEY);

  // See if this key is a substring of the string passed in
  cmp = compare_keys_substring(node->key, node->strlen, string, strlen, &keylen);
  if (cmp == 0) {
    // Yes, either quit, or recur on the children

    // If this key is longer than our search string, the key isn't here
    if (node->strlen > keylen) {
      //release verything if we are going to return
      unlockNode(previous);
      unlockNode(node);
      if(previous == NULL){
        pthread_mutex_unlock(&mutex);
      }
      return NULL;
    } else if (strlen > keylen) {
      // Recur on children list

      lockNode(node->children);
      unlockNode(previous);
     //if we are in the first iteration, go head and unlock the global root mutex  
      if(previous == NULL){
        pthread_mutex_unlock(&mutex);
      }
      return _fine_search(node->children, string, strlen - keylen, node);
    } else {
      assert (strlen == keylen);
      unlockNode(previous);
      unlockNode(node);
      //last thing we do is we always want to unlock the global mutex
      if(previous == NULL){
        pthread_mutex_unlock(&mutex);
      }
      return node;
    }

  } else {
    cmp = compare_keys(node->key, node->strlen, string, strlen, &keylen);
    if (cmp < 0) {
      // No, look right (the node's key is "less" than the search key)
      //return _prep_fine_search(node, string, strlen, parent, NEXT_FLAG);
      lockNode(node->next);
      unlockNode(previous);

      if(previous == NULL){
        pthread_mutex_unlock(&mutex);
      }
      return _fine_search(node->next, string, strlen, node);
    } else {
      // Quit early
      unlockNode(previous);
      unlockNode(node);

      if(previous == NULL){
        pthread_mutex_unlock(&mutex);
      }
      return 0;
    }
  }

}


int search  (const char *string, size_t strlen, int32_t *ip4_address) {

  struct trie_node *found;

  assert(strlen <= MAX_KEY);

  // Skip strings of length 0
  if (strlen == 0){
    return 0;
  }

  //always lock global mutex and then lock down the root
  pthread_mutex_lock(&mutex);
  lockNode(root);
  found = _fine_search(root, string, strlen, NULL);
  
  if (found && ip4_address)
    *ip4_address = found->ip4_address;

  int foundBool = (found != NULL);

  return foundBool;
}

/* Recursive helper function */
//we want recursion iteration to know when to unlock the global mutex
int _insert (const char *string, size_t strlen, int32_t ip4_address, 
       struct trie_node *node, struct trie_node *parent, struct trie_node *left, int recursionIteration) {

  int cmp, keylen;

  // First things first, check if we are NULL
  assert (node != NULL);
  assert (node->strlen <= MAX_KEY);
  
  //if we are in the second iteration of recursing, go ahead and unlock the global root mutex
  if(recursionIteration == 2){
    pthread_mutex_unlock(&mutex);
  }

  // Take the minimum of the two lengths
  cmp = compare_keys_substring (node->key, node->strlen, string, strlen, &keylen);
  if (cmp == 0) {
    // Yes, either quit, or recur on the children

    // If this key is longer than our search string, we need to insert
    // "above" this node
    if (node->strlen > keylen) {
      struct trie_node *new_node;

      assert(keylen == strlen);
      assert((!parent) || parent->children == node);
      //lock and unlock node_count_mutex before creating a new leaf because we will be altering node_count
      pthread_mutex_lock(&node_count_mutex);
      new_node = new_leaf (string, strlen, ip4_address);
      pthread_mutex_unlock(&node_count_mutex);
      
      lockNode(new_node);
      node->strlen -= keylen;
      new_node->children = node;
      new_node->next = node->next;
      node->next = NULL;
      
      assert ((!parent) || (!left));

      if (parent) {
          parent->children = new_node;
      } else if (left) {
          left->next = new_node;
      } else if ((!parent) || (!left)) {
          root = new_node;
      }
      //unlock everything before returning
      doubleUnlock(left, parent);
      unlockNode(node);
      unlockNode(new_node);
      //but only unlock global mutex root lock if we are on first recursion
      if(recursionIteration == 1){
        pthread_mutex_unlock(&mutex);  
      }
      return 1;

    } else if (strlen > keylen) {
      
      if (node->children == NULL) {
        // Insert leaf here
        pthread_mutex_lock(&node_count_mutex);
        struct trie_node *new_node = new_leaf (string, strlen - keylen, ip4_address);
        pthread_mutex_unlock(&node_count_mutex);
        /*final*///lockNode(new_node);
        node->children = new_node;
        doubleUnlock(left, parent);
        unlockNode(node);
      
        /*final*///unlockNode(new_node);

        if(recursionIteration == 1){
          pthread_mutex_unlock(&mutex);  
        }
        return 1;
      } else {
        // Recur on children list, store "parent" (loosely defined)
        //lock node->children since it is about to become the new "node"
        lockNode(node->children);
        doubleUnlock(left, parent);
        //we will unlock the global mutex lock in the next recursion
        return _insert(string, strlen - keylen, ip4_address, node->children, node, NULL, recursionIteration++);
      }
    } else {
      assert (strlen == keylen);
      if (node->ip4_address == 0) {
        node->ip4_address = ip4_address;
        doubleUnlock(left, parent);
        unlockNode(node);

        if(recursionIteration == 1){
          pthread_mutex_unlock(&mutex);  
        }
        return 1;
      } else {
        doubleUnlock(left, parent);
        unlockNode(node);

        if(recursionIteration == 1){
          pthread_mutex_unlock(&mutex);  
        }
        return 0;
      }
    }

  } else {
    /* Is there any common substring? */
    int i, cmp2, keylen2, overlap = 0;
    for (i = 1; i < keylen; i++) {
      cmp2 = compare_keys_substring (&node->key[i], node->strlen - i, &string[i], strlen - i, &keylen2);
      assert (keylen2 > 0);
      if (cmp2 == 0) {
        overlap = 1;
        break;
      }
    }

    if (overlap) {
      // Insert a common parent, recur
      int offset = strlen - keylen2;
      pthread_mutex_lock(&node_count_mutex);
      struct trie_node *new_node = new_leaf (&string[offset], keylen2, 0);
      pthread_mutex_unlock(&node_count_mutex);
      //need to lock new node
      lockNode(new_node);
      assert ((node->strlen - keylen2) > 0);
      node->strlen -= keylen2;
      new_node->children = node;
      new_node->next = node->next;
      node->next = NULL;
      assert ((!parent) || (!left));

      if (node == root) {
          root = new_node;
      } else if (parent) {
          assert(parent->children == node);
          parent->children = new_node;
      } else if (left) {
          left->next = new_node;
      } else if ((!parent) && (!left)) {
          root = new_node;
      }
      doubleUnlock(left, parent);
      //node is already locked
      //new node is currently locked
      return _insert(string, offset, ip4_address, node, new_node, NULL, recursionIteration++);
    } else {
      cmp = compare_keys (node->key, node->strlen, string, strlen, &keylen);
      if (cmp < 0) {
        // No, recur right (the node's key is "less" than  the search key)
        if (node->next){
          lockNode(node->next);
          doubleUnlock(left, parent);
          return _insert(string, strlen, ip4_address, node->next, NULL, node, recursionIteration++);
        }else {
          // Insert here
          pthread_mutex_lock(&node_count_mutex);
          struct trie_node *new_node = new_leaf (string, strlen, ip4_address);
          pthread_mutex_unlock(&node_count_mutex);
          lockNode(new_node);
          node->next = new_node;
          doubleUnlock(left, parent);
          unlockNode(node);
          unlockNode(new_node);

          if(recursionIteration == 1){
            pthread_mutex_unlock(&mutex);  
          }
          return 1;
        }
      } else {
        // Insert here
        pthread_mutex_lock(&node_count_mutex);
        struct trie_node *new_node = new_leaf (string, strlen, ip4_address);
        pthread_mutex_unlock(&node_count_mutex);
        lockNode(new_node);
        new_node->next = node;
        if (node == root)
          root = new_node;
        else if (parent && parent->children == node)
          parent->children = new_node;
        else if (left && left->next == node)
          left->next = new_node;
        
        unlockNode(new_node);
        doubleUnlock(left, parent);
        unlockNode(node);
      }
    }

    doubleUnlock(left, parent);
    unlockNode(node);

    if(recursionIteration == 1){
      pthread_mutex_unlock(&mutex);  
    }
    return 1;
  }
}

void assert_invariants();

int insert (const char *string, size_t strlen, int32_t ip4_address) {

  // Skip strings of length 0
  if (strlen == 0){
    return 0;
  }

  
  int ret = 0;
  /* Edge case: root is null */
  if (root == NULL) {
    
    //need to lock and unlock mutex, then node_count_mutex, since it will be changing when we add a new leaf
    pthread_mutex_lock(&mutex);
    pthread_mutex_lock(&node_count_mutex);

    root = new_leaf (string, strlen, ip4_address);

    pthread_mutex_unlock(&node_count_mutex);
    pthread_mutex_unlock(&mutex);
    return 1;
  }

  //lock down the global root mutex to protect the root
  pthread_mutex_lock(&mutex);
  lockNode(root);
  ret = _insert (string, strlen, ip4_address, root, NULL, NULL, 1);

  assert_invariants();

  //only signal the condition if we have separate delete thread running
  if(node_count > max_count && separate_delete_thread){
     pthread_cond_signal(&condition);
  }

  return ret;
}

/* Recursive helper function.
 * Returns a pointer to the node if found.
 * Stores an optional pointer to the 
 * parent, or what should be the parent if not found.
 * 
 */
 //previous is here mostly for knowing whether or not we are in the first recursion
struct trie_node * 
_delete (struct trie_node *node, const char *string, 
   size_t strlen, struct trie_node *previous) {
  int keylen, cmp;

  // First things first, check if we are NULL
   
  if (node == NULL){
    if(previous == NULL){
      pthread_mutex_unlock(&mutex);
    }
    unlockNode(previous);
    unlockNode(node);
    return NULL;
  }

  assert(node->strlen <= MAX_KEY);

  // See if this key is a substring of the string passed in
  cmp = compare_keys_substring (node->key, node->strlen, string, strlen, &keylen);
  if (cmp == 0) {
    // Yes, either quit, or recur on the children

    // If this key is longer than our search string, the key isn't here
    if (node->strlen > keylen) {
      unlockNode(node);
      if(previous == NULL){
        pthread_mutex_unlock(&mutex);
      }
      return NULL;
    } else if (strlen > keylen) {
      //lock nodes going down the recursion and unlock them when we return from the function call
      lockNode(node->children);
      struct trie_node *found =  _delete(node->children, string, strlen - keylen, node);
      if (found) {
        /* If the node doesn't have children, delete it.
         * Otherwise, keep it around to find the kids */
        if (found->children == NULL && found->ip4_address == 0) {
          lockNode(found);
          assert(node->children == found);
          node->children = found->next;
          pthread_mutex_lock(&node_count_mutex);
          unlockNode(found);
          free(found);
          node_count--;
          pthread_mutex_unlock(&node_count_mutex);
        }

        /* Delete the root node if we empty the tree */
        if (node == root && node->children == NULL && node->ip4_address == 0) {
          root = node->next;
          free(node);
          pthread_mutex_lock(&node_count_mutex);
        
          node_count--;
          pthread_mutex_unlock(&node_count_mutex);
        }
        
        unlockNode(node);
        if(previous == NULL){
          pthread_mutex_unlock(&mutex);
        }

        return node; /* Recursively delete needless interior nodes */
      } else{ 
        unlockNode(node);
        if(previous == NULL){
          pthread_mutex_unlock(&mutex);
        }
        return NULL;
      }
    } else {
      assert (strlen == keylen);

      /* We found it! Clear the ip4 address and return. */
      if (node->ip4_address) {
        node->ip4_address = 0;

        /* Delete the root node if we empty the tree */
        if (node == root && node->children == NULL && node->ip4_address == 0) {
          root = node->next;
          free(node);

          pthread_mutex_lock(&node_count_mutex);

          node_count--;
          pthread_mutex_unlock(&node_count_mutex);
          unlockNode(node);
          if(previous == NULL){
            pthread_mutex_unlock(&mutex);
          }
          return (struct trie_node *) 0x100100; /* XXX: Don't use this pointer for anything except 
             * comparison with NULL, since the memory is freed.
             * Return a "poison" pointer that will probably 
             * segfault if used.
             */
        }
        unlockNode(node);
        if(previous == NULL){
          pthread_mutex_unlock(&mutex);
        }
        return node;
      } else {
        /* Just an interior node with no value */
        unlockNode(node);
        if(previous == NULL){
          pthread_mutex_unlock(&mutex);
        }
        return NULL;
      }
    }
  } else {
    cmp = compare_keys (node->key, node->strlen, string, strlen, &keylen);
    if (cmp < 0) {
      // No, look right (the node's key is "less" than  the search key)
      lockNode(node->next);
      struct trie_node *found = _delete(node->next, string, strlen, node);
      if (found) {
        /* If the node doesn't have children, delete it.
         * Otherwise, keep it around to find the kids */
        if (found->children == NULL && found->ip4_address == 0) {
          assert(node->next == found);
          node->next = found->next;
          free(found);
          
          pthread_mutex_lock(&node_count_mutex);
          node_count--;
          pthread_mutex_unlock(&node_count_mutex);
        }       
        unlockNode(node);
        if(previous == NULL){
          pthread_mutex_unlock(&mutex);
        }
        return node; /* Recursively delete needless interior nodes */
      }
      unlockNode(node);
      if(previous == NULL){
        pthread_mutex_unlock(&mutex);
      }
      return NULL;
    } else {
      // Quit early
      unlockNode(node);
      if(previous == NULL){
        pthread_mutex_unlock(&mutex);
      }
      return NULL;
    }
  }
}

int delete  (const char *string, size_t strlen) {

  assert(strlen <= MAX_KEY);
  // Skip strings of length 0
  if (strlen == 0){
    return 0;
  }
  


  //locking...
  pthread_mutex_lock(&mutex);
  lockNode(root);
  int ret =  (NULL != _delete(root, string, strlen, NULL));

  assert_invariants();

  return ret;

}


/* Find one node to remove from the tree. 
 * Use any policy you like to select the node.
 */
int drop_one_node  () {
  if(root){
    fflush(stdout);
    pthread_mutex_lock(&mutex);
    lockNode(root);
    int totalStrLen = root->strlen;
    char *fullKey = keyString(root->key, root->strlen);
    struct trie_node *tmp = root;
    //get a full string, lock nodes on the way down
    while(tmp->children != NULL){
      tmp = tmp->children;
      lockNode(tmp);

      char *tmpStr = cat(keyString(tmp->key, tmp->strlen),fullKey, tmp->strlen, totalStrLen);
      totalStrLen += tmp->strlen;

      free(fullKey);
      fullKey = tmpStr;
      unlockNode(tmp);
    }
    if(totalStrLen == 0){
      return 0;
    }
    int returnVal = (NULL != _delete(root, fullKey, totalStrLen, NULL));    
    return returnVal; 
  }
  return 0;
}

/* Check the total node count; see if we have exceeded a the max.
 */
void check_max_nodes  () {

  //lock the node count mutex
  pthread_mutex_lock(&node_count_mutex);
  
  //only wait if we are in separate delete thread mode
  if(separate_delete_thread && !finished){
    while(node_count <= max_count){
      pthread_cond_wait(&condition, &node_count_mutex);
    }
  }

  while (node_count > max_count) {
    //unlock the node count mutex since we will be altering node count in _delete and locking it
    pthread_mutex_unlock(&node_count_mutex);
    if(!drop_one_node()){
      pthread_mutex_lock(&node_count_mutex);
      break;
    }
    //lock it to protect node count in this small amount of time
    pthread_mutex_lock(&node_count_mutex);
  }
  pthread_mutex_unlock(&node_count_mutex);
  
  return;
}


void _print (struct trie_node *node) {
  printf ("Node at %p.  Key %.*s, IP %d.  Next %p, Children %p\n", 
    node, node->strlen, node->key, node->ip4_address, node->next, node->children);
  if (node->children)
    _print(node->children);
  if (node->next)
    _print(node->next);
}

void print() {

  printf ("Root is at %p\n", root);
  /* Do a simple depth-first search */
  if (root)
    _print(root);

  printf("node count is : %d\n", node_count);
  
}

int _assert_invariants (struct trie_node *node, int prefix_length, int *error) {
    int count = 1;

    int len = prefix_length + node->strlen;
    if (len > MAX_KEY) {
        printf("key too long at node %p.  Key %.*s (%d), IP %d.  Next %p, Children %p\n", 
               node, node->strlen, node->key, node->strlen, node->ip4_address, node->next, node->children);
        *error = 1;
        return count;
    }
    
    if (node->children) {
        count += _assert_invariants(node->children, len, error);
        if (*error) {
            printf("Unwinding tree on error: node %p.  Key %.*s (%d), IP %d.  Next %p, Children %p\n", 
                   node, node->strlen, node->key, node->strlen, node->ip4_address, node->next, node->children);
            return count;
        }
    }

    if (node->next) {
        count += _assert_invariants(node->next, prefix_length, error);
    }

    return count;
}

void assert_invariants () {
#ifdef DEBUG    
    int err = 0;
    if (root) {
        int count = _assert_invariants(root, 0, &err);
        if (err) print();
        assert(count == node_count);
    }
#endif // DEBUG    
}

//DC METHODS
char* cat(char *str1, char *str2, int len1, int len2){
  char *res = malloc(len1*sizeof(char)+len2*sizeof(char)+1);
  strcpy(res, str1);
  strcat(res, str2);
  return res;
}

char* keyString(char *str, int len){
  char *tempStr = malloc(len*sizeof(char)+1);
  int i = 0;
  for(;i < len; i++){
    tempStr[i] = str[i];
  }
  tempStr[len] = '\0';
  return tempStr;
}

