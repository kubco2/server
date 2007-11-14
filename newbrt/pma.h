#ifndef PMA_H
#define PMA_H

#include "brttypes.h"
#include "ybt.h"
#include "yerror.h"
#include "../include/db.h"
#include "log.h"

/* An in-memory Packed Memory Array dictionary. */
/* There is a built-in-cursor. */

/* All functions return 0 on success. */

typedef struct pma *PMA;
typedef struct pma_cursor *PMA_CURSOR;

/* compare 2 DBT's
   return a value < 0, = 0, > 0 if a < b, a == b, a > b respectively */
typedef int (*pma_compare_fun_t)(DB *, const DBT *a, const DBT *b);

int pma_create(PMA *, pma_compare_fun_t compare_fun, int maxsize);

/* set the duplicate mode 
   0 -> no duplications, DB_DUP, DB_DUPSORT */
int pma_set_dup_mode(PMA pma, int mode);

/* set the duplicate compare function */
int pma_set_dup_compare(PMA pma, pma_compare_fun_t dup_compare_fun);

/* verify the integrity of a pma */
void pma_verify(PMA pma, DB *db);

/* returns 0 if OK.
 * You must have freed all the cursors, otherwise returns nonzero and does nothing. */
int pma_free (PMA *);

int  pma_n_entries (PMA);

/* Returns an error if the key is already present. */
/* The values returned should not be modified.by the caller. */
/* Any cursors should be updated. */
/* Duplicates the key and keylen. */
//enum pma_errors pma_insert (PMA, bytevec key, ITEMLEN keylen, bytevec data, ITEMLEN datalen);
// The DB pointer is there so that the comparison function can be called.
enum pma_errors pma_insert (PMA, DBT*, DBT*, DB*, TOKUTXN txn, DISKOFF, u_int32_t /*random for fingerprint */, u_int32_t */*fingerprint*/);
/* This returns an error if the key is NOT present. */
int pma_replace (PMA, bytevec key, ITEMLEN keylen, bytevec data, ITEMLEN datalen);
/* This returns an error if the key is NOT present. */
int pma_delete (PMA, DBT *, DB*, u_int32_t /*random for fingerprint*/, u_int32_t */*fingerprint*/);

int pma_insert_or_replace (PMA pma, DBT *k, DBT *v,
			   int *replaced_v_size, /* If it is a replacement, set to the size of the old value, otherwise set to -1. */
			   DB *db, TOKUTXN txn, DISKOFF,
			   u_int32_t /*random for fingerprint*/, u_int32_t */*fingerprint*/);


/* Exposes internals of the PMA by returning a pointer to the guts.
 * Don't modify the returned data.  Don't free it. */
enum pma_errors pma_lookup (PMA, DBT*, DBT*, DB*);

/*
 * The kv pairs in the original pma are split into 2 equal sized sets
 * and moved to the leftpma and rightpma.  The size is determined by
 * the sum of the keys and values. the left and right pma's must be
 * empty.
 *
 * origpma - the pma to be split
 * leftpma - the pma assigned keys <= pivot key
 * rightpma - the pma assigned keys > pivot key
 */
int pma_split(PMA origpma,  unsigned int *origpma_size,
	      PMA leftpma,  unsigned int *leftpma_size,  u_int32_t leftrand4sum,  u_int32_t *leftfingerprint,
	      PMA rightpma, unsigned int *rightpma_size, u_int32_t rightrand4sum, u_int32_t *rightfingerprint);

/*
 * Insert several key value pairs into an empty pma.  
 * Doesn't delete any existing keys (even if they are duplicates)
 * Requires: The keys are sorted
 *
 * pma - the pma that the key value pairs will be inserted into.
 *      must be empty with no cursors.
 * keys - an array of keys
 * vals - an array of values
 * n_newpairs - the number of key value pairs
 */
int pma_bulk_insert(PMA pma, DBT *keys, DBT *vals, int n_newpairs, u_int32_t rand4sem, u_int32_t *fingerprint);

/* Move the cursor to the beginning or the end or to a key */
int pma_cursor (PMA, PMA_CURSOR *);
int pma_cursor_free (PMA_CURSOR*);

/* get the pma that a pma cursor is bound to */
int pma_cursor_get_pma(PMA_CURSOR c, PMA *pma);
int pma_cursor_set_position_last (PMA_CURSOR c);
int pma_cursor_set_position_first (PMA_CURSOR c);
int pma_cursor_set_position_next (PMA_CURSOR c); /* Requires the cursor is init'd.  Returns DB_NOTFOUND if we fall off the end. */
int pma_cursor_set_position_prev (PMA_CURSOR c);

/* get the key and data under the cursor */
int pma_cursor_get_current(PMA_CURSOR c, DBT *key, DBT *val);

/* set the cursor to the matching key and value pair */
int pma_cursor_set_both(PMA_CURSOR c, DBT *key, DBT *val, DB *db);

/* move the cursor to the kv pair matching the key */
int pma_cursor_set_key(PMA_CURSOR c, DBT *key, DB *db);

/* set the cursor to the smallest key in the pma >= key */
int pma_cursor_set_range(PMA_CURSOR c, DBT *key, DB *db);

/* delete the key value pair under the cursor, return the size of the pair */
int pma_cursor_delete_under(PMA_CURSOR c, int *kvsize);

/* get the last key and value in the pma */
int pma_get_last(PMA pma, DBT *key, DBT *val);

int pma_random_pick(PMA, bytevec *key, ITEMLEN *keylen, bytevec *data, ITEMLEN *datalen);

int pma_index_limit(PMA);
int pmanode_valid(PMA,int);
bytevec pmanode_key(PMA,int);
ITEMLEN pmanode_keylen(PMA,int);
bytevec pmanode_val(PMA,int);
ITEMLEN pmanode_vallen(PMA,int);

void pma_iterate (PMA, void(*)(bytevec,ITEMLEN,bytevec,ITEMLEN, void*), void*);

#define PMA_ITERATE(table,keyvar,keylenvar,datavar,datalenvar,body) ({ \
  int __i;                                                             \
  for (__i=0; __i<pma_index_limit(table); __i++) {		       \
    if (pmanode_valid(table,__i)) {                                    \
      bytevec keyvar = pmanode_key(table,__i);                         \
      ITEMLEN keylenvar = pmanode_keylen(table,__i);                   \
      bytevec datavar = pmanode_val(table, __i);                       \
      ITEMLEN datalenvar = pmanode_vallen(table, __i);                 \
      body;                                                            \
} } })

int keycompare (bytevec key1, ITEMLEN key1len, bytevec key2, ITEMLEN key2len);

void pma_verify_fingerprint (PMA pma, u_int32_t rand4fingerprint, u_int32_t fingerprint);

#endif
