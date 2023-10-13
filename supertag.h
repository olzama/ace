// note: the token lattice has tokens as edges and spots between tokens as vertices

// This simple module will receive the supertags from an external supertagger.
// It will then go over the lattice and only keep tokens for which the lexical type matches
// the one provided by the external supertagger.

#include "lattice-mapping.h"

#ifndef SUPERTAG_H
#define SUPERTAG_H

#define SIZE 10240

struct charspan
{
	int start;
	int end;
};

struct edgetag {
    char* vertex;
    char* supertag;
};

struct supertagger
{
	int ntags;
	char **tags;
	int hashed_tags[SIZE][SIZE];
	char *pretagged[SIZE][SIZE];
	struct charspan spans[SIZE][SIZE];
	struct edgetag tags_found[SIZE][SIZE];
	int sentence_lengths[SIZE];
	struct hash *tag_hash;
};

void **load_supertags(char *filename, struct supertagger *st);
void **load_word_ids(char *filename, struct supertagger *st);
void **load_spans(char *filename, struct supertagger *st);
void load_supertagger(char *pretaggedpath);
#endif
