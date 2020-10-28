#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <string.h>
#include <sys/stat.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <stdbool.h>
#include<mcheck.h>


#include "../cakelog/cakelog.h"

/* This function will return a pointer to a block of memory that consists of
all text in the dictionary file.*/
char* read_dictionary_file(const char* dict_file) {

    cakelog("===== read_dictionary() =====");
    cakelog("dictionary file: %s", dict_file);

    /* Let's use some syscalls for this stuff because I'm on Linux and
    feeling awkward. Obviously, if you're doing this on another platform you can
    use functions in the stdio.h library */

    /* Open the dictionary file */
    const int dictionary_fd = open(dict_file, O_RDONLY);
    if (dictionary_fd == -1) {
        perror("open()");
        cakelog("failed to open dictionary file");
        exit(EXIT_FAILURE);
    }

    cakelog("dictionary file opened with fd %d", dictionary_fd);

    /* Get the size of the file using fstat() so we can work out how big our 
    text buffer should be. We're going to read the whole lot into memory at once */
    struct stat dict_stats;

    if (fstat(dictionary_fd, &dict_stats) == -1) {
        cakelog("failed to get statistics for dictionary file");
        perror("fstat()");
        exit(EXIT_FAILURE);
    }

    /* Total size in bytes can be found in the st_size variable of the stat
     (https://man7.org/linux/man-pages/man2/stat.2.html) */
    const long file_size = dict_stats.st_size;

    cakelog("retrieved file_size of %ld bytes", file_size);

    /* We got the size of our buffer, but we will need space to add a terminating \0
    because the raw file text wont' have one */
    const long buffer_size = file_size + 1;

    /* Now allocate memory for our buffer */
    char* buffer = malloc(buffer_size);

    cakelog("initialised buffer size of %ld bytes (extra one for 0 (NULL terminator))", buffer_size);
 
    /* Now use the read() to load all the data in at once, no messing. */
    ssize_t bytes_read;
    if((bytes_read = read(dictionary_fd, buffer, file_size)) != file_size) {
        cakelog("unable to load file data into buffer");
        perror("read()");
        exit(EXIT_FAILURE);
    }

    cakelog("loaded %ld bytes into buffer", bytes_read);

    /* Good Housekeeping! */
    close(dictionary_fd);

    /* Finally, add the terminating \0 and we now have a large char block that
    consists of all the data in the file */
    buffer[file_size] = '\0';

    cakelog("added 0 (NULL terminator) to buffer position %ld", buffer_size);

    cakelog("returning buffer");

    return buffer;
}

long get_word_count(const char* data) {

    /* We know the data is made of of words, one on each line so we're just going to
    count the newlines ('\n') in the data */

    cakelog("===== get_word_count() =====");

    long word_count = 0;
    for (int i=0; data[i] != '\0'; i++) {
         if (data[i] == '\n') {
             word_count++;
         }
     }
     /* Add an extra one for the last word of the file (no newline postfix) */
     word_count++;

     cakelog("returning word count of %ld", word_count);
     
     return word_count;
}

char * hexdigest(const unsigned char* hash) {
    // Return a 64 character string representation of the hash.

    cakelog("===== hexdigest() =====");

    char * hexdigest = calloc(1,65);
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        // We're converting to Hex which takes up two digits per byte
        // Sha256 produces a 32 byte value
        // so when converting to string we need to add 2
        // Sha Digest Length is 32 bytes but when you represent each byte as a hexadecimal character in a string
        // you need two digits, so that means we need to shift along an extra one when loading up the array.
		sprintf(hexdigest + (i * 2), "%02x", hash[i]);
    }
    hexdigest[64]='\0';
    cakelog("returning %s", hexdigest);
    return hexdigest;
    
}

unsigned char * sha256(const char * data) {

    // Use openssl library to get a SHA256 hash of `data`. This is returned
    // as binary values in an unsigned char * so we will need a conversion
    // function to actually see the 64 character hash digest itself.

    cakelog("===== sha256() =====");

    unsigned int data_len = strlen(data);
    unsigned char * hash_digest;

    EVP_MD_CTX *mdctx;

    cakelog("initialising new mdctx");
    mdctx = EVP_MD_CTX_new();
    
    // All these functions return 1 for success and 0 for error ... well, it's a free country.
    EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL);
    cakelog("updating mdctx digest with data [%s]", data);
    EVP_DigestUpdate(mdctx, data, data_len);
    cakelog("initialising new hash_digest buffer");
    hash_digest = (unsigned char *)OPENSSL_malloc(EVP_MD_size(EVP_sha256()));
    EVP_DigestFinal_ex(mdctx, hash_digest, &data_len);
    cakelog("succesfully copied new digest to hash_digest buffer");
    EVP_MD_CTX_free(mdctx);
    cakelog("successfully freed mdctx digest");

    cakelog("returning");
    return hash_digest;

}

struct Node {
    struct Node * left;
    struct Node * right;
    char * sha256_digest;
    char * data;
}; typedef struct Node Node;

Node * new_node(Node * left, Node * right, char * data, char * sha256_digest) {

    // Helper function n to construct nodes
    cakelog("===== new_node() =====");
    cakelog("left: %p, right: %p, data: [%s], hash: [%s]", left, right, data, sha256_digest);

    Node * node = malloc(sizeof(Node));
    node->left = left;
    node->right = right;
    node->data = data;
    node->sha256_digest = sha256_digest;

    cakelog("returning new node at address %p", node);
    cakelog("======================");
    return node;
}

Node ** build_leaves(char* data) {

    cakelog("===== build_leaves() =====");

    long word_count = get_word_count(data);

    cakelog("building %ld leaves (pointers) from buffer", word_count);

    Node ** leaves = malloc(sizeof(Node *)*word_count);

    cakelog("allocated array of %ld bytes for leaves (number of leaves * sizeof(pointer) + NULL terminator", word_count * sizeof(unsigned char *) + 1);

    long index = 0;
    long hash_count = 0;

    cakelog("beginning loop through buffer using strtok");

    char * word = strtok(data, "\n\0");
    while( word != NULL) {
        cakelog("next word is [%s]", word);
        Node * n = new_node(NULL, NULL, word, hexdigest(sha256(word)));
        leaves[index] = n;
        hash_count++;
        index++;
        word = strtok(NULL, "\n\0");
    }

    int leaf_count = index;

    cakelog("returning %ld leaves", leaf_count);

    return leaves;
}

Node * build_merkle_tree(Node ** nodes, long len) {

    cakelog("===== build_merkle_tree() =====");

    cakelog("passed in node ** with address of %p and len of %ld", nodes, len);

    // We already have the root
    if (len == 1) {

        cakelog("len is 1 so that means we already have the root. Returning nodes[0] at address %p", nodes[0]);

        return nodes[0];
    }

    cakelog("len is greater than 1");

    // We know how many nodes are in this layer because it's passed in, so just declare
    // a big fat array.
    // Node * node_layer[len];
    // cakelog("created array with space for %ld node pointers in node_layer at address %p", len, node_layer);

    Node ** node_layer = malloc(sizeof(Node *)*len);

    cakelog("allocated space for %ld node pointers in node_layer at address %p", len, node_layer);

    int node_layer_index = 0;
    long left_index = 0;
    long right_index = 0;

    cakelog("entering main loop");

    while (left_index < len) {

        cakelog("top of loop");

        right_index = left_index + 1;

        cakelog("left_index = %ld, right_index = %ld", left_index, right_index);
        
        if (right_index < len) {

            cakelog("we have both left node and right node");

            int data_len = strlen(nodes[left_index]->data) + strlen(nodes[right_index]->data) + 1;

            cakelog("left node addr: %p, left node data: [%s], right node addr: %p, right node data: [%s]", nodes[left_index], nodes[left_index]->data, nodes[right_index], nodes[right_index]->data);
            
            char* data = malloc(sizeof(char) * data_len + 1);

            cakelog("allocated %ld bytes for new node data at %p", data_len + 1, data);

            char* digest = malloc(sizeof(char) * 129);

            cakelog("allocated 129 bytes for digest");

            // Store the actual data in the tree. Only doing this because this
            // is experimental and I want to do some checks at the end to prove
            // that its working. Clearly you wouldn't do this if you were
            // building a real Merkle Tree. One of the major advantages is that
            // is that it obsfuscates data and doesn't stick it right next to 
            // the hash like it was written by an idiot.
            strcpy(data, nodes[left_index]->data);
            strcat(data, nodes[right_index]->data);

            cakelog("new node data is: %s", data);
            cakelog("new node data len is: %ld", strlen(data));

            // We're going to store the digest as a hex string because I want to
            // do some rudimentary checking. We could probably just store
            // the unsigned char * binary version that openssl returns, though,
            // and only pull out the hex string right at the end for the root.
            strcpy(digest, nodes[left_index]->sha256_digest);
            strcat(digest, nodes[right_index]->sha256_digest);

            cakelog("new node digest is: %s", digest);

            Node * n = new_node(nodes[left_index], nodes[right_index], data, hexdigest(sha256(digest)));

            node_layer[node_layer_index] = n;

            cakelog("added node at address %p to node_layer with an index of %ld", n, node_layer_index);

            node_layer_index++;
        }
        else {
            // We only have a left leaf left (say that after eight pints)
            cakelog("we only have left node");
            cakelog("left node data: [%p]", nodes[left_index]);
            int data_len = strlen(nodes[left_index]->data) + 1;

            cakelog("length of new data: %ld", data_len);

            cakelog("left node addr: %p, left node data: [%s]", nodes[left_index], nodes[left_index]->data);

            char * data = malloc(sizeof(char) * data_len + 1);
            cakelog("allocated %ld bytes for new node data at %p", data_len + 1, data);

            char * digest = malloc(sizeof(char) * 65);
            cakelog("allocated %ld bytes for new node digest at %p", 65, digest);

            strcpy(data, nodes[left_index]->data);
            cakelog("new node data is: %s", data);
            cakelog("new node data len is: %ld", strlen(data));

            strcpy(digest, nodes[left_index]->sha256_digest);
            cakelog("new node digest is: %s", digest);

            Node * n = new_node(nodes[left_index], NULL, data, hexdigest(sha256(digest)));
            node_layer[node_layer_index] = n;

            cakelog("added node at address %p to node_layer with an index of %ld", n, node_layer_index);
            node_layer_index++;
        }
        left_index = right_index + 1;
    }

    // Recursive call
    cakelog("recursive call with nodelayer at %p and layer_index at %ld", node_layer, node_layer_index);
    return build_merkle_tree(node_layer, node_layer_index);
}

int main(int argc, char *argv[])
{
    // mtrace();
    cakelog_initialise("merkle_tree",false);

    char * words = read_dictionary_file("./test_data/ukenglish.txt");
    long word_count = get_word_count(words);
    Node ** leaves = build_leaves(words);
    Node * root = build_merkle_tree(leaves, word_count);
    printf("Root digest is: %s\n", root->sha256_digest);

    cakelog_stop();

}