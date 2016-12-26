/* Multi-threaded DNS-like Simulation.
 * 
 * Don Porter - porter@cs.unc.edu
 * 
 * COMP 530 - University of North Carolina, Chapel Hill
 * modified and added to by Greer Glenn (greer112) and Danila Chenchik (chenchik) 
 * We agree to and abide by the UNC Honor Code.
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <ctype.h>
#include "trie.h"

int separate_delete_thread = 0;
int simulation_length = 7; // default to 30 seconds
volatile int finished = 0;
int testlen = 100;
char strArr[100][63];


// Uncomment this line for debug printing
#define DEBUG 1
#ifdef DEBUG
#define DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
#define DEBUG_PRINT(...) 
#endif


static void *
delete_thread(void *arg) {
  while (!finished)
    check_max_nodes();
  return NULL;
}


static void *
client(void *arg)
{
  struct random_data rd;
  char rand_state[256];
  int32_t salt = time(0);

  // See http://lists.debian.org/debian-glibc/2006/01/msg00037.html
  rd.state = (int32_t*)rand_state;
  int iter = 0;
  // Initialize the prng.  For testing, it may be helpful to reduce noise by 
  // temporarily setting this to a fixed value.
  initstate_r(salt, rand_state, sizeof(rand_state), &rd);

  while (!finished) {
    /* Pick a random operation, string, and ip */
    int32_t code;
    int rv = random_r(&rd, &code);
    int length = (code >> 2) & (64-1);
    char buf[64];
    int j;
    int32_t ip4_addr;
    
    if (rv) {
      printf("Failed to get random number - %d\n", rv);
      return NULL;
    }

    DEBUG_PRINT("Length is %d\n", length);
    memset(buf, 0, 64);
    /* Generate a random string in lowercase */
    for (j = 0; j < length; j+= 6) {
      int i;
      int32_t chars;

      rv = random_r(&rd, &chars);
      if (rv) {
        printf("Failed to get random number - %d\n", rv);
        return NULL;
      }

      for (i = 0; i < 6 && (i+j) < length; i++) {
        char val = ( (chars >> (5 * i)) & 31);
        if (val > 25)
        val = 25;
        buf[j+i] = 'a' + val;
      }
    }

    DEBUG_PRINT ("Random string is %s\n", buf);
    
    //code = 2;
    switch (code % 3) {
    case 0: // Search
      DEBUG_PRINT ("Search\n");
      search (buf, length, NULL);
      break;
    case 1: // insert
      DEBUG_PRINT ("insert\n");
      rv = random_r(&rd, &ip4_addr);
      if (rv) {
        printf("Failed to get random number - %d\n", rv);
        return NULL;
      }
      insert (buf, length, ip4_addr);
      break;
    case 2: // delete
      DEBUG_PRINT ("delete\n");
      delete (buf, length);
      break;
    default:
      assert(0);
    }

    /* If we don't have a separate delete thread, the client needs to
     * make sure that the count didn't exceed the max.
     */
    printf("ITERATIoN %d of CLIENT\n", iter);
    iter++;
    if (!separate_delete_thread)
      check_max_nodes();
  }

  printf("&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&done with client&&&&&&&&&&&&&&\n");
  return NULL;
}


#define die(msg) do {       \
  print();          \
  printf(msg);          \
  exit(1);          \
  } while (0)

#define INSERT_TEST(ky, len, ip) do {       \
    rv = insert(ky, len, ip);         \
    if (!rv) die ("Failed to insert key " ky "\n"); \
  } while (0)             

#define SEARCH_TEST(ky, len, ex) do {       \
    rv = search(ky, len, &ip);          \
    if (!rv) die ("Failed fine insert key " ky "\n");   \
    if (ip != ex) die ("Found bad IP for key " ky "\n");  \
  } while (0)             

#define DELETE_TEST(ky, len) do {       \
    rv = delete(ky, len);         \
    if (!rv) die ("Failed to delete key " ky "\n"); \
  } while (0)             

static char *rando_string(char *str, size_t size)
{
  const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
  if (size) {
    --size;
    size_t n = 0;
    for (; n <= size; n++) {
      int key = rand() % (int) (26 + 26 -1);
      assert(key < 52);
      str[n] = charset[key];
    }
  }
  return str;
}

int customCopy(char * str, int pos, int len){
  int i = 0;
  for(; i < len; i++){
    strArr[pos][i] = str[i];
  }
  return 1;
}

int delete_tests_2(){

  int i = 0;
  for(; i < testlen; i++){
    printf("*** delete tests 2 inserting i: %d\n", i);
    char rando[63];
    rando_string(rando , 63);
    printf("random string : %s\n", rando);
    customCopy(rando, i, 63);
    int insertCheck = insert(rando, 63, 2);
    if (!insertCheck) die ("ERROR INSERTING IN DELETE TESTS 2\n");
  }
  return 1;
}

//this delete test serves as a minimal increase in node count to see how well the delete thread can keep up with minimal changes
int delete_tests_3(){

  int smalltestlen = testlen/10;
  int i = 0;
  for(; i < smalltestlen; i++){
    printf("*** delete tests 3 inserting i: %d\n", i);
    char rando[10];
    rando_string(rando , 10);
    printf("random string : %s\n", rando);
    int insertCheck = insert(rando, 10, 2);
    if (!insertCheck) die ("ERROR INSERTING IN DELETE TESTS 3\n");
  }
  return 1;

}

int self_tests() {
  int rv;
  int32_t ip = 0;
  int i = 0;
  if(!separate_delete_thread){
    delete_tests_2();
    for(; i < testlen; i++){
      printf("strarr at |%d| is |%s|\n", i , strArr[i]);
    }
    printf("SLEEPIN...");
 
    printf("about to check max nodes\n");
    sleep(1);
    check_max_nodes();
    print();
    sleep(1);
  }
  
  //testing the deleete thread
  if(separate_delete_thread){
    delete_tests_2();
    printf("sleeping...about to run a test for checking a smaller number of insertions and seeing how well the delete thread can catch up...\n");
    sleep(3);
    delete_tests_3();
    print();
    printf("sleeeping, about to run client after a few more tests\n");
    sleep(1); 
  }

  if(!separate_delete_thread){
    //will not work if we have dlete thread runnign, likely that some will not exist anymore
    // Tests suggested by James
    INSERT_TEST("google", 6, 1);
    INSERT_TEST("com", 3, 2);
    INSERT_TEST("edu", 3, 3);
    INSERT_TEST("org", 3, 4);
    INSERT_TEST("but", 3, 5);
    INSERT_TEST("butter", 6, 6);
    INSERT_TEST("pincher", 7, 7);
    INSERT_TEST("pinter", 6, 8);
    INSERT_TEST("roller", 6, 9);
    INSERT_TEST("simple", 6, 10);
    INSERT_TEST("file", 4, 11);
    INSERT_TEST("principle", 9, 12);

    SEARCH_TEST("google", 6, 1);
    SEARCH_TEST("com", 3, 2);
    SEARCH_TEST("edu", 3, 3);
    SEARCH_TEST("org", 3, 4);
    SEARCH_TEST("but", 3, 5);
    SEARCH_TEST("butter", 6, 6);
    SEARCH_TEST("pincher", 7, 7);
    SEARCH_TEST("pinter", 6, 8);
    SEARCH_TEST("roller", 6, 9);
    SEARCH_TEST("simple", 6, 10);
    SEARCH_TEST("file", 4, 11);
    SEARCH_TEST("principle", 9, 12);

    print();
  
    DELETE_TEST("google", 6);
    DELETE_TEST("com", 3);
    DELETE_TEST("edu", 3);
    DELETE_TEST("org", 3);
    DELETE_TEST("but", 3);
    DELETE_TEST("butter", 6);
    DELETE_TEST("pincher", 7);
    DELETE_TEST("pinter", 6);
    DELETE_TEST("roller", 6);
    DELETE_TEST("simple", 6);
    DELETE_TEST("file", 4);
    DELETE_TEST("principle", 9);

    // Tests suggested by Kammy
    INSERT_TEST("zhriz", 5, 1); 
    INSERT_TEST("eeonbws", 7, 2); 
    INSERT_TEST("mfpmirs", 7, 3); 
    INSERT_TEST("pzkvlyi", 7, 14);
    INSERT_TEST("xzrtjbz", 7, 6);

    print();

    SEARCH_TEST("xzrtjbz", 7, 6); 
    SEARCH_TEST("pzkvlyi", 7, 14);
    SEARCH_TEST("mfpmirs", 7, 3);
    SEARCH_TEST("eeonbws", 7, 2);
    SEARCH_TEST("zhriz", 5, 1); 

  
    DELETE_TEST("mfpmirs", 7);
    DELETE_TEST("xzrtjbz", 7); 
    DELETE_TEST("eeonbws", 7);
    DELETE_TEST("zhriz", 5); 
    DELETE_TEST("pzkvlyi", 7);
  }
  //printf("Starting delete test\n");
  //delete_tests_2();
  //printf("Done with delete tests, about to sleep\n");
  //sleep(10);
  
  printf("End of self-tests, tree is:\n");
  print();
  printf("End of self-tests\n");
  return 0;
}

void help() {
  printf ("DNS Simulator.  Usage: ./dns-[variant] [options]\n\n");
  printf ("Options:\n");
  printf ("\t-c numclients - Use numclients threads.\n");
  printf ("\t-h - Print this help.\n");
  printf ("\t-l length - Run clients for length seconds.\n");
  printf ("\t-t  - Run a separate delete thread.\n");
  printf ("\n\n");
}

int main(int argc, char ** argv) {
  int numthreads = 1; // default to 1
  //numthreads = 1;
  int c, i, rv;
  pthread_t *tinfo;

  // Read options from command line:
  //   # clients from command line, as well as seed file
  //   Simulation length
  //   Block if a name is already taken ("Squat")
  while ((c = getopt (argc, argv, "c:hl:t")) != -1) {
    switch (c) {
    case 'c':
      numthreads = atoi(optarg);
      break;
    case 'h':
      help();
      return 0;
    case 'l':
      simulation_length = atoi(optarg);
      break;
    case 't':
      separate_delete_thread = 1;
      break;
    default:
      printf ("Unknown option\n");
      help();
      return 1;
    }
  }

  // Create initial data structure, populate with initial entries
  // Note: Each variant of the tree has a different init function, statically compiled in
  init(numthreads);
  srandom(time(0));

  tinfo = calloc(numthreads + separate_delete_thread, sizeof(pthread_t));

  if (separate_delete_thread) {
    rv = pthread_create(&tinfo[numthreads], NULL,
      &delete_thread, NULL);
    if (rv != 0) {
      printf ("Delete thread creation failed %d\n", rv);
      return rv;
    }
  }
  // Run the self-tests if we are in debug mode 
#ifdef DEBUG
  self_tests();
#endif

  // Launch client threads
  for (i = 0; i < numthreads; i++) {

    rv = pthread_create(&tinfo[i], NULL,
      &client, NULL);
    if (rv != 0) {
      printf ("Thread creation failed %d\n", rv);
      return rv;
    }
  }

  // After the simulation is done, shut it down
  sleep (simulation_length);
  finished = 1;

  // Wait for all clients to exit.  If we are allowing blocking,
  // cancel the threads, since they may hang forever
  if (separate_delete_thread) {
    shutdown_delete_thread();
  }

  for (i = 0; i < numthreads; i++) {
    int rv = pthread_join(tinfo[i], NULL);
    if (rv != 0)
      printf ("Uh oh.  pthread_join failed %d\n", rv);
  }

  
#ifdef DEBUG  
  /* Print the final tree for fun */
  print();
#endif
  
  return 0;
}

