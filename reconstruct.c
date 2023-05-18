#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<unistd.h>
#include	<assert.h>

#include	"reconstruct.h"
#include	"token.h"
#include	"freeze.h"

int	nunifications;

int	reconstruction_noise_level = 1;

struct lexeme	*get_lex_by_name_hash(char	*name)
{
	static struct hash	*h = NULL;
	if(!h)h = hash_new("lexicon");

	struct lexeme	*l = hash_find(h, name);
	if(l)return l;

	l = get_lex_by_name(name);
	if(l) { hash_add(h, strdup(name), l); return l; }

	int i;
	for(i=0;i<ngeneric_les;i++)
		if(!strcmp(name, generic_les[i]->word))
			{ hash_add(h, strdup(name), generic_les[i]); return generic_les[i]; }

	if(reconstruction_noise_level)fprintf(stderr, "WARNING: unknown lexeme '%s'!\n", name);
	return NULL;
}

struct type	*tokstr_type_of_string(char	*kp)
{
	if(!*kp)return NULL;
	if(*kp == '"')
	{
		char	*p = kp+1;
		while(*p != '"' && *p)
		{
			if(*p=='\\' && p[1])p+=2;
			else p+=1;
		}
		if(*p=='"')
		{
			char	s = p[1];
			p[1] = 0;
			struct type	*t = lookup_type(strdup(kp));
			p[1] = s;
			return t;
		}
		else return NULL;
	}
	else
	{
		// plain old type
		char	*sp = kp;
		while(*sp && *sp!=' ' && *sp!='\n' && *sp!='\t')sp++;
		char	s = *sp;
		*sp = 0;
		struct type	*t = lookup_type(kp);
		*sp = s;
		return t;
	}
}

char	*tokstr_dup_value(char	*kp)
{
	if(!*kp)return NULL;
	if(*kp == '"')
	{
		char	*p = kp+1;
		while(*p != '"' && *p)
		{
			if(*p=='\\' && p[1])p+=2;
			else p+=1;
		}
		if(*p=='"')
		{
			char	s = p[1];
			p[1] = 0;
			char	*returnme = strdup(kp);
			p[1] = s;
			return returnme;
		}
		else return NULL;
	}
	else
	{
		// plain old type
		char	*sp = kp;
		while(*sp && *sp!=' ' && *sp!='\n' && *sp!='\t')sp++;
		char	s = *sp;
		*sp = 0;
		char	*returnme = strdup(kp);
		*sp = s;
		return returnme;
	}
}

char	*tokstr_parse_value_of_as_string(char	*tokstr, char	*key)
{
	//printf("looking for `%s' in `%s'\n", key, tokstr);
	char	*kp = strstr(tokstr, key);
	if(!kp)return NULL;

	kp += strlen(key);
	if(*kp == '#')
	{
		char	*p = kp+1;
		while(*p && *p != ' ' && *p != '=')p++;
		if(*p=='=')return tokstr_dup_value(p+1);
		char	buffer[1024];
		sprintf(buffer, "%.*s=", (int)(p-kp), kp);
		return tokstr_parse_value_of_as_string(tokstr, buffer);
	}
	else return tokstr_dup_value(kp);
}

struct type	*tokstr_parse_value_of(char	*tokstr, char	*key)
{
	//printf("looking for `%s' in `%s'\n", key, tokstr);
	char	*kp = strstr(tokstr, key);
	if(!kp)return NULL;

	kp += strlen(key);
	if(*kp == '#')
	{
		char	*p = kp+1;
		while(*p && *p != ' ' && *p != '=')p++;
		if(*p=='=')return tokstr_type_of_string(p+1);
		char	buffer[1024];
		sprintf(buffer, "%.*s=", (int)(p-kp), kp);
		return tokstr_parse_value_of(tokstr, buffer);
	}
	else return tokstr_type_of_string(kp);
}

void	set_token_dg_feat(struct dg	*dg, struct type	*cargval, char	*feat)
{
	struct dg	*cdg;
	cdg = dg_hop(dg, lookup_fname(feat));
	if(cdg)cdg->type = cdg->xtype = cargval;
}

struct dg	*reconstruct_token(char	*tokstr, struct dg	*dg)
{
	struct type	*cargval = tokstr_parse_value_of(tokstr, "+CARG ");
	struct type	*predval = tokstr_parse_value_of(tokstr, "+PRED ");

	if(predval)set_token_dg_feat(dg, predval, "+PRED");
	if(cargval)set_token_dg_feat(dg, cargval, "+CARG");

	// this would be done for us by build_token_dag if we managed to get
	// the right values on the struct token -- however,
	// they are not present (without parsing) on the struct tree.
	// a better idea would be to do this tokstr_pars-ing before calling build_token_dag...
	struct type	*fromval = tokstr_parse_value_of(tokstr, "+FROM ");
	struct type	*toval = tokstr_parse_value_of(tokstr, "+TO ");
	if(fromval)set_token_dg_feat(dg, fromval, "+FROM");
	if(toval)set_token_dg_feat(dg, toval, "+TO");

	return dg;
}

struct type	*type_for_unquoted_string(char	*us)
{
	char	*qs = malloc(strlen(us) + 3);
	sprintf(qs, "\"%s\"", us);
	struct type	*ty = add_string(qs);
	return ty;
}

int	do_reconstruct_tokens = 1;

static void	skip_space(char	**S)
{
	while(isspace(**S))(*S)++;
}

static char	*read_symbol(char	**S)
{
	skip_space(S);
	char	*tok = *S;
	while(!isspace(**S) && **S!='=' && **S!=']' && **S!='"' && **S!='[' && **S!=0)(*S)++;
	return tok;
}

static char	*read_string(char	**S)
{
	assert(**S=='"');
	char	*string = *S, *q = 1+string, *p = q;
	int	escape = 0;
	while(*q && !(*q=='"' && !escape))
	{
		if(escape)escape = 0;
		else if(*q=='\\')escape=1;
		if(!escape)*p++ = *q;
		q++;
	}
	if(*q++ != '"')return NULL;
	*p++ = '"';
	*S = q;
	return string;
}

// "string"
// type
// type [ ATTR value ]
// #tag
// #tag=value
int	parse_token_avm(struct hash	*taghash, struct dg	*d, char	**S)
{
	skip_space(S);
	if(**S == '#')
	{
		(*S)++;
		char	*tag = read_symbol(S);
		char	delim = **S;
		**S = 0;
		struct dg	*found = hash_find(taghash, tag);
		if(!found)
		{
			found = atomic_dg(top_type);
			hash_add(taghash, strdup(tag), found);
		}
		**S = delim;
		assert(!d->narcs);
		permanent_forward_dg(d, found);
		skip_space(S);
		if(**S != '=')
			return 0;
		(*S)++;
		skip_space(S);
		d = found;
	}

	char	*typename;
	if(**S == '"')
	{
		typename = read_string(S);
		if(!typename) { fprintf(stderr, "ERROR: bad string in token avm\n"); exit(-1); }
	}
	else typename = read_symbol(S);
	char	delim = **S;
	**S = 0;
	// must freeze_string() because if the type becomes a new temporary string, it will point at the argument...
	d = dereference_dg(d);
	d->type = d->xtype = lookup_type(freeze_string(typename));
	if(!d->type)
	{
		fprintf(stderr, "WARNING: token structure had nonexistant type '%s'\n", typename);
		return -1;
	}
	**S = delim;
	skip_space(S);

	//printf("type %s\n", d->xtype->name);
	//printf("lookahead: %s\n", *S);

	if(**S != '[')
		return 0;

	(*S)++;	// skip over the opening bracket
	skip_space(S);
	while(**S != ']')
	{
		char	*feat = read_symbol(S);
		char	delim = **S;
		**S = 0;
		int	fnum = lookup_fname(feat);
		//printf("feature %s\n", feat);
		**S = delim;
		skip_space(S);
		if(**S == ']' || **S == 0)
		{
			fprintf(stderr, "ERROR: token structure string ended before a whole number of feature-value pairs\n");
			exit(-1);
		}
		struct dg	*dest = atomic_dg(top_type);
		d = add_dg_hop(d, fnum, dest);
		if(0 != parse_token_avm(taghash, dest, S))
			return -1;
		skip_space(S);
	}

	assert(**S == ']');
	(*S)++;
	skip_space(S);

	return 0;
}

void	recons_unification_failure(struct reconstruction_error	*error)
{
	bzero(error,sizeof(*error));
	extern struct type	*ut1, *ut2;
	error->path = stringify_unify_failure_path();
	error->type1 = strdup(ut1?ut1->name:"UNKNOWN");
	error->type2 = strdup(ut2?ut2->name:"UNKNOWN");
}

struct dg	*reconstruct_tree_or_error(struct tree	*t, void	(*callback)(struct tree	*t, struct dg	*d), struct reconstruction_error	*error)
{
	if(!enable_token_avms)do_reconstruct_tokens = 0;
	if(t->ndaughters == 1 && t->daughters[0]->ndaughters == 0)
	{
		// this is a lexeme
		char	*lname = strdup(t->label);
		// if we are looking at a derivation printed by the LKB, then:
		//  1. it doesn't contain token structures, and
		//  2. therefore it needs a different way to record the CARG of generic lexemes.
		//     this is apparently accomplished by tweaking the name of the lexeme to look like:
		//     GENERIC_DATE_NE[1970]
		//    BUT... it's not always that simple.
		//      2a. usually it shows up in lowercase.
		//          generic_date_ne[1970]
		//      2b. but sometimes it shows up in uppercase with ||'s around it, like:
		//          |GENERIC_PL_NOUN_NE[1970's]|
		char	*uln = lname;
		if(*lname=='|')
		{
			uln++;
			assert(*uln && uln[strlen(uln)-1]=='|');
			uln[strlen(uln)-1] = 0;
			int i;
			for(i=0;uln[i];i++)
				uln[i] = tolower(uln[i]);
		}
		char	*lkb_carg = NULL;
		if(strchr(uln, '['))
		{
			lkb_carg = strchr(uln, '[');
			*lkb_carg++ = 0;
			assert(*lkb_carg && lkb_carg[strlen(lkb_carg)-1]==']');
			lkb_carg[strlen(lkb_carg)-1] = 0;
			lkb_carg = strdup(lkb_carg);
		}
		struct lexeme	*l = get_lex_by_name_hash(uln);
		if(!l)
		{
			if(reconstruction_noise_level)
				fprintf(stderr, "WARNING: unable to find lexeme '%s'\n", uln);
			if(error)
			{
				bzero(error,sizeof(*error));
				error->type = reconsNoSuchLexeme;
				error->node = t;
				error->comment = strdup(uln);
			}
			free(lname);
			return NULL;
		}
		free(lname);
		struct dg	*d = lexical_dg(l, NULL);
		if(do_reconstruct_tokens)
		{
			struct edge	le = {.lex = l, .dg = d};
			struct token	tl[l->stemlen];
			struct token	*tpl[l->stemlen];
			int	i;
			if(!(l->stemlen == t->daughters[0]->ntokens))
			{
				if(reconstruction_noise_level)
					fprintf(stderr, "WARNING: lexeme '%s' stem has %d tokens; tree has %d\n", l->word, l->stemlen, t->daughters[0]->ntokens);
				if(error)
				{
					bzero(error,sizeof(*error));
					error->type = reconsIncompatibleToken;
					error->node = t;
					error->comment = strdup("wrong number of tokens");
				}
				return NULL;
			}
			for(i=0;i<l->stemlen;i++)
			{
				struct tree	*tt = t->daughters[0];
				//printf("parse token: `%s'\n", tt->token);
				//printf(" [ %s %d - %d ]\n", tt->label, tt->cfrom, tt->cto);
				struct token	*tok = tl+i;
				bzero(tok,sizeof(*tok));
				char	*cpy = strdup(tt->tokens[i]), *c = cpy;
				struct hash	*taghash = hash_new("tags");
				tok->dg = atomic_dg(top_type);
				int parse_result = parse_token_avm(taghash, tok->dg, &c);
				if(parse_result != 0)
				{
					if(error)
					{
						bzero(error,sizeof(*error));
						error->type = reconsIncompatibleToken;
						error->node = t;
						error->comment = strdup("malformed token structure");
					}
					return NULL;
				}
				//printf("token dag is:\n"); print_dg(tok->dg); printf("\n");
				hash_free(taghash);
				free(cpy);
				tpl[i] = tl+i;
			}
			int status = install_tokens_in_le(&le, tpl);
			if(status != 0)
			{
				if(reconstruction_noise_level)
				{
					fprintf(stderr, "WARNING: failed to install tokens in lexeme '%s'\n", l->word);
					unify_backtrace();
				}
				if(error)
				{
					recons_unification_failure(error);
					error->node = t;
					error->type = reconsIncompatibleToken;
				}
				return NULL;
			}
			//printf("reconstructed lexeme DAG [l->stemlen = %d]:\n", l->stemlen);
			//print_dg(le.dg);
			//printf("\n");
		}
		else if(lkb_carg)
		{
			assert(l->stemlen == 1);
			struct edge	le = {.lex = l, .dg = d};
			struct token	tok;
			bzero(&tok, sizeof(tok));
			tok.string = lkb_carg;
			tok.cfrom = -1;
			tok.cto = -1;
			build_token_dag(&tok);
			struct type	*cargtype = type_for_unquoted_string(lkb_carg);
			set_token_dg_feat(tok.dg, cargtype, "+CARG");
			struct token	*tpl[1];
			tpl[0] = &tok;
			int status = install_tokens_in_le(&le, tpl);
			if(status != 0)
			{
				if(reconstruction_noise_level)
				{
					fprintf(stderr, "WARNING: failed to install tokens in lexeme '%s'\n", l->word);
					unify_backtrace();
				}
				if(error)
				{
					recons_unification_failure(error);
					error->type = reconsIncompatibleToken;
					error->node = t;
					error->comment = strdup("hallucinating a token from an LKB CARG specification");
				}
				return NULL;
			}
		}
		assert(d != NULL);
		if(callback)callback(t, d);
		return d;
	}
	else
	{
		// this is a rule/production
		struct rule	*r = get_rule_by_name(t->label);
		if(!r)
		{
			if(reconstruction_noise_level)
				fprintf(stderr, "WARNING: unable to find rule '%s'\n", t->label);
			if(error)
			{
				bzero(error,sizeof(*error));
				error->type = reconsNoSuchRule;
				error->node = t;
			}
			return NULL;
		}
		if(t->ndaughters != r->ndaughters)
		{
			if(reconstruction_noise_level)
				fprintf(stderr, "WARNING: rule %s has %d daughters but tree node has %d\n", r->name, r->ndaughters, t->ndaughters);
			if(error)
			{
				bzero(error,sizeof(*error));
				error->type = reconsNoSuchRule;
				error->node = t;
				error->comment = strdup("Grammar rule with that name had a different number of daughters.");
			}
			return NULL;
		}
		int	i;
		struct dg	*daughters[r->ndaughters];
		for(i=0;i<t->ndaughters;i++)
		{
			daughters[i] = reconstruct_tree_or_error(t->daughters[i], callback, error);
			if(!daughters[i])return NULL;
		}
		struct dg	*rule_dg = copy_dg(r->dg);
		for(i=0;i<t->ndaughters;i++)
		{
			int	result = unify_dg_tmp(daughters[i], rule_dg, i);
			nunifications++;
			if(result!=0)
			{
				forget_tmp();
				if(reconstruction_noise_level)
				{
					fprintf(stderr, "WARNING: reconstruction: unification failed for rule '%s'\n", t->label);
					unify_backtrace();
					if(reconstruction_noise_level > 1)print_tree(t,0);
				}
				if(error)
				{
					recons_unification_failure(error);
					error->type = reconsRuleGLB;
					error->node = t;
					error->daughter = i;
				}
				//printf("rule dg [ want arg %d ]\n", i);
				//print_dg(rule_dg);
				//printf("\n\n");
				//print_dg(daughter);
				/*output_lui_dg(rule_dg, "rule");
				output_lui_dg(daughter, "daughter");
				printf("\n\n");
				sleep(1000);*/
				return NULL;
			}
		}
		struct dg	*d = finalize_tmp(rule_dg, 1);
		if(!d)
		{
			if(reconstruction_noise_level)
			{
				fprintf(stderr, "WARNING: reconstruction: cycle check failed for rule '%s'\n", t->label);
				if(reconstruction_noise_level > 1)print_tree(t, 0);
			}
			if(error)
			{
				bzero(error,sizeof(*error));
				error->type = reconsRuleCycle;
				error->node = t;
			}
			return NULL;
		}
		if(callback)callback(t, d);
		return d;
	}
}

struct dg	*reconstruct_tree(struct tree	*t, void	(*callback)(struct tree	*t, struct dg	*d))
{
	return reconstruct_tree_or_error(t, callback, NULL);
}

// for some reason, certain parts of 'm' are heap (most are slab)
void	free_mrs_stuff(struct mrs	*m)
{
	int	i, j;
	for(i=0;i<m->vlist.nvars;i++)
	{
		struct mrs_var	*v = m->vlist.vars[i];
		free(v->type);
		free(v->name);
		for(j=0;j<v->nprops;j++)
			free(v->props[j].value);
	}
	for(i=0;i<m->nrels;i++)
		free(m->rels[i].pred);
}
