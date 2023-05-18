// note: the token lattice has tokens as edges and spots between tokens as vertices

// This simple module will receive the supertags from an external supertagger.
// It will then go over the lattice and only keep tokens for which the lexical type matches
// the one provided by the external supertagger. 

#include	"lattice-mapping.h"

#ifndef	SUPERTAG_H
#define	SUPERTAG_H

#define SIZE 10240

struct supertagger
{
	int		ntags;
	char	**tags;
	int hashed_tags[SIZE];
	char *pretagged[SIZE];
	struct hash *tag_hash;				
};

void **load_supertags(char *filename, struct supertagger *st);
struct supertagger	*load_supertagger(char	*pretaggedpath);
//void	supertag_lattice(struct supertagger	*st, struct lattice	*ll, double	thresh);
//void	free_supertagger(struct supertagger	*st);
//int load_supertagging();
#endif
