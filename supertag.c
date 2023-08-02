#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
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
// #define	DEBUG(x...)

int enable_supertagging = 0;
char *supertags_path = NULL;

int st_lookup_tag(struct supertagger *st, char *tag, int insert)
{
	int *tagi = hash_find(st->tag_hash, tag);
	if (!tagi)
	{
		if (!insert)
			return -1;
		tagi = malloc(sizeof(int));
		*tagi = st->ntags++;
		st->tags = realloc(st->tags, sizeof(char *) * st->ntags);
		st->tags[*tagi] = strdup(tag);
		hash_add(st->tag_hash, st->tags[*tagi], tagi);
	}
	return *tagi;
}

void **load_spans(char *filename, struct supertagger *st)
{
	FILE *fp = fopen(filename, "r");
	if (fp == NULL)
	{
		fprintf(stderr, "Error: cannot open file %s\n", filename);
		exit(EXIT_FAILURE);
	}

	char *line = NULL;
	size_t len = 0;
	int s_i = 0; // sentence number

	while ((getline(&line, &len, fp)) != -1)
	{
		int i = 0; // tuple number
		char *p = strdup(line);
		char *tuple_str = strtok(p, "\t\n");

		while (tuple_str != NULL)
		{
			int start, end;
			sscanf(tuple_str, "(%d, %d)", &start, &end);

			// Store the tuple in the spans array
			st->spans[s_i][i].start = start;
			st->spans[s_i][i].end = end;
			i++;

			tuple_str = strtok(NULL, "\t\n");
		}
		s_i++;
	}
	fclose(fp);
	if (line)
		free(line);
}

void **load_supertags(char *filename, struct supertagger *st)
{
	FILE *fp = fopen(filename, "r");
	if (fp == NULL)
	{
		fprintf(stderr, "Error: cannot open file %s\n", filename);
		exit(EXIT_FAILURE);
	}

	char *line = NULL;
	size_t len = 0;
	int read;
	int s_i = 0; // sentence number

	while ((read = getline(&line, &len, fp)) != -1)
	{
		int i = 0; // token number
		char *p = strdup(line);
		char *tag = strtok(p, ",\n");

		while (tag != NULL)
		{
			char *s = strdup(tag);
			while (*s == ' ')
			{
				s++;
			}
			st->pretagged[s_i][i] = s;
			i++;
			tag = strtok(NULL, ",\n");
		}
		st->sentence_lengths[s_i] = i;
		s_i++;
	}
	fclose(fp);
	if (line)
		free(line);
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

struct supertagger *the_supertagger;

void load_supertagger(char *dir)
{
	char *hash_name = "supertag_hash";
	char *supertags_file = malloc(strlen(dir) + strlen("predictions.txt") + 1);
	char *spans_file = malloc(strlen(dir) + strlen("spans.txt") + 1);
	strcpy(supertags_file, dir);
	strcpy(spans_file, dir);
	strcat(supertags_file, "predictions.txt");
	strcat(spans_file, "spans.txt");
	struct supertagger *st = calloc(sizeof(struct supertagger), 1);
	st->ntags = 0;
	st->tag_hash = hash_new(hash_name);
	// Initialize the spans to -1:
	for (int i = 0; i < SIZE; i++)
	{
		for (int j = 0; j < SIZE; j++)
		{
			st->spans[i][j].start = -1;
			st->spans[i][j].end = -1;
		}
	}
	load_supertags(supertags_file, st);
	load_spans(spans_file, st);
	the_supertagger = st;
}

char *find_supertag(struct supertagger *st, int s_i, struct lattice_edge *e)
{
	if (e->token)
	{
		int e_start = e->token->cfrom;
		int e_end = e->token->cto;
		for (int i = 0; i < st->sentence_lengths[s_i]; i++)
		{
			struct charspan span = st->spans[s_i][i];
			if (e_start == span.start && e_end == span.end)
			{
				return st->pretagged[s_i][i];
			}
		}
	}
	return NULL;
}

// lexical chart has an edge for each possible tag of each possible token
// OZ: This function is based on ubertag_lattice in ubertag.c.
// For now, this function simply relies on obtaining a list of supertags
// for each terminal (word) in the input sentence. This list comes from an "oracle"
// in the sense that the supertagger was already run externally and provided
// exactly one tag per word. The word can consist of more than one token.
// In such cases, the same supertag should be used for all tokens in the word.
void supertag_lattice(struct supertagger *st, struct lattice *ll, int s_i)
{
	// sort_lattice(ll);
	int new_nedges = 0;
	int i_true;
	for (i_true = 0; i_true < ll->nedges; i_true++)
	{
		struct lattice_edge *e = ll->edges[i_true];
		char *tagname = e->edge->lex->lextype->name;
		// int position = e->from->id;
		char *supertag = find_supertag(st, s_i, e);
		// int edge_tag = st_lookup_tag(st, tagname, 1);
		// int desired_tag = st_lookup_tag(st, supertag, 1);
		// if (edge_tag == desired_tag)
		if (supertag && strcmp(supertag, tagname) == 0)
		{
			DEBUG("KEEPING %s %s vtx [%d-%d]\n", tagname, e->edge->lex->word, e->edge->from, e->edge->to);
			ll->edges[new_nedges++] = ll->edges[i_true];
			// printf("Lexical chart:\n");
			// print_lexical_chart(ll);
		}
		else
		{
			DEBUG("DISCARDING %s\n", tagname);
			// printf("Lexical chart:\n");
			// print_lexical_chart(ll);
		}
	}
	ll->nedges = new_nedges;
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
