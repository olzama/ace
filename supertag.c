#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <wctype.h>
#include "unicode.h"
#include "token.h"
#include "chart.h"
#include "rule.h"
#include "lexicon.h"
#include "hash.h"
#include "supertag.h"
#include "ubertag.h"
#include "lattice-mapping.h"
#include "tdl.h"
#include "freeze.h"

#define DEBUG(x...) \
	do              \
	{               \
		printf(x);  \
	} while (0)
//#define	DEBUG(x...)

int enable_supertagging = 0;


int	st_lookup_tag(struct supertagger *st, char	*tag, int	insert)
{
	int	*tagi = hash_find(st->tag_hash,tag);
	if(!tagi)
	{
		if(!insert)return -1;
		tagi = malloc(sizeof(int));
		*tagi = st->ntags++;
		st->tags = realloc(st->tags, sizeof(char*)*st->ntags);
		st->tags[*tagi] = strdup(tag);
		hash_add(st->tag_hash, st->tags[*tagi], tagi);
	}
	return *tagi;
}


// Load a list of supertags from a file
// Format: ['tag1', 'tag2', ...]
// Return: a list of supertags
// Note: written for the most part by Bing Chat
void **load_supertags(char *filename, struct supertagger *st)
{
	FILE *fp = fopen(filename, "r");
	if (fp == NULL)
	{
		fprintf(stderr, "Error: cannot open file %s\n", filename);
		exit(EXIT_FAILURE);
	}

	//int supertags[SIZE];
	char *supertag_names[SIZE];
	char *line = NULL;
	size_t len = 0;
	int read;
	int i = 0;
	while ((read = getline(&line, &len, fp)) != -1)
	{
		if (line[0] == '[')
		{
			char *p = line + 1;
			char *q = strchr(p, ']');
			if (q == NULL)
			{
				fprintf(stderr, "Error: invalid supertag list format\n");
				exit(EXIT_FAILURE);
			}
			*q = '\0';
			char *tag = strtok(p, ",");
			while (tag != NULL)
			{
				st->hashed_tags[i] = st_lookup_tag(st, tag, 1);
				// supertags = (char **)realloc(supertags, sizeof(char *) * (SIZE + 1));
				// supertags[SIZE - 1] = strdup(tag);
				char *s = malloc(len + strlen(tag) + 1);
				strcpy(s, tag);
				st->pretagged[i++] = s;
				printf("Added supertag %s hashed to bucket %d\n", st->pretagged[i-1], st->hashed_tags[i-1]);
				tag = strtok(NULL, ", ");
			}
		}
	}

	fclose(fp);
	if (line)
		free(line);

	//*st->hashed_tags = supertags;
	//*st->pretagged = supertag_names;
}


// Get the n-th item from the list of supertags
char *get_supertag(char **supertags, int n)
{
	if (n < 0 || n >= SIZE)
	{
		fprintf(stderr, "Error: supertag index out of range\n");
		exit(EXIT_FAILURE);
	}
	return supertags[n];
}

// Initialize the supertagger.

struct supertagger *load_supertagger(char* filename)
{
	char * hash_name = "supertag_hash";
	struct supertagger *st = calloc(sizeof(struct supertagger),1);
	st->ntags = 0;
	st->tag_hash = hash_new(hash_name);
	load_supertags(filename, st);
	// Debug: print all the tags in the st->pretagged list:
	for (int i = 0; i < st->ntags; i++) {
		printf("%s\n", st->pretagged[i]);
	}
	return st;
}

// lexical chart has an edge for each possible tag of each possible token
void supertag_lattice(struct supertagger *st, struct lattice *ll)
{
	int i;
	int		tags[ll->nedges];
	char	*tagnames[ll->nedges];
	for(i=0;i<ll->nedges;i++)
	{
		struct lattice_edge	*e = ll->edges[i];
		char	*tagname = e->edge->lex->lextype->name;
		int position = e->from->id;
		char *supertag = st->pretagged[position];
		// The tag comes in single quotes, but in the lexical chart there won't be any, so we strip them.
		char * norm_tag = malloc(strlen(supertag)); 
		strcpy(norm_tag, supertag);
		norm_tag++;
    	norm_tag[strlen(supertag) - 2] = '\0';					
		if (strcmp(norm_tag, tagname) == 0) // OZ: This should instead be using hash. Just testing for now.
		{
			printf("KEEPING %s\n", tagname);
		}
		else
		{
			printf("DISCARDING %s\n", tagname);
		}
		
	}
}


// Destructor for supertagger:
// void free_supertagger(struct supertagger *st)
// {
// 	for (int i = 0; i < array_size(st->pretagged); i++)
// 	{
// 		free(st->pretagged[i]);
// 	}
// 	free(st->pretagged);
// 	free(st);
// }
