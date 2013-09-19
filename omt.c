/*
 * Copyright (c) 2013, BohuTANG <overred.shuttler at gmail dot com>
 * All rights reserved.
 * Code is licensed with GPL. See COPYING.GPL file.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>

#include "omt.h"

#define FACTOR (4)
#define CAPACITY (256)
#define IDXNULL (UINT32_MAX)

struct slice *_slice_clone(struct slice *s) {
	struct slice *clone;

	clone = calloc(1, sizeof(struct slice));
	assert(clone);

	clone->size = s->size;
	clone->data = calloc(s->size, sizeof(char));
	memcpy(clone->data, s->data, s->size);

	return clone;
}

int _keycmp(struct slice *sa, struct slice *sb)
{
	int cmp;
	uint32_t min;
	uint32_t as = sa->size;
	uint32_t bs = sb->size;

	min = as < bs ? as : bs;
	cmp = memcmp(sa->data, sb->data, min);
	if (cmp == 0)
		return (as - bs);

	return cmp;
}

static inline void _extend(struct omt_tree *tree, uint32_t n)
{
	uint32_t size, new_size;

	size = tree->capacity;
	new_size = (tree->free_idx + n);
	if (size > new_size) return;

	new_size = new_size * 2;
	tree->nodes = realloc(tree->nodes, new_size * sizeof(*tree->nodes));
	assert(tree->nodes);

	tree->capacity = new_size;
}

static inline int _subtree_isnull(struct omt_subtree *subtree)
{
	return subtree->idx == IDXNULL;
}

static inline void _subtree_setnull(struct omt_subtree *subtree)
{
	subtree->idx = IDXNULL;
}

void _subtree_setidx(struct omt_subtree *subtree, uint32_t idx)
{
	subtree->idx = idx;
}

uint32_t _weight(struct omt_tree *tree, struct omt_subtree *subtree)
{
	if (_subtree_isnull(subtree)) return 0;

	return tree->nodes[subtree->idx].weight;
}

struct omt_tree *omt_new() {
	struct omt_tree *tree;
	struct omt_node *nodes;

	tree = calloc(1, sizeof(*tree));
	if (!tree)
		goto ERR;

	nodes = calloc(CAPACITY, sizeof(*nodes));
	if (!nodes)
		goto ERR;

	tree->nodes = nodes;
	tree->capacity = CAPACITY;
	_subtree_setnull(&tree->root_subtree);

	return tree;

ERR:
	if (tree)
		free(tree);
	if (nodes)
		free(nodes);

	return NULL;
}

void _subtree_to_idxs(struct omt_tree *tree, struct omt_subtree *subtree, uint32_t *idxs)
{
	int w;
	struct omt_node *node;

	if (_subtree_isnull(subtree)) return;

	node = &tree->nodes[subtree->idx];
	w = _weight(tree, &node->left);
	_subtree_to_idxs(tree, &node->left, idxs);
	idxs[w] = node->nidx;
	_subtree_to_idxs(tree, &node->right, idxs + w + 1);
}

void _idxs_to_subtree(struct omt_tree *tree,
                      struct omt_subtree *subtree,
                      uint32_t *idxs,
                      int nums)
{
	if (nums == 0)
		_subtree_setnull(subtree);
	else {
		uint32_t nidx;
		uint32_t half;
		struct omt_node *node;

		half = nums / 2;
		nidx = idxs[half];

		_subtree_setidx(subtree, nidx);
		node = &tree->nodes[nidx];
		node->weight = nums;

		_idxs_to_subtree(tree,
		                 &node->left,
		                 idxs,
		                 half);

		_idxs_to_subtree(tree,
		                 &node->right,
		                 idxs + half + 1,
		                 nums - (half + 1));
	}
}

int _maybe_rebalance(struct omt_tree *tree, struct omt_subtree *subtree)
{
	int lw, rw;
	struct omt_node *node;

	if (_subtree_isnull(subtree)) return 0;
	if (_weight(tree, subtree) < FACTOR * 2) return 0;

	node = &tree->nodes[subtree->idx];
	lw = _weight(tree, &node->left);
	rw = _weight(tree, &node->right);

	if (lw > FACTOR * rw)
		return (lw - rw);

	if (rw > FACTOR * lw)
		return (rw - lw);

	return 0;
}

void _rebalance(struct omt_tree *tree, struct omt_subtree *subtree)
{
	uint32_t weight;
	uint32_t *idxs;

	weight = tree->nodes[subtree->idx].weight;
	idxs = calloc(weight, sizeof(uint32_t*));
	if (!idxs)
		return;

	_subtree_to_idxs(tree, subtree, idxs);
	_idxs_to_subtree(tree,
	                 subtree,
	                 idxs,
	                 weight);

	free(idxs);
	tree->status_rebalance_nums++;
}

void _insert_at(struct omt_tree *tree,
                struct omt_subtree *subtree,
                struct slice *val,
                int idx,
                struct omt_subtree **rebalance_subtree)
{
	struct omt_node *node;

	if (_subtree_isnull(subtree)) {
		int newidx;

		newidx = tree->free_idx;
		node = &tree->nodes[tree->free_idx++];
		node->weight = 1;
		node->nidx = newidx;
		node->value = _slice_clone(val);
		_subtree_setnull(&node->left);
		_subtree_setnull(&node->right);
		_subtree_setidx(subtree, newidx);
		return;
	} else {
		node = &tree->nodes[subtree->idx];

		node->weight++;
		if (*rebalance_subtree == NULL && _maybe_rebalance(tree, subtree) > 0) {
			*rebalance_subtree = subtree;
		}
		if (idx <= _weight(tree, &node->left)) {
			_insert_at(tree,
			           &node->left,
			           val,
			           idx,
			           rebalance_subtree);
		} else {
			int sub_idx = idx - _weight(tree, &node->left) - 1;

			_insert_at(tree,
			           &node->right,
			           val,
			           sub_idx,
			           rebalance_subtree);
		}
	}
}

int _find_order(struct omt_tree *tree,
                struct omt_subtree *subtree,
                struct slice *val,
                uint32_t *order)
{
	int r;
	int cmp;
	struct omt_node *node;

	if (_subtree_isnull(subtree)) {
		*order = 0U;
		return 1;
	}

	node = &tree->nodes[subtree->idx];
	cmp = _keycmp(node->value, val);
	if (cmp < 0) {
		r = _find_order(tree, &node->right, val, order);
		*order += (_weight(tree, &node->left) + 1);
		return r;
	} else if (cmp > 0) {
		return _find_order(tree, &node->left, val, order);
	} else {
		return -1;
	}

	return -1;
}

int omt_find_order(struct omt_tree *tree, struct slice *val, uint32_t *order)
{
	return _find_order(tree,
	                   &tree->root_subtree,
	                   val,
	                   order);
}

int omt_insert(struct omt_tree *tree, struct slice *val)
{
	int r = 0;
	uint32_t idx = 0;
	struct omt_subtree *re_subtree = NULL;

	_extend(tree, 1);
	r = _find_order(tree,
	                &tree->root_subtree,
	                val,
	                &idx);

	if (r == 1) {
		_insert_at(tree,
		           &tree->root_subtree,
		           val,
		           idx,
		           &re_subtree);

		if (re_subtree) {
			_rebalance(tree, re_subtree);
		}
	}

	return r;
}

void omt_free(struct omt_tree *tree)
{
	int i;

	for (i = 0; i < tree->free_idx; i++) {
		if (tree->nodes[i].value) {
			struct slice *s = tree->nodes[i].value;

			if (s->data)
				free(s->data);
			free(s);
		}
	}
	free(tree->nodes);
	free(tree);
}
