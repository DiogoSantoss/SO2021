#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "state.h"
#include "../tecnicofs-api-constants.h"

inode_t inode_table[INODE_TABLE_SIZE];
pthread_rwlock_t lock; /* Used to prevent conflits while creating a new inode with inode_create() */

/**
 * Sleeps for synchronization testing.
 * @param cycles: number of cycles
*/
void insert_delay(int cycles) {
    for (int i = 0; i < cycles; i++) {}
}

/**
 * Initializes the i-nodes table.
*/
void inode_table_init() {

    /*lock for inode_create*/
    if(pthread_rwlock_init(&lock,NULL) != 0){          
        fprintf(stderr, "Error: rwlock create error\n");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < INODE_TABLE_SIZE; i++) {
        inode_table[i].nodeType = T_NONE;
        inode_table[i].data.dirEntries = NULL;
        inode_table[i].data.fileContents = NULL;
        if (pthread_rwlock_init(&inode_table[i].rwl, NULL) !=0){
            fprintf(stderr, "Error: rwlock create error\n");
            exit(EXIT_FAILURE);
        }
    }
}

/**
 * Releases the allocated memory for the i-nodes tables.
*/
void inode_table_destroy() {

    /*lock for inode_create*/
    if(pthread_rwlock_destroy(&lock) != 0){
        fprintf(stderr, "Error: rwlock destroy error\n");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < INODE_TABLE_SIZE; i++) {
        if(pthread_rwlock_destroy(&inode_table[i].rwl) != 0){
            fprintf(stderr, "Error: rwlock destroy error\n");
            exit(EXIT_FAILURE);
        }
        if (inode_table[i].nodeType != T_NONE) {
            /* as data is an union, the same pointer is used for both dirEntries and fileContents */
            /* just release one of them */
	        if (inode_table[i].data.dirEntries)
                free(inode_table[i].data.dirEntries);
        }
    }
}

/**
 * Creates a new i-node in the table with the given information.
 * @param nType: the type of the node (file or directory)
 * @return inumber of FAIL
*/
int inode_create(type nType) {
    /* Used for testing synchronization speedup */
    insert_delay(DELAY);

    if(pthread_rwlock_wrlock(&lock) != 0){
			fprintf(stderr, "Error: wrlock lock error\n");
			exit(EXIT_FAILURE);
	}

    for (int inumber = 0; inumber < INODE_TABLE_SIZE; inumber++) {
        if (inode_table[inumber].nodeType == T_NONE) {
            inode_table[inumber].nodeType = nType;

            if (nType == T_DIRECTORY) {
                /* Initializes entry table */
                inode_table[inumber].data.dirEntries = malloc(sizeof(DirEntry) * MAX_DIR_ENTRIES);
                
                for (int i = 0; i < MAX_DIR_ENTRIES; i++) {
                    inode_table[inumber].data.dirEntries[i].inumber = FREE_INODE;
                }
            }
            else {
                inode_table[inumber].data.fileContents = NULL;
            }
            if(pthread_rwlock_unlock(&lock) != 0){
                fprintf(stderr, "Error: rwlock unlock error\n");
                exit(EXIT_FAILURE);
            }
            return inumber;
        }
    }
    if(pthread_rwlock_unlock(&lock) != 0){
        fprintf(stderr, "Error: rwlock unlock error\n");
        exit(EXIT_FAILURE);
    }
    return FAIL;
}

/**
 * Deletes the i-node.
 * @param inumber: identifier of the i-node
 * @return SUCCESS or FAIL
*/
int inode_delete(int inumber) {
    /* Used for testing synchronization speedup */
    insert_delay(DELAY);

    if ((inumber < 0) || (inumber > INODE_TABLE_SIZE) || (inode_table[inumber].nodeType == T_NONE)) {
        printf("inode_delete: invalid inumber\n");
        return FAIL;
    }

    inode_table[inumber].nodeType = T_NONE;
    /* see inode_table_destroy function */
    if (inode_table[inumber].data.dirEntries)
        free(inode_table[inumber].data.dirEntries);

    return SUCCESS;
}

/**
 * Copies the contents of the i-node into the arguments.
 * Only the fields referenced by non-null arguments are copied.
 * @param inumber: identifier of the i-node
 * @param nType: pointer to type
 * @param data: pointer to data
 * @return SUCCESS or FAIL
*/
int inode_get(int inumber, type *nType, union Data *data) {
    /* Used for testing synchronization speedup */
    insert_delay(DELAY);

    if ((inumber < 0) || (inumber > INODE_TABLE_SIZE) || (inode_table[inumber].nodeType == T_NONE)) {
        printf("inode_get: invalid inumber %d\n", inumber);
        return FAIL;
    }

    if (nType)
        *nType = inode_table[inumber].nodeType;

    if (data)
        *data = inode_table[inumber].data;

    return SUCCESS;
}

/**
 * Resets an entry for a directory.
 * @param inumber: identifier of the i-node
 * @param sub_inumber: identifier of the sub i-node entry
 * @return SUCCESS or FAIL
*/
int dir_reset_entry(int inumber, int sub_inumber) {
    /* Used for testing synchronization speedup */
    insert_delay(DELAY);

    if ((inumber < 0) || (inumber > INODE_TABLE_SIZE) || (inode_table[inumber].nodeType == T_NONE)) {
        printf("inode_reset_entry: invalid inumber\n");
        return FAIL;
    }

    if (inode_table[inumber].nodeType != T_DIRECTORY) {
        printf("inode_reset_entry: can only reset entry to directories\n");
        return FAIL;
    }

    if ((sub_inumber < FREE_INODE) || (sub_inumber > INODE_TABLE_SIZE) || (inode_table[sub_inumber].nodeType == T_NONE)) {
        printf("inode_reset_entry: invalid entry inumber\n");
        return FAIL;
    }

    for (int i = 0; i < MAX_DIR_ENTRIES; i++) {
        if (inode_table[inumber].data.dirEntries[i].inumber == sub_inumber) {
            inode_table[inumber].data.dirEntries[i].inumber = FREE_INODE;
            inode_table[inumber].data.dirEntries[i].name[0] = '\0';
            return SUCCESS;
        }
    }
    return FAIL;
}

/**
 * Adds an entry to the i-node directory data.
 * @param inumber: identifier of the i-node
 * @param sub_inumber: identifier of the sub i-node entry
 * @param sub_name: name of the sub i-node entry 
 * @return SUCCESS or FAIL
*/
int dir_add_entry(int inumber, int sub_inumber, char *sub_name) {
    /* Used for testing synchronization speedup */
    insert_delay(DELAY);

    if ((inumber < 0) || (inumber > INODE_TABLE_SIZE) || (inode_table[inumber].nodeType == T_NONE)) {
        printf("inode_add_entry: invalid inumber\n");
        return FAIL;
    }

    if (inode_table[inumber].nodeType != T_DIRECTORY) {
        printf("inode_add_entry: can only add entry to directories\n");
        return FAIL;
    }

    if ((sub_inumber < 0) || (sub_inumber > INODE_TABLE_SIZE) || (inode_table[sub_inumber].nodeType == T_NONE)) {
        printf("inode_add_entry: invalid entry inumber\n");
        return FAIL;
    }

    if (strlen(sub_name) == 0 ) {
        printf("inode_add_entry: \
               entry name must be non-empty\n");
        return FAIL;
    }
    
    for (int i = 0; i < MAX_DIR_ENTRIES; i++) {
        if (inode_table[inumber].data.dirEntries[i].inumber == FREE_INODE) {
            inode_table[inumber].data.dirEntries[i].inumber = sub_inumber;
            strcpy(inode_table[inumber].data.dirEntries[i].name, sub_name);
            return SUCCESS;
        }
    }
    return FAIL;
}

/**
 * Prints the i-nodes table.
 * @param fp: pointer to file
 * @param inumber: identifier of the i-node
 * @param name: pointer to the name of current file/dir
*/
void inode_print_tree(FILE *fp, int inumber, char *name) {
    if (inode_table[inumber].nodeType == T_FILE) {
        fprintf(fp, "%s\n", name);
        return;
    }

    if (inode_table[inumber].nodeType == T_DIRECTORY) {
        fprintf(fp, "%s\n", name);
        for (int i = 0; i < MAX_DIR_ENTRIES; i++) {
            if (inode_table[inumber].data.dirEntries[i].inumber != FREE_INODE) {
                char path[MAX_FILE_NAME];
                if (snprintf(path, sizeof(path), "%s/%s", name, inode_table[inumber].data.dirEntries[i].name) > sizeof(path)) {
                    fprintf(stderr, "truncation when building full path\n");
                }
                inode_print_tree(fp, inode_table[inumber].data.dirEntries[i].inumber, path);
            }
        }
    }
}

/**
 * Locks inode.
 * @param inumber: identifier of the i-node
 * @param flag: type of lock
 * @return SUCCESS or FAIL
*/
int inode_lock(int inumber,char* flag) {
    if ((inumber < 0) || (inumber > INODE_TABLE_SIZE) || (inode_table[inumber].nodeType == T_NONE)) {
        printf("inode_get_lock: invalid inumber %d\n", inumber);
        return FAIL;
    }

    if(!strcmp("w",flag)){
        if(pthread_rwlock_wrlock(&inode_table[inumber].rwl) != 0){
            fprintf(stderr, "Error: lock wrlock error\n");
            exit(EXIT_FAILURE);
        }
    }
    else if(!strcmp("r",flag)){
        if(pthread_rwlock_rdlock(&inode_table[inumber].rwl) != 0){
            fprintf(stderr, "Error: lock rdlock error\n");
            exit(EXIT_FAILURE);
        }
    }
    else if(!strcmp("mw",flag)){
        return pthread_rwlock_trywrlock(&inode_table[inumber].rwl);
    }
    else if(!strcmp("mr",flag)){
        return pthread_rwlock_tryrdlock(&inode_table[inumber].rwl);
    }
    else
        exit(EXIT_FAILURE);
    
    return 1;
}

/**
 * Unlocks inode.
 * @param inumber: identifier of the i-node
 * @return SUCESS
*/
int inode_unlock(int inumber){
    if(pthread_rwlock_unlock(&inode_table[inumber].rwl) != 0){
        fprintf(stderr, "Error: rwlock unlock error\n");
        exit(EXIT_FAILURE);
    }
    return SUCCESS;
}

/**
 * Returns lock from inumber.
 * @param inumber: identifier of the i-node
 * @return i-node lock
*/
pthread_rwlock_t* getlock(int inumber){
    if ((inumber < 0) || (inumber > INODE_TABLE_SIZE) || (inode_table[inumber].nodeType == T_NONE)) {
        printf("getlock: invalid inumber %d\n", inumber);
        return NULL;
    }
    return &inode_table[inumber].rwl;
}