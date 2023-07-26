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

// #define DEBUG(x...) \
// 	do              \
// 	{               \
// 		printf(x);  \
// 	} while (0)
#define	DEBUG(x...)

int enable_supertagging = 0;
char *supertags_path = NULL;

// The tag comes in single quotes, but in the lexical chart there won't be any, so we strip them.
// static char * normalize_tag(char * supertag)
// {
// 	char * norm_tag = malloc(strlen(supertag)); 
// 	strcpy(norm_tag, supertag);
// 	norm_tag++;
// 	norm_tag[strlen(supertag) - 2] = '\0';	
// 	return norm_tag;
// }

static char * normalize_tag(char * supertag)
{
    size_t len = strlen(supertag);
    char * norm_tag = malloc(len + 1);
    strncpy(norm_tag, supertag + 1, len - 2);
    norm_tag[len - 2] = '\0';
    return norm_tag;
}


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

//Load a list of lists of numbers from a file.
// Format: [1, 2, 3, 4, 5, 6, 6, 7]\n[1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 10, 12]\n... etc.
// Store the numbers in a 2D array of ints in the supertagger struct.
void **load_word_ids(char *filename, struct supertagger *st)
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
		if (line[0] == '[')
		{
			int i = 0; // token number
			char *p = line + 1;
			char *q = strchr(p, ']');
			if (q == NULL)
			{
				fprintf(stderr, "Error: invalid supertag list format\n");
				exit(EXIT_FAILURE);
			}
			*q = '\0';
			char *word_id = strtok(p, ", ");
			while (word_id != NULL)
			{
				st->word_ids[s_i][i] = atoi(word_id);
				DEBUG("Added word id %s\n", st->word_ids[s_i][i]);
				word_id = strtok(NULL, ", ");
				i++;
			}
			st->sentence_lengths[s_i] = i;
		}
		s_i++;
	}

	fclose(fp);
	if (line)
		free(line);
}

// Load a list of lists of supertags from a file
// Format: ['tag1', 'tag2', ...]\n['tag1', 'tag2', ...]\n...
// Store the tags in the supertagger struct.
// Note: written for the most part by Bing Chat
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
        if (line[0] == '[')
        {
			int i = 0; // token number
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
                char *norm_tag = normalize_tag(tag);
                //st->hashed_tags[s_i][i] = st_lookup_tag(st, norm_tag, 1);
				//char *s = malloc(len + strlen(norm_tag) + 1);
				char *s = malloc(strlen(norm_tag) + 1);
                //strcpy(s, norm_tag);
				strncpy(s, norm_tag, strlen(norm_tag) + 1);
                st->pretagged[s_i][i] = s;
				DEBUG("Added supertag %s\n", st->pretagged[s_i][i]);
                //DEBUG("Added supertag %s hashed to bucket %d\n", st->pretagged[s_i][i], st->hashed_tags[i]);
                tag = strtok(NULL, ", ");
				i++;
            }
			st->sentence_lengths[s_i] = i;
        }
		s_i++;
    }

    fclose(fp);
    if (line)
        free(line);
}

// Load a list of lists of supertags from a file
// Format: ['tag1', 'tag2', ...]\n['tag1', 'tag2', ...]\n...
// Return: a list of lists of supertags
// Note: written for the most part by Bing Chat
/*void **load_supertags2(char *filename, struct supertagger *st)
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
				char *norm_tag = normalize_tag(tag);
				st->hashed_tags[i] = st_lookup_tag(st, norm_tag, 1);
				char *s = malloc(len + strlen(norm_tag) + 1);
				strcpy(s, norm_tag);
				st->pretagged[i++] = s;
				DEBUG("Added supertag %s hashed to bucket %d\n", st->pretagged[i-1], st->hashed_tags[i-1]);
				tag = strtok(NULL, ", ");
			}
		}
	}

	fclose(fp);
	if (line)
		free(line);
}*/


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

void load_supertagger(char* dir)
{
	char * hash_name = "supertag_hash";
	char * supertags_file = malloc(strlen(dir) + strlen("predictions.txt") + 1);
	char * word_ids_file = malloc(strlen(dir) + strlen("word_nums.txt") + 1);
	strcpy(supertags_file, dir);
	strcpy(word_ids_file, dir);
	strcat(supertags_file, "predictions.txt");
	strcat(word_ids_file, "word_nums.txt");
	struct supertagger *st = calloc(sizeof(struct supertagger),1);
	st->ntags = 0;
	st->tag_hash = hash_new(hash_name);
	load_supertags(supertags_file, st);
	load_word_ids(word_ids_file, st);
	the_supertagger = st;
}


// True if the supertagger has a supertag for the given token at the any position
bool find_candidate(struct supertagger *st, int s_i, int position, char *tagname)
{
	char *supertag = NULL;
	int i;
	for (i = 0; i < st->sentence_lengths[s_i]; i++) {
		supertag = st->pretagged[s_i][i];
		if (strcmp(tagname, supertag) == 0) {
			return 1;
		}
	}
	return 0;
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
	//sort_lattice(ll);
	int	new_nedges = 0;
	int i_true;
	for(i_true=0;i_true<ll->nedges;i_true++)
	{
		struct lattice_edge	*e = ll->edges[i_true];
		char	*tagname = e->edge->lex->lextype->name;
		//char *stemname = (*e->edge->lex->stem)->name;
		//printf("word: %s\n", stemname);
		int position = e->from->id;
		char *supertag = NULL;
		bool keep = 0;
		// If there are more tokens in a sentence than terminals, need to iterate through the list of tags
		// every time to see if there is one that matches both the lexical type of the current token and the
		// starting position.
		if (st->sentence_lengths[s_i] != ll->nvertices - 1) {
			keep = find_candidate(st, s_i, position, tagname);
		} // otherwise, can take the tag directly based on the token starting position.
		else {
			supertag = st->pretagged[s_i][position];
		}
		//int edge_tag = st_lookup_tag(st, tagname, 1); 
		//int desired_tag = st_lookup_tag(st, supertag, 1);					
		if (keep || (supertag && strcmp(supertag, tagname) == 0)) // OZ: This should instead be using hash. Just testing for now.
		//if (edge_tag == desired_tag)
		{
			DEBUG("KEEPING %s\n", tagname);
			ll->edges[new_nedges++] = ll->edges[i_true];
			//printf("Lexical chart:\n");
			//print_lexical_chart(ll);
		}
		else
		{
			DEBUG("DISCARDING %s\n", tagname);
			//printf("Lexical chart:\n");
			//print_lexical_chart(ll);
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
